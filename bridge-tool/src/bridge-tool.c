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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>

#include "bridge_tool_utils.h"
#include "shared_utils.h"
#include "attr_container.h"
#include "coap_ext.h"
#include "cJSON.h"
#include "app_manager_export.h" /* for Module_WASM_App */
#include "host_link.h" /* for REQUEST_PACKET */
#include "transport.h"

#include "mongoose.h"

#define BUF_SIZE 1024
#define TIMEOUT_EXIT_CODE 2
#define URL_MAX_LEN 256
#define DEFAULT_TIMEOUT_MS 5000
#define DEFAULT_ALIVE_TIME_MS 0

#define CONNECTION_MODE_TCP 1
#define CONNECTION_MODE_UART 2

typedef enum {
    INSTALL, UNINSTALL, QUERY, REQUEST, REGISTER, UNREGISTER
} op_type;

typedef struct {
    const char *file;
    const char *name;
    const char *module_type;
    int heap_size;
    /* max timers number */
    int timers;
    int watchdog_interval;
} inst_info;

typedef struct {
    const char *name;
    const char *module_type;
} uninst_info;

typedef struct {
    const char *name;
} query_info;

typedef struct {
    const char *url;
    int action;
    const char *json_payload_file;
} req_info;

typedef struct {
    const char *urls;
} reg_info;

typedef struct {
    const char *urls;
} unreg_info;

typedef union operation_info {
    inst_info inst;
    uninst_info uinst;
    query_info query;
    req_info req;
    reg_info reg;
    unreg_info unreg;
} operation_info;

typedef struct {
    op_type type;
    operation_info info;
} operation;

typedef enum REPLY_PACKET_TYPE {
    REPLY_TYPE_EVENT = 0, REPLY_TYPE_RESPONSE = 1
} REPLY_PACKET_TYPE;

/* Package Type */
typedef enum {
    Wasm_Module_Bytecode = 0, Wasm_Module_AoT, Package_Type_Unknown = 0xFFFF
} PackageType;

static uint32_t g_timeout_ms = DEFAULT_TIMEOUT_MS;
static uint32_t g_alive_time_ms = DEFAULT_ALIVE_TIME_MS;
static char *g_redirect_file_name = NULL;
static int g_redirect_udp_port = -1;
static int g_conn_fd; /* may be tcp or uart */
static char *g_server_addr = "127.0.0.1";
static int g_server_port = 8888;
static char *g_uart_dev = "/dev/ttyS2";
static int g_baudrate = B115200;
static int g_connection_mode = CONNECTION_MODE_TCP;

extern int g_mid;
extern unsigned char leading[2];

static int process_rcvd_data(const char *buf, int len, imrt_link_recv_context_t *ctx);
static response_t * parse_response_from_imrtlink(imrt_link_message_t *message, response_t *response);
static request_t * parse_event_from_imrtlink(imrt_link_message_t *message, request_t *request);
static void http_output(attr_container_t *payload, int foramt, int payload_len, struct mg_connection *nc);
static void http_output_response(response_t *obj, struct mg_connection *nc);

/* -1 fail, 0 success */
static int send_request(request_t *request, bool is_install_wasm_bytecode_app)
{
    char *req_p;
    int req_size, req_size_n, ret = -1;
    uint16_t msg_type = REQUEST_PACKET;

    if (is_install_wasm_bytecode_app)
        msg_type = INSTALL_WASM_BYTECODE_APP;

    if ((req_p = pack_request(request, &req_size)) == NULL)
        return -1;

    /* leanding bytes */
    if (!host_tool_send_data(g_conn_fd, leading, sizeof(leading)))
        goto ret;

    /* message type */
    msg_type = htons(msg_type);
    if (!host_tool_send_data(g_conn_fd, (char *) &msg_type, sizeof(msg_type)))
        goto ret;

    /* payload length */
    req_size_n = htonl(req_size);
    if (!host_tool_send_data(g_conn_fd, (char *) &req_size_n,
            sizeof(req_size_n)))
        goto ret;

    /* payload */
    if (!host_tool_send_data(g_conn_fd, req_p, req_size))
        goto ret;

    ret = 0;

    ret: free_req_resp_packet(req_p);

    return ret;
}

