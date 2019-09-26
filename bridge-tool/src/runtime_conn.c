/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <termios.h>
#include <fcntl.h>
#include <stdint.h>

#include "coap_ext.h"
#include "bridge_tool_utils.h"
#include "shared_utils.h"
#include "attr_container.h"
#include "http.h"
#include "runtime_conn.h"
#include "config.h" 
#include "module_list.h"
#include "mqtt.h"
#include "http_mqtt_req.h"
#include "runtime_request.h"

#include "app_manager_export.h" /* for Module_WASM_App */
#include "host_link.h" /* for REQUEST_PACKET */

static int g_runtime_conn_fd; /* may be tcp or uart */

//static uint32_t g_timeout_ms = DEFAULT_TIMEOUT_MS;

#define SA struct sockaddr

unsigned char leading[2] = { 0x12, 0x34 };

extern unsigned char leading[2];

static int g_mid; // request message id, to check incoming responses (mids should match)
static op_type g_req_op_type=NONE; // are we waiting for an install request

/* lock for request/response data access */
static pthread_mutex_t mutex_request = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_pending;

bool tcp_init(const char *address, uint16_t port, int *fd)
{
    int sock;
    struct sockaddr_in servaddr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return false;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(address);
    servaddr.sin_port = htons(port);

    if (connect(sock, (SA*) &servaddr, sizeof(servaddr)) != 0)
        return false;

    *fd = sock;
    return true;
}

int parse_baudrate(int baud)
{
    switch (baud) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        case 230400:
            return B230400;
        case 460800:
            return B460800;
        case 500000:
            return B500000;
        case 576000:
            return B576000;
        case 921600:
            return B921600;
        case 1000000:
            return B1000000;
        case 1152000:
            return B1152000;
        case 1500000:
            return B1500000;
        case 2000000:
            return B2000000;
        case 2500000:
            return B2500000;
        case 3000000:
            return B3000000;
        case 3500000:
            return B3500000;
        case 4000000:
            return B4000000;
        default:
            return -1;
    }
}

bool uart_init(const char *device, int baudrate, int *fd)
{
    int uart_fd;
    struct termios uart_term;

    uart_fd = open(device, O_RDWR | O_NOCTTY);

    if (uart_fd <= 0)
        return false;

    memset(&uart_term, 0, sizeof(uart_term));
    uart_term.c_cflag = baudrate | CS8 | CLOCAL | CREAD;
    uart_term.c_iflag = IGNPAR;
    uart_term.c_oflag = 0;

    /* set noncanonical mode */
    uart_term.c_lflag = 0;
    uart_term.c_cc[VTIME] = 30;
    uart_term.c_cc[VMIN] = 1;
    tcflush(uart_fd, TCIFLUSH);

    if (tcsetattr(uart_fd, TCSANOW, &uart_term) != 0) {
        close(uart_fd);
        return false;
    }

    *fd = uart_fd;

    return true;
}

bool udp_send(const char *address, int port, const char *buf, int len)
{
    int sockfd;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
        return false;

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    sendto(sockfd, buf, len, MSG_CONFIRM, (const struct sockaddr *) &servaddr,
        sizeof(servaddr));

    close(sockfd);

    return true;
}

bool host_tool_send_data(const char *buf, unsigned int len)
{
    int cnt = 0;
    ssize_t ret;

    if (g_runtime_conn_fd == -1 || buf == NULL || len <= 0) {
        return false;
    }

    resend: ret = write(g_runtime_conn_fd, buf, len);

    if (ret == -1) {
        if (errno == ECONNRESET) {
            close(g_runtime_conn_fd);
        }

        // repeat sending if the outbuffer is full
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (++cnt > 10) {
                close(g_runtime_conn_fd);
                return false;
            }
            sleep(1);
            goto resend;
        }
    }

    return (ret == len);
}

#define SET_RECV_PHASE(ctx, new_phase) {ctx->phase = new_phase; ctx->size_in_phase = 0;}

/*
 * input:    1 byte from remote
 * output:   parse result
 * return:   -1 invalid sync byte
 *           1 byte added to buffer, waiting more for complete packet
 *           0 completed packet
 *           2 in receiving payload
 */
