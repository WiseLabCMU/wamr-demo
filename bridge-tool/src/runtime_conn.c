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

#include "bridge_tool_utils.h"
#include "shared_utils.h"
#include "attr_container.h"
#include "http.h"
#include "runtime_conn.h"

#include "app_manager_export.h" /* for Module_WASM_App */
#include "host_link.h" /* for REQUEST_PACKET */

static int g_runtime_conn_fd; /* may be tcp or uart */

static char *g_runtime_addr = "127.0.0.1";
static int g_connection_mode = CONNECTION_MODE_TCP;
static int g_runtime_port = 8888;
static char *g_uart_dev = "/dev/ttyS2";
static int g_baudrate = B115200;

static uint32_t g_timeout_ms = DEFAULT_TIMEOUT_MS;

#define SA struct sockaddr

unsigned char leading[2] = { 0x12, 0x34 };

extern unsigned char leading[2];
int g_mid;

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
    if (g_connection_mode == CONNECTION_MODE_TCP) {
        int fd;
        if (!tcp_init(g_runtime_addr, g_runtime_port, &fd))
            return -1;
        g_runtime_conn_fd = *runtime_conn_fd = fd;
        return 0;
    } else if (g_connection_mode == CONNECTION_MODE_UART) {
        int fd;
        if (!uart_init(g_uart_dev, g_baudrate, &fd))
            return -1;
        g_runtime_conn_fd = *runtime_conn_fd = fd;
        return 0;
    }

    return -1;
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
 * Reads response from runtime. If connection to rest client is given, forwards the response to it
 * 
 * @param http_mg_conn the connection to rest client; if !=NULL outputs to the connection; if =NULL outputs to console
 * @return returns -1 on error, -2 on timeout; >=0 status code
 */
int get_request_response(struct mg_connection *http_mg_conn)
{
    int result, n;
    imrt_link_recv_context_t recv_ctx = { 0 };
    uint32_t last_check, total_elpased_ms = 0;
    fd_set readfds;
    char buffer[BUF_SIZE] = { 0 };

    bh_get_elpased_ms(&last_check);

  struct timeval tv;
  while (1) {
        total_elpased_ms += bh_get_elpased_ms(&last_check);

        if (total_elpased_ms >= g_timeout_ms) {
            if (http_mg_conn != NULL)
                mg_printf(http_mg_conn, "%s", "HTTP/1.1 504 Gateway Timeout\r\n\r\n");
            return TIMEOUT_EXIT_CODE;
        }

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        // initialize the set of active sockets (readfds is changed at each select() call)
        FD_ZERO (&readfds);
        FD_SET (g_runtime_conn_fd, &readfds);

        result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

        if (result < 0) {
            if (errno != EINTR) {
                if (http_mg_conn != NULL)
                    mg_printf(http_mg_conn, "%s", "HTTP/1.1 500 Internal Server Error\r\n\r\n");
                else printf("Internal Server Error\n");
                return -1;
            }
        } else if (result == 0) { /* select timeout */
        } else if (result > 0) {
            if (FD_ISSET(g_runtime_conn_fd, &readfds)) {
               int reply_type = -1; 
                n = read(g_runtime_conn_fd, buffer, BUF_SIZE);
                if (n <= 0) {
                    if (http_mg_conn != NULL)
                        mg_printf(http_mg_conn, "%s", "HTTP/1.1 500 Internal Server Error\r\n\r\n");
                    else printf("Internal Server Error\n");
                    return -1;
                }

                reply_type = process_rcvd_data((char *) buffer, n, &recv_ctx);

                if (reply_type == REPLY_TYPE_RESPONSE) {
                    response_t response[1] = { 0 };

                    parse_response_from_imrtlink(&recv_ctx.message, response);

                    if (response->mid != g_mid) {
                        /* ignore invalid response */
                        mg_printf(http_mg_conn, "%s", "HTTP/1.1 412 Precondition Failed\r\n\r\n");
                        return -1;
                    }
                    
                    if (http_mg_conn != NULL)
                        http_output_runtime_response(http_mg_conn, response);
                    else output_response(response);
                    return response->status;
                } else {
                    if (http_mg_conn != NULL)
                        mg_printf(http_mg_conn, "%s", "HTTP/1.1 500 Internal Server Error\r\n\r\n"); // no response ?
                    else printf("Internal Server Error\n");
                    return -1;
                }
            }
        }
    } /* end of while(1) */
    return -1;
}