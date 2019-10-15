#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 
#include "wasm_app.h"
#include "mqtt_pubsub.h"
#include "stdlib-legacy.h" // gcvt()

#define MSG_FORMAT_STR "%s,%s,1,%s,0,0,0,0,0.2,0.2,0.2,#FFEEAA,on"
#define STR_MAX_LEN 100

char obj_name[STR_MAX_LEN];
char topic[STR_MAX_LEN];

float x=0, z=1;

/* Timer callback */
void timer1_update(user_timer_t timer)
{
    char str_x[20], str_y[20], str_z[20];
    attr_container_t *msg;
    char str_msg[STR_MAX_LEN];    

    // use an attribute container to send a json formatted message
    msg = attr_container_create("msg"); // "msg" is the attribute container tag; not forwarded to mqtt

    x += (((double)rand() / (double)RAND_MAX) - 0.5) / 2.0;
    z += (((double)rand() / (double)RAND_MAX) - 0.5) / 2.0; 

    // convert floaf to string; (need due to bug in snprintf's %f)
    gcvt(x, 5, str_x); 
    gcvt(z, 5, str_z); 
    snprintf(str_msg, STR_MAX_LEN, MSG_FORMAT_STR, obj_name, str_x, str_z);

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
    srand(time(0));
    int i = rand();
    snprintf(obj_name, STR_MAX_LEN, "sphere_%d", 1000 + rand() % 1000); // large random id to avoid name collisions
    snprintf(topic, STR_MAX_LEN, "/topic/earth/%s", obj_name);

    start_timer();
}

void on_destroy()
{

}
