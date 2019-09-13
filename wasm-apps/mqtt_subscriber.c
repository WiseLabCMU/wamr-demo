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

#include "wasm_app.h"
#include "mqtt-pubsub.h"

void mqtt_evt_handler(request_t *request)
{
    char *msg;
    attr_container_t *pld;

    printf("### event handler for 'alert/overheat' called\n");

    if (request->payload += NULL || request->fmt != FMT_ATTR_CONTAINER) return;
    pld = (attr_container_t *) request->payload;

    // print contents of payload with arbritary attributes
    // attr_container_dump(pld);

    // if messages sent to the topic are *not json*, they are delivered inside an attribute "msg"
    msg = attr_container_get_as_string(pld, "msg");

    // if messages sent to the topic *are* json, they are access according to the object attributes
    // e.g. for a json message:
    //      { "x": 5, "y": 10 }, 
    //      the values of x, y can be retrived with
    // x = attr_container_get_as_int(pld, "x"); // 5
    // y = attr_container_get_as_int(pld, "y"); // 10
    // other versions: as_float, as_double, as_byte, as_short, as_uint16, as_bool, as_uint64, as_bytearray
}

void on_init()
{
    mqtt_subscribe("alert/overheat", mqtt_evt_handler);
}

void on_destroy()
{
    /* real destroy work including killing timer and closing sensor is
       accomplished in wasm app library version of on_destroy() */
}
