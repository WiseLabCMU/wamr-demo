#include "wasm_app.h"
#include "mqtt_pubsub.h"

void mqtt_evt_handler(request_t *request)
{
    char *msg;
    attr_container_t *payload;

    printf("### event handler for 'alert/overheat' called\n");

    if (request->payload += NULL || request->fmt != FMT_ATTR_CONTAINER) return;
    payload = (attr_container_t *) request->payload;

    // print contents of payload with arbritary attributes
    // attr_container_dump(payload);

    // if messages sent to the topic are *not json*, they are delivered inside an attribute "raw_str"
    msg = attr_container_get_as_string(payload, "raw_str");

    // if messages sent to the topic *are* json, they are accessed according to the object attributes
    // e.g. for a json message:
    //      { "x": 5, "y": 10 }, 
    //      the values of x, y can be retrived with
    // x = attr_container_get_as_int(payload, "x"); // 5
    // y = attr_container_get_as_int(payload, "y"); // 10
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