int on_imrt_link_byte_arrive(unsigned char ch, imrt_link_recv_context_t *ctx)
{
    if (ctx->phase == Phase_Non_Start) {
        if (ctx->message.payload) {
            free(ctx->message.payload);
            ctx->message.payload = NULL;
            ctx->message.payload_size = 0;
        }

        if (leading[0] == ch) {
            ctx->phase = Phase_Leading;
        } else {
            return -1;
        }
    } else if (ctx->phase == Phase_Leading) {
        if (leading[1] == ch) {
            SET_RECV_PHASE(ctx, Phase_Type);
        } else {
            ctx->phase = Phase_Non_Start;
            return -1;
        }
    } else if (ctx->phase == Phase_Type) {
        unsigned char *p = (unsigned char *) &ctx->message.message_type;
        p[ctx->size_in_phase++] = ch;

        if (ctx->size_in_phase == sizeof(ctx->message.message_type)) {
            ctx->message.message_type = ntohs(ctx->message.message_type);
            SET_RECV_PHASE(ctx, Phase_Size);
        }
    } else if (ctx->phase == Phase_Size) {
        unsigned char * p = (unsigned char *) &ctx->message.payload_size;
        p[ctx->size_in_phase++] = ch;

        if (ctx->size_in_phase == sizeof(ctx->message.payload_size)) {
            ctx->message.payload_size = ntohl(ctx->message.payload_size);
            SET_RECV_PHASE(ctx, Phase_Payload);

            if (ctx->message.payload) {
                free(ctx->message.payload);
                ctx->message.payload = NULL;
            }

            /* no payload */
            if (ctx->message.payload_size == 0) {
                SET_RECV_PHASE(ctx, Phase_Non_Start);
                return 0;
            }

            if (ctx->message.payload_size > 1024 * 1024) {
                SET_RECV_PHASE(ctx, Phase_Non_Start);
                return -1;
            }

            ctx->message.payload = (char *) malloc(ctx->message.payload_size);
            SET_RECV_PHASE(ctx, Phase_Payload);
        }
    } else if (ctx->phase == Phase_Payload) {
        ctx->message.payload[ctx->size_in_phase++] = ch;

        if (ctx->size_in_phase == ctx->message.payload_size) {
            SET_RECV_PHASE(ctx, Phase_Non_Start);
            return 0;
        }

        return 2;
    }

    return 1;
}

int runtime_conn_init(int *runtime_conn_fd)
{
    if (g_bt_config.rt_connection_mode == CONNECTION_MODE_TCP) {
        int fd;
        if (!tcp_init(g_bt_config.rt_address, g_bt_config.rt_port, &fd))
            return -1;
        g_runtime_conn_fd = *runtime_conn_fd = fd;
        return 0;
    } else if (g_bt_config.rt_connection_mode == CONNECTION_MODE_UART) {
        int fd;
        if (!uart_init(g_bt_config.rt_uart_dev, g_bt_config.rt_uart_baudrate, &fd))
            return -1;
        g_runtime_conn_fd = *runtime_conn_fd = fd;
        return 0;
    }

    return pthread_cond_init(&cond_pending, NULL);
}

void runtime_conn_close()
{
    close(g_runtime_conn_fd);
}

/*
 return value:
 < 0: not complete message
 REPLY_TYPE_EVENT: event(request)
 REPLY_TYPE_RESPONSE: response
 */
int process_rcvd_data(const char *buf, int len,
        imrt_link_recv_context_t *ctx)
{
    int result = -1;
    const char *pos = buf;

#if DEBUG
    int i = 0;
    for (; i < len; i++) {
        printf(" 0x%02x", buf[i]);
    }
    printf("\n");
#endif

    while (len-- > 0) {
        result = on_imrt_link_byte_arrive((unsigned char) *pos++, ctx);
        switch (result) {
        case 0: {
            imrt_link_message_t *message = &ctx->message;
            if (message->message_type == RESPONSE_PACKET)
                return REPLY_TYPE_RESPONSE;
            if (message->message_type == REQUEST_PACKET)
                return REPLY_TYPE_EVENT;
            break;
        }
        default:
            break;
        }
    }
    return -1;
}