static PackageType get_package_type(const char *buf, int size)
{
    if (buf && size > 4) {
        if (buf[0] == '\0' && buf[1] == 'a' && buf[2] == 's' && buf[3] == 'm')
            return Wasm_Module_Bytecode;
        if (buf[0] == '\0' && buf[1] == 'a' && buf[2] == 'o' && buf[3] == 't')
            return Wasm_Module_AoT;
    }
    return Package_Type_Unknown;
}

#define url_remain_space (sizeof(url) - strlen(url))

static int http_get_request_response(struct mg_connection *nc)
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
            mg_printf(nc, "%s", "HTTP/1.1 504 Gateway Timeout\r\n\r\n");
            return TIMEOUT_EXIT_CODE;
        }

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        // initialize the set of active sockets (readfds is changed at each select() call)
        FD_ZERO (&readfds);
        FD_SET (g_conn_fd, &readfds);

        result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

        if (result < 0) {
            if (errno != EINTR) {
                mg_printf(nc, "%s", "HTTP/1.1 500 Internal Server Error\r\n\r\n");
                return -1;
            }
        } else if (result == 0) { /* select timeout */
        } else if (result > 0) {
            if (FD_ISSET(g_conn_fd, &readfds)) {
               int reply_type = -1; 
              n = read(g_conn_fd, buffer, BUF_SIZE);
                if (n <= 0) {
                    g_conn_fd = -1;
                    continue;
                }

                reply_type = process_rcvd_data((char *) buffer, n, &recv_ctx);

                if (reply_type == REPLY_TYPE_RESPONSE) {
                    response_t response[1] = { 0 };

                    parse_response_from_imrtlink(&recv_ctx.message, response);

                    if (response->mid != g_mid) {
                        /* ignore invalid response */
                        continue;
                    }
                    
                    http_output_response(response, nc);
                    return response->status;
                } else {
                    mg_printf(nc, "%s", "HTTP/1.1 500 Internal Server Error\r\n\r\n"); // no response ?
                    return -1;
                }
            }
        }
    } /* end of while(1) */
    return -1;
}

/*return:
 0: success
 others: fail*/
static int install(inst_info *info)
{
    request_t request[1] = { 0 };
    char *app_file_buf;
    char url[URL_MAX_LEN] = { 0 };
    int ret = -1, app_size;
    bool is_wasm_bytecode_app;

    snprintf(url, sizeof(url) - 1, "/applet?name=%s", info->name);

    if (info->module_type != NULL && url_remain_space > 0)
        snprintf(url + strlen(url), url_remain_space, "&type=%s",
                info->module_type);

    if (info->heap_size > 0 && url_remain_space > 0)
        snprintf(url + strlen(url), url_remain_space, "&heap=%d",
                info->heap_size);

    if (info->timers > 0 && url_remain_space > 0)
        snprintf(url + strlen(url), url_remain_space, "&timers=%d",
                info->timers);

    if (info->watchdog_interval > 0 && url_remain_space > 0)
        snprintf(url + strlen(url), url_remain_space, "&wd=%d",
                info->watchdog_interval);

    /*TODO: permissions to access JLF resource: AUDIO LOCATION SENSOR VISION platform.SERVICE */

    if ((app_file_buf = read_file_to_buffer(info->file, &app_size)) == NULL) {
        printf("Error reading file! %s\n", info->file);
        return -1;
    }

    init_request(request, url, COAP_PUT,
    FMT_APP_RAW_BINARY, app_file_buf, app_size);
    request->mid = g_mid = gen_random_id();

    if ((info->module_type == NULL || strcmp(info->module_type, "wasm") == 0)
            && get_package_type(app_file_buf, app_size) == Wasm_Module_Bytecode)
        is_wasm_bytecode_app = true;
    else
        is_wasm_bytecode_app = false;

    ret = send_request(request, is_wasm_bytecode_app);

    free(app_file_buf);

    return ret;
}

static int uninstall(uninst_info *info)
{
    request_t request[1] = { 0 };
    char url[URL_MAX_LEN] = { 0 };

    snprintf(url, sizeof(url) - 1, "/applet?name=%s", info->name);

    if (info->module_type != NULL && url_remain_space > 0)
        snprintf(url + strlen(url), url_remain_space, "&type=%s",
                info->module_type);

    init_request(request, url, COAP_DELETE,
    FMT_ATTR_CONTAINER,
    NULL, 0);
    request->mid = g_mid = gen_random_id();

    return send_request(request, false);
}

