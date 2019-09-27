 /** @file bridge-tool.c
 *  @brief Implementation of a pubsub and http bridge 
 * 
 *  Bridges between a WAMR runtime and MQTT pubsub. Also bridges requests from an http rest interface for management of modules.
 *  This code is based on WAMR host tool; original copyright bellow.
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */

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
#include <termios.h>
#include <unistd.h>

#include "coap_ext.h"
#include "http_mqtt_req.h"
#include "cJSON.h"
#include "runtime_conn.h"
#include "runtime_request.h"
#include "module_list.h"
#include "http.h"
#include "mqtt.h"
#include "config.h"

#define RECONNECT_ATTEMPTS 3

int main(int argc, char *argv[])
{
    int ret, result, runtime_conn_fd, n, reply_type = -1, rconn=0;
    int mid, op_type;
    char buffer[BUF_SIZE] = { 0 };
    fd_set readfds;
    struct timeval tv;
    struct mg_connection *http_mg_conn=NULL;    
    struct mg_connection *mqtt_mg_conn=NULL;
    uint32_t last_check, total_elapsed_ms = 0;
    imrt_link_recv_context_t recv_ctx = { 0 };

    if (read_config() != 0) return -1;

    bh_get_elpased_ms(&last_check);

    if (runtime_conn_init(&runtime_conn_fd) != 0) return -1;

    if (http_init(&http_mg_conn) != 0 ) return -1;

    if (mqtt_init(&mqtt_mg_conn) != 0 ) return -1;
    
    while (1) {
        total_elapsed_ms += bh_get_elpased_ms(&last_check);

        if (total_elapsed_ms >= g_bt_config.mqtt_keepalive_ms) {
            mg_mqtt_ping(mqtt_mg_conn);
            mqtt_pool_requests(); // process mqtt requests
            total_elapsed_ms = 0;
        }
        
        if (rconn > g_bt_config.rt_reconnect_attempts) {
            printf("Error: too many reconnection attempts.\n");
            exit(-1);
        }

        if (runtime_conn_fd == -1) {
            rconn++;
            if (runtime_conn_init(&runtime_conn_fd) != 0) {
                sleep(1);
                continue;
            } else rconn = 0;
        }

        if (mqtt_mg_conn->sock == -1) {
            if (mqtt_init(&mqtt_mg_conn) != 0 ) {
                sleep(1);
                continue;
            }
        }

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        // initialize the set of active sockets (readfds is changed at each select() call)
        FD_ZERO (&readfds);
        FD_SET (runtime_conn_fd, &readfds);
        FD_SET (mqtt_mg_conn->sock, &readfds);

        result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

        if (result < 0) {
            if (errno != EINTR) {
                printf("Error in select, errno: 0x%x\n", errno);
                continue;
            }
        } else if (result == 0) { /* select timeout */
        } else if (result > 0) {
            if (FD_ISSET(mqtt_mg_conn->sock, &readfds)) {
                mqtt_pool_requests(); // process mqtt requests
            } else if (FD_ISSET(runtime_conn_fd, &readfds)) {
                n = read(runtime_conn_fd, buffer, BUF_SIZE);
                if (n <= 0) {
                    runtime_conn_fd = -1;
                    printf("error!\n");
                    continue;
                }

                reply_type = process_rcvd_data((char *) buffer, n, &recv_ctx);

                if (reply_type == REPLY_TYPE_RESPONSE) {
                    response_t response[1] = { 0 };
                    int mod_id;
                    char mod_name[50];                    
                    parse_response_from_imrtlink(&recv_ctx.message, response);

                    ret = response->status;

                    get_pending_request_info(&mid, &op_type);

                    //printf("%d == %d; status=%d\n", response->mid, mid, ret);

                    if (response->mid != mid) {
                        //ignore invalid response 
                        printf("Unexpected response!\n");
                        output_response(response);
                        http_printf(http_mg_conn, "%s", "HTTP/1.1 412 Precondition Failed\r\n\r\n");
                        return -1;
                    }

                    if (ret == CREATED_2_01 || ret == DELETED_2_02 || ret == CONTENT_2_05) {
                        if (op_type == INSTALL) {
                            install_response_get_module_id_and_name(response, &mod_id, mod_name, sizeof(mod_name));
                            module_list_add(mod_id, mod_name);
                            mqtt_notify_module_event(EVENT_MOD_INST, mod_id, mod_name);
                        } else if (op_type == UNINSTALL) {
                            install_response_get_module_id_and_name(response, &mod_id, mod_name, 0);
                            module_list_del_by_id(mod_id);
                            mqtt_notify_module_event(EVENT_MOD_UNINST, mod_id, mod_name);
                        }
                    }

                    http_output_runtime_response(http_mg_conn, response);

                    rt_conn_response_received();

                } else if (reply_type == REPLY_TYPE_EVENT) {
                    request_t event[1] = { 0 };

                    parse_event_from_imrtlink(&recv_ctx.message, event);

                    //if (op.type == REGISTER || op.type == UNREGISTER) {
                        //printf("op=%d\n", op.type);
                        //output_event(event);
                    //}
                    //mqtt_process_runtime_event(mqtt_mg_conn, event);
                    mqtt_process_runtime_event(event);
                } else {
                    printf("received  type:%d\n", reply_type);
                }

            }
        }
    } /* end of while(1) */

    runtime_conn_close();

    return ret;
}
