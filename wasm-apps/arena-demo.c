#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wasm_app.h"
#include "mqtt_pubsub.h"

#define MSG_FORMAT_STR "%s,%d,%d,%d,0,0,0,0,1,1,1,#FFEEAA,on"
#define STR_MAX_LEN 50

char obj_name[]="sphere_145";
char topic[STR_MAX_LEN];

int x=0, y=1, z=1;
int i=0, d=0;

/* Timer callback */
void timer1_update(user_timer_t timer)
{
    attr_container_t *msg;
    char str_msg[STR_MAX_LEN];    

    // use an attribute container to send a json formatted message
    msg = attr_container_create("msg"); // "msg" is the attribute container tag; not forwarded to mqtt

    if (d == 0) {
        x = x + 2;
        z -= 1; 
        i++;
    } else {
        x = x - 2;
        z += 1; 
        i--;
    }
    snprintf(str_msg, STR_MAX_LEN, MSG_FORMAT_STR, obj_name, x, y, z);

    // change direction
    if (i == 10 || i == 0) d=!d;

    // publish message
    attr_container_set_string(&msg, "raw_str", str_msg); 

    mqtt_publish(topic, FMT_ATTR_CONTAINER, msg,
            attr_container_get_serialize_length(msg));

    attr_container_destroy(msg);
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
    snprintf(topic, STR_MAX_LEN, "/topic/render/%s", obj_name);

    start_timer();
}

void on_destroy()
{

}