static int query(query_info *info)
{
    request_t request[1] = { 0 };
    int ret = -1;
    char url[URL_MAX_LEN] = { 0 };

    if (info->name != NULL)
        snprintf(url, sizeof(url) - 1, "/applet?name=%s", info->name);
    else
        snprintf(url, sizeof(url) - 1, "/applet");

    init_request(request, url, COAP_GET,
    FMT_ATTR_CONTAINER,
    NULL, 0);
    request->mid = g_mid = gen_random_id();

    ret = send_request(request, false);

    return ret;
}

static int request(req_info *info)
{
    request_t request[1] = { 0 };
    attr_container_t *payload = NULL;
    int ret = -1, payload_len = 0;

    if (info->json_payload_file != NULL) {
        char *payload_file;
        cJSON *json;
        int payload_file_size;

        if ((payload_file = read_file_to_buffer(info->json_payload_file,
                &payload_file_size)) == NULL)
            return -1;

        if (NULL == (json = cJSON_Parse(payload_file))) {
            free(payload_file);
            goto fail;
        }

        if (NULL == (payload = json2attr(json))) {
            cJSON_Delete(json);
            free(payload_file);
            goto fail;
        }
        payload_len = attr_container_get_serialize_length(payload);

        cJSON_Delete(json);
        free(payload_file);
    }

    init_request(request, (char *)info->url, info->action,
    FMT_ATTR_CONTAINER, payload, payload_len);
    request->mid = gen_random_id();

    ret = send_request(request, false);

    if (info->json_payload_file != NULL && payload != NULL)
        attr_container_destroy(payload);

    fail: return ret;
}

/*
 TODO: currently only support 1 url.
 how to handle multiple responses and set process's exit code?
 */
static int subscribe(reg_info *info)
{
    request_t request[1] = { 0 };
    int ret = -1;
#if 0
    char *p;

    p = strtok(info->urls, ",");
    while(p != NULL) {
        char url[URL_MAX_LEN] = {0};
        sprintf(url, "%s%s", "/event/", p);
        init_request(request,
                url,
                COAP_PUT,
                FMT_ATTR_CONTAINER,
                NULL,
                0);
        request->mid = gen_random_id();
        ret = send_request(request, false);
        p = strtok (NULL, ",");
    }
#else
    char url[URL_MAX_LEN] = { 0 };
    char *prefix = info->urls[0] == '/' ? "/event" : "/event/";
    sprintf(url, "%s%s", prefix, info->urls);
    init_request(request, url, COAP_PUT,
    FMT_ATTR_CONTAINER,
    NULL, 0);
    request->mid = gen_random_id();
    ret = send_request(request, false);
#endif
    return ret;
}

static int unsubscribe(unreg_info *info)
{
    request_t request[1] = { 0 };
    int ret = -1;
#if 0
    char *p;

    p = strtok(info->urls, ",");
    while(p != NULL) {
        char url[URL_MAX_LEN] = {0};
        sprintf(url, "%s%s", "/event/", p);
        init_request(request,
                url,
                COAP_DELETE,
                FMT_ATTR_CONTAINER,
                NULL,
                0);
        request->mid = gen_random_id();
        ret = send_request(request, false);
        p = strtok (NULL, ",");
    }
#else
    char url[URL_MAX_LEN] = { 0 };
    sprintf(url, "%s%s", "/event/", info->urls);
    init_request(request, url, COAP_DELETE,
    FMT_ATTR_CONTAINER,
    NULL, 0);
    request->mid = gen_random_id();
    ret = send_request(request, false);
#endif
    return ret;
}

static int init_runtime_conn()
{
    if (g_connection_mode == CONNECTION_MODE_TCP) {
        int fd;
        if (!tcp_init(g_server_addr, g_server_port, &fd))
            return -1;
        g_conn_fd = fd;
        return 0;
    } else if (g_connection_mode == CONNECTION_MODE_UART) {
        int fd;
        if (!uart_init(g_uart_dev, g_baudrate, &fd))
            return -1;
        g_conn_fd = fd;
        return 0;
    }

    return -1;
}

static void deinit_runtime_conn()
{
    close(g_conn_fd);
}

