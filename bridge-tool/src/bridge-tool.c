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

#include "cJSON.h"
#include "runtime_conn.h"
#include "runtime_request.h"
#include "http.h"

int main(int argc, char *argv[])
{
    int ret;
    imrt_link_recv_context_t recv_ctx = { 0 };
    char buffer[BUF_SIZE] = { 0 };
    uint32_t last_check, total_elpased_ms = 0;
    //bool is_responsed = true;
    fd_set readfds;
    int result = 0;
    struct timeval tv;
    int runtime_conn_fd;
    struct mg_connection *http_mg_conn=NULL;

    //TODO: reconnect 3 times
    if (runtime_conn_init(&runtime_conn_fd) != 0)
        return -1;

    if (http_init(http_mg_conn) != 0 )
        return -1;

    bh_get_elpased_ms(&last_check);

    while (1) {
        //struct timeval tv;

        total_elpased_ms += bh_get_elpased_ms(&last_check);

        if (runtime_conn_fd == -1) {
            if (runtime_conn_init() != 0) {
                sleep(1);
                continue;
            }
        }

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        // initialize the set of active sockets (readfds is changed at each select() call)
        FD_ZERO (&readfds);
        FD_SET (runtime_conn_fd, &readfds);
        FD_SET (http_mg_conn->sock, &readfds);

        result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

        if (result < 0) {
            if (errno != EINTR) {
                printf("Error in select, errno: 0x%x\n", errno);
                ret = -1;
                goto ret;
            }
        } else if (result == 0) { /* select timeout */
        } else if (result > 0) {
            if (FD_ISSET(http_mg_conn->sock, &readfds)) {
                http_pool_requests();
            }
            if (FD_ISSET(runtime_conn_fd, &readfds)) {
                int n, reply_type = -1;

                n = read(runtime_conn_fd, buffer, BUF_SIZE);
                if (n <= 0) {
                    runtime_conn_fd = -1;
                    continue;
                }

                reply_type = process_rcvd_data((char *) buffer, n, &recv_ctx);

                if (reply_type == REPLY_TYPE_RESPONSE) {
                    response_t response[1] = { 0 };

                    parse_response_from_imrtlink(&recv_ctx.message, response);

                    //is_responsed = true;
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

    runtime_conn_close();

    return ret;
}
