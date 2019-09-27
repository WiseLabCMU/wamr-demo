/**
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

#include "bridge_tool_utils.h"
#include "shared_utils.h"
#include "attr_container.h"

#include "runtime_request.h"
#include "runtime_conn.h"
#include "coap_ext.h"

extern unsigned char leading[2];

#define url_remain_space (sizeof(url) - strlen(url))

/*return:
 0: success
 others: fail*/
int rt_req_install(char *filename, char *app_file_buf, int app_size, char *name, char *module_type, int heap_size, int timers, int watchdog_interval)
{
    request_t request[1] = { 0 };
    char url[URL_MAX_LEN] = { 0 };
    int ret = -1;
    bool is_wasm_bytecode_app;

    snprintf(url, sizeof(url) - 1, "/applet?name=%s", name);

    if (module_type != NULL && url_remain_space > 0)
        snprintf(url + strlen(url), url_remain_space, "&type=%s",
                module_type);

    if (heap_size > 0 && url_remain_space > 0)
        snprintf(url + strlen(url), url_remain_space, "&heap=%d",
                (int)heap_size);

    if (timers > 0 && url_remain_space > 0)
        snprintf(url + strlen(url), url_remain_space, "&timers=%d",
                timers);

    if (watchdog_interval > 0 && url_remain_space > 0)
        snprintf(url + strlen(url), url_remain_space, "&wd=%d",
                watchdog_interval);

    /*TODO: permissions to access JLF resource: AUDIO LOCATION SENSOR VISION platform.SERVICE */

    if (app_file_buf == NULL && filename != NULL) { 
        app_file_buf = read_file_to_buffer(filename, &app_size);
    }

    if (app_file_buf == NULL || app_size <= 0) {
        printf("Error reading file! %s\n", filename);
        return -1;
    }

    //printf ("Installing size=%d \n", app_size);
    init_request(request, url, COAP_PUT, FMT_APP_RAW_BINARY, app_file_buf, app_size);
    request->mid = gen_random_id();

    if ((module_type == NULL || strcmp(module_type, "wasm") == 0)
            && get_package_type(app_file_buf, app_size) == Wasm_Module_Bytecode)
        is_wasm_bytecode_app = true;
    else
        is_wasm_bytecode_app = false;

    ret = send_request(request, is_wasm_bytecode_app);

    if (ret >=0) rt_conn_request_sent(INSTALL, request->mid); // indicate a request was successfully sent; 

    free(app_file_buf);

    return ret;
}

int rt_req_uninstall(char *name, char *module_type)
{
    request_t request[1] = { 0 };
    char url[URL_MAX_LEN] = { 0 };
    int ret;

    snprintf(url, sizeof(url) - 1, "/applet?name=%s", name);

    if (module_type != NULL && url_remain_space > 0)
        snprintf(url + strlen(url), url_remain_space, "&type=%s",
                module_type);

    init_request(request, url, COAP_DELETE,
    FMT_ATTR_CONTAINER,
    NULL, 0);
    request->mid = gen_random_id();

    ret = send_request(request, false);

    if (ret >=0) rt_conn_request_sent(UNINSTALL, request->mid); // indicate a request was successfully sent; 

    return ret;
}

int rt_req_query(char *name)
{
    request_t request[1] = { 0 };
    int ret = -1;
    char url[URL_MAX_LEN] = { 0 };

    if (name != NULL)
        snprintf(url, sizeof(url) - 1, "/applet?name=%s", name);
    else
        snprintf(url, sizeof(url) - 1, "/applet");

    init_request(request, url, COAP_GET,
    FMT_ATTR_CONTAINER,
    NULL, 0);
    request->mid = gen_random_id();

    ret = send_request(request, false);

    if (ret >=0) rt_conn_request_sent(QUERY, request->mid); // indicate a request was successfully sent; 

    return ret;
}

int rt_req_request(char *url, int action, cJSON *json)
{
    request_t request[1] = { 0 };
    attr_container_t *payload = NULL;
    int ret = -1, payload_len = 0;

    if (json != NULL) {
        if (NULL == (payload = json2attr(json))) {
            goto fail;
        }
        payload_len = attr_container_get_serialize_length(payload);
    }

    init_request(request, (char *)url, action,
    FMT_ATTR_CONTAINER, payload, payload_len);
    request->mid = gen_random_id();

    ret = send_request(request, false);

    if (payload != NULL)
        attr_container_destroy(payload);

    if (ret >=0) rt_conn_request_sent(REQUEST, request->mid); // indicate a request was successfully sent; 

    fail: return ret;
}

/*
 TODO: currently only support 1 url.
 how to handle multiple responses and set process's exit code?
 */
int rt_req_subscribe(char *urls)
{
    request_t request[1] = { 0 };
    int ret = -1;
    char url[URL_MAX_LEN] = { 0 };
    char *prefix = urls[0] == '/' ? "/event" : "/event/";
    sprintf(url, "%s%s", prefix, urls);
    init_request(request, url, COAP_PUT,
    FMT_ATTR_CONTAINER,
    NULL, 0);
    request->mid = gen_random_id();
    ret = send_request(request, false);

    if (ret >=0) rt_conn_request_sent(REGISTER, request->mid); // indicate a request was successfully sent; 

    return ret;
}

/*
 TODO: currently only support 1 url.
 */
int rt_req_unsubscribe(char *urls)
{
    request_t request[1] = { 0 };
    int ret = -1;
    char url[URL_MAX_LEN] = { 0 };
    sprintf(url, "%s%s", "/event/", urls);
    init_request(request, url, COAP_DELETE,
    FMT_ATTR_CONTAINER,
    NULL, 0);
    request->mid = gen_random_id();
    ret = send_request(request, false);

    if (ret >=0) rt_conn_request_sent(UNREGISTER, request->mid); // indicate a request was successfully sent; 

    return ret;
}

void output_event(request_t *obj)
{
    char header[256] = { 0 };
    snprintf(header, sizeof(header), "\nreceived an event %s (%d)\n", obj->url, obj->action);
    output(header, obj->payload, obj->fmt, obj->payload_len);
}

/* -1 fail, 0 success */
int send_request(request_t *request, bool is_install_wasm_bytecode_app)
{
    char *req_p;
    int req_size, req_size_n, ret = -1;
    uint16_t msg_type = REQUEST_PACKET;

    if (is_install_wasm_bytecode_app)
        msg_type = INSTALL_WASM_BYTECODE_APP;

    if ((req_p = pack_request(request, &req_size)) == NULL)
        return -1;

    /* leading bytes */
    if (!host_tool_send_data(leading, sizeof(leading)))
        goto ret;

    /* message type */
    msg_type = htons(msg_type);
    if (!host_tool_send_data((char *) &msg_type, sizeof(msg_type)))
        goto ret;

    /* payload length */
    req_size_n = htonl(req_size);
    if (!host_tool_send_data((char *) &req_size_n,
            sizeof(req_size_n)))
        goto ret;

    /* payload */
    if (!host_tool_send_data(req_p, req_size))
        goto ret;

    ret = 0;

    ret: free_req_resp_packet(req_p);

    return ret;
}

PackageType get_package_type(const char *buf, int size)
{
    if (buf && size > 4) {
        if (buf[0] == '\0' && buf[1] == 'a' && buf[2] == 's' && buf[3] == 'm')
            return Wasm_Module_Bytecode;
        if (buf[0] == '\0' && buf[1] == 'a' && buf[2] == 'o' && buf[3] == 't')
            return Wasm_Module_AoT;
    }
    return Package_Type_Unknown;
}