/*
 return value:
 < 0: not complete message
 REPLY_TYPE_EVENT: event(request)
 REPLY_TYPE_RESPONSE: response
 */
static int process_rcvd_data(const char *buf, int len,
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

static response_t *
parse_response_from_imrtlink(imrt_link_message_t *message, response_t *response)
{
    if (!unpack_response(message->payload, message->payload_size, response))
        return NULL;

    return response;
}

static request_t *
parse_event_from_imrtlink(imrt_link_message_t *message, request_t *request)
{
    if (!unpack_request(message->payload, message->payload_size, request))
        return NULL;

    return request;
}

static void output(const char *header, attr_container_t *payload, int foramt,
        int payload_len)
{
    cJSON *json = NULL;
    char *json_str = NULL;

    /* output the header */
    printf("%s", header);
    if (g_redirect_file_name != NULL)
        wirte_buffer_to_file(g_redirect_file_name, header, strlen(header));
    if (g_redirect_udp_port > 0 && g_redirect_udp_port < 65535)
        udp_send("127.0.0.1", g_redirect_udp_port, header, strlen(header));

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
    if (g_redirect_file_name != NULL)
        wirte_buffer_to_file(g_redirect_file_name, json_str, strlen(json_str));
    if (g_redirect_udp_port > 0 && g_redirect_udp_port < 65535)
        udp_send("127.0.0.1", g_redirect_udp_port, json_str, strlen(json_str));

    free(json_str);
    cJSON_Delete(json);
}

static void http_output(attr_container_t *payload, int foramt,
        int payload_len, struct mg_connection *nc)
{
    cJSON *json = NULL;
    char *json_str = NULL;

    if (foramt != FMT_ATTR_CONTAINER || payload == NULL || payload_len <= 0)
        return;

    if ((json = attr2json(payload)) == NULL)
        return;

    if ((json_str = cJSON_Print(json)) == NULL) {
        cJSON_Delete(json);
        return;
    }

    mg_printf_http_chunk(nc, "%s", json_str);

    free(json_str);
    cJSON_Delete(json);
}

static void output_response(response_t *obj)
{
    char header[32] = { 0 };
    snprintf(header, sizeof(header), "\nresponse status %d\n", obj->status);
    output(header, obj->payload, obj->fmt, obj->payload_len);
}

static void http_output_response(response_t *obj, struct mg_connection *nc)
{
    char status[100];
    switch (obj->status) {
        case CREATED_2_01: snprintf(status, 100, "201 Created"); 
            break; 
        case INTERNAL_SERVER_ERROR_5_00: snprintf(status, 100, "500 Internal Server Error"); 
            break;             
    }
    
    /* Send headers */
    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
    http_output(obj->payload, obj->fmt, obj->payload_len, nc);
    mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
}


static void output_event(request_t *obj)
{
    char header[256] = { 0 };
    snprintf(header, sizeof(header), "\nreceived an event %s (%d)\n", obj->url, obj->action);
    output(header, obj->payload, obj->fmt, obj->payload_len);
}

#define FILE_SUB "../../out/wasm-apps/event_subscriber.wasm"
#define MODULE_NAME_SUB "sub"

static const char *s_http_port = "8000";
static const char *s_doc_root = ".";
static struct mg_serve_http_opts s_http_server_opts;

static void http_ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;
  double result;

  switch (ev) {
    case MG_EV_HTTP_REQUEST:
      if (mg_vcmp(&hm->uri, "/api/v1/test") == 0) {
        operation op;
        op.info.inst.file = FILE_SUB;
        op.info.inst.name = MODULE_NAME_SUB;

        install((inst_info *) &op.info.inst);
        http_get_request_response(nc);

      } else if (mg_vcmp(&hm->uri, "/api/v1/query") == 0) {
        /* Send headers */
        mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

        /*  send response as a JSON object */
        mg_printf_http_chunk(nc, "{ \"result\": \"ok\" }");
        mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
      } else {
        mg_serve_http(nc, hm, s_http_server_opts); /* Serve static content */
      }
      break;
    default:
      break;
  }
}