response_t *parse_response_from_imrtlink(imrt_link_message_t *message, response_t *response)
{
    if (!unpack_response(message->payload, message->payload_size, response))
        return NULL;

    return response;
}

request_t *parse_event_from_imrtlink(imrt_link_message_t *message, request_t *request)
{
    if (!unpack_request(message->payload, message->payload_size, request))
        return NULL;

    return request;
}

int install_response_get_module_id_and_name(response_t *obj, int *mod_id, char *mod_name, int module_name_len)
{
    attr_container_t *payload = obj->payload;
    int foramt = obj->fmt;
    int payload_len = obj->payload_len;
    char *data;
    cJSON *req_json = NULL;

    if (obj == NULL || mod_id == NULL) return -1;

    if (foramt != FMT_ATTR_CONTAINER || payload == NULL || payload_len <= 0)
        return -1;

    data = attr_container_get_as_string(payload, "data");

    req_json = cJSON_Parse(data);

    if (req_json == NULL) {
        printf("Error parsing response json!\n");
        return -1;
    }
    cJSON *json_module_id = cJSON_GetObjectItemCaseSensitive(req_json, "id");
    if (cJSON_IsString(json_module_id) && (json_module_id->valuestring != NULL)) {
        *mod_id = atoi(json_module_id->valuestring);
    } else {
        cJSON_Delete(req_json);
        return -1;
    }

    if (mod_name != NULL && module_name_len > 0) {
        cJSON *json_module_name = cJSON_GetObjectItemCaseSensitive(req_json, "name");
        if (cJSON_IsString(json_module_name) && (json_module_name->valuestring != NULL)) {
            strncpy(mod_name, json_module_name->valuestring, module_name_len);
            mod_name[module_name_len]='\0';
        } else {
            cJSON_Delete(req_json);
            return -1;
        }
    }

    cJSON_Delete(req_json);
    return 0;
}


void output(const char *header, attr_container_t *payload, int foramt,
        int payload_len)
{
    cJSON *json = NULL;
    char *json_str = NULL;

    /* output the header */
    printf("%s", header);

    if (foramt != FMT_ATTR_CONTAINER || payload == NULL || payload_len <= 0)
        return;

    if ((json = attr2json(payload)) == NULL)
        return;

    if ((json_str = cJSON_Print(json)) == NULL) {
        cJSON_Delete(json);
        return;
    }

    /* output the payload as json format */
    printf("%s", json_str);

    free(json_str);
    cJSON_Delete(json);
}

void output_response(response_t *obj)
{
    char header[32] = { 0 };
    snprintf(header, sizeof(header), "\nresponse status %d\n", obj->status);
    output(header, obj->payload, obj->fmt, obj->payload_len);
}

/**
 * Return the mid and and op_type of a pending (to which we have not received a response) request
 * 
 */
void get_pending_request_info(int *mid, int*op_type)
{
    *mid = g_mid;
    *op_type = g_req_op_type;
}

/**
 * Called by runtime_request when a request is sent, indicating that we are waiting for 
 * the respective response
 * 
 */
void rt_conn_request_sent(op_type request_type, int mid) {
    
    pthread_mutex_lock(&mutex_request);

    g_mid = mid; 
    g_req_op_type = request_type;

    pthread_mutex_unlock(&mutex_request);
}

/**
 * Indicate that we have no pending request
 * 
 */
void rt_conn_response_received() {
    // ready to receive another request
    pthread_mutex_lock(&mutex_request);

    g_req_op_type=NONE;
    pthread_cond_signal(&cond_pending);

    pthread_mutex_unlock(&mutex_request);
}

/**
 * Waits for a response from the runtime 
 * 
 */
void rt_conn_wait_pending_response() 
{
    pthread_mutex_lock(&mutex_request);
 
    while (g_req_op_type!=NONE) 
        pthread_cond_wait(&cond_pending, &mutex_request);

    pthread_mutex_unlock(&mutex_request);
}

