#include "wasm_app.h"
#include "mqtt_pubsub.h"

void publish_alert()
{
    attr_container_t *msg;

    // use an attribute container to send a json formatted message
    msg = attr_container_create("msg"); // "msg" is the attribute container tag; not forwarded to mqtt

    // e.g. the following will create a json string: { "x": 5, "y": 10 }
     attr_container_set_int(&msg, "x", 5); 
     attr_container_set_int(&msg, "y", 10);
    // other versions: set_string, set_float, set_double, set_byte, set_short, set_uint16, set_bool, set_uint64, set_bytearray 

    // to publish a *non-json formatted* string, insert the message inside an attribute "raw_str" (other attributes will be ignored)
    //attr_container_set_string(&msg, "raw_str", "test message!"); // 'overheat detected' will be published to the topic 

    // publish to topic 'alert/test'
    mqtt_publish("alert/test", FMT_ATTR_CONTAINER, msg,
            attr_container_get_serialize_length(msg));

    attr_container_destroy(msg);
}

/* Timer callback */
void timer1_update(user_timer_t timer)
{
    publish_alert();
}

void start_timer()
{
    user_timer_t timer;

    /* set up a timer */
    timer = api_timer_create(1000, true, false, timer1_update);
    api_timer_restart(timer, 1000);
}

void on_init()
{
    start_timer();
}

void on_destroy()
{
    /* real destroy work including killing timer and closing sensor is accomplished in wasm app library version of on_destroy() */
}