int main(int argc, char *argv[])
{
    int ret;
    imrt_link_recv_context_t recv_ctx = { 0 };
    char buffer[BUF_SIZE] = { 0 };
    uint32_t last_check, total_elpased_ms = 0;
    bool is_responsed = true;
    fd_set readfds;
    int result = 0;
    struct timeval tv;


    //if (!parse_args(argc, argv, &op))
    //    return -1;

    //TODO: reconnect 3 times
    if (init_runtime_conn() != 0)
        return -1;

    //operation op;
    //memset(&op, 0, sizeof(op));
    //op.info.inst.file = FILE_SUB;
    //op.info.inst.name = MODULE_NAME_SUB;
    //install((inst_info *) &op.info.inst);

    struct mg_mgr mgr;
    struct mg_connection *http_c;

    s_http_server_opts.document_root = s_doc_root;
    s_http_server_opts.enable_directory_listing = "yes";

    mg_mgr_init(&mgr, NULL);
    http_c = mg_bind(&mgr, s_http_port, http_ev_handler);
    if (http_c == NULL) {
        fprintf(stderr, "Error starting server on port %s:\n", s_http_port);
        exit(1);
    }
    mg_set_protocol_http_websocket(http_c);

    printf("Starting RESTful server on port %s, serving %s\n", s_http_port,
         s_http_server_opts.document_root);    

    bh_get_elpased_ms(&last_check);

    /* Initialize the set of active sockets. */
    //FD_ZERO (&readfds);
    //FD_SET (g_conn_fd, &readfds);
    //FD_SET (http_c->sock, &readfds);
/* 
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);
    if (result) { printf("received!\n"); mg_mgr_poll(&mgr, 1000); }
    sleep(5);
    mg_mgr_free(&mgr);

    exit(1);
*/
    while (1) {
        //struct timeval tv;

        total_elpased_ms += bh_get_elpased_ms(&last_check);

        if (!is_responsed) {
            if (total_elpased_ms >= g_timeout_ms) {
                output("operation timeout\n", NULL, 0, 0);
                ret = TIMEOUT_EXIT_CODE;
                //goto ret;
                is_responsed = true;
            }
        } else {
            if (total_elpased_ms >= g_alive_time_ms) {
                /*ret = 0;*/
                //goto ret;
            }
        }

        if (g_conn_fd == -1) {
            if (init_runtime_conn() != 0) {
                sleep(1);
                continue;
            }
        }

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        // initialize the set of active sockets (readfds is changed at each select() call)
        FD_ZERO (&readfds);
        FD_SET (g_conn_fd, &readfds);
        FD_SET (http_c->sock, &readfds);

        result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

        if (result < 0) {
            if (errno != EINTR) {
                printf("Error in select, errno: 0x%x\n", errno);
                ret = -1;
                goto ret;
            }
        } else if (result == 0) { /* select timeout */
        } else if (result > 0) {
            int n;
            if (FD_ISSET(http_c->sock, &readfds)) {
                while (mg_mgr_poll(&mgr, 1000) > 0); // drive mg smgr state machine
            }
            if (FD_ISSET(g_conn_fd, &readfds)) {
                int reply_type = -1;

                n = read(g_conn_fd, buffer, BUF_SIZE);
                if (n <= 0) {
                    g_conn_fd = -1;
                    continue;
                }

                reply_type = process_rcvd_data((char *) buffer, n, &recv_ctx);

                if (reply_type == REPLY_TYPE_RESPONSE) {
                    response_t response[1] = { 0 };

                    parse_response_from_imrtlink(&recv_ctx.message, response);

                    if (response->mid != g_mid) {
                        /* ignore invalid response */
                        continue;
                    }

                    is_responsed = true;
                    ret = response->status;
                    output_response(response);

                    //if (op.type == REGISTER || op.type == UNREGISTER) {
                        /* alive time start */
                    //    total_elpased_ms = 0;
                     //   bh_get_elpased_ms(&last_check);
                    //}
                } else if (reply_type == REPLY_TYPE_EVENT) {
                    request_t event[1] = { 0 };

                    parse_event_from_imrtlink(&recv_ctx.message, event);

                    //if (op.type == REGISTER || op.type == UNREGISTER) {
                        //printf("op=%d\n", op.type);
                        output_event(event);
                    //}
                } else {
                    printf("received  type:%d\n", reply_type);
                }
            }
        }
    } /* end of while(1) */

    ret: if (recv_ctx.message.payload != NULL)
        free(recv_ctx.message.payload);

    deinit_runtime_conn();

    return ret;
}
