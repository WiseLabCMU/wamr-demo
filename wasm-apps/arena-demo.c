#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 
#include "wasm_app.h"
#include "mqtt_pubsub.h"
#include "stdlib-legacy.h" // gcvt()

#define MSG_FORMAT_STR "{\"object_id\" : \"%s\", \"action\": \"update\", \"type\": \"object\", \"data\": {\"position\": {\"x\": \"%s\", \"y\": \"2\", \"z\": \"%s\"}}}"
#define MSG_BUF_MAX_LEN 500
#define STR_MAX_LEN 50

char obj_name[STR_MAX_LEN];
char topic[STR_MAX_LEN];

float x=0, z=1;

/* Timer callback */
void timer1_update(user_timer_t timer)
{
    char str_x[20], str_z[20];
    attr_container_t *msg;
    char msg_buf[MSG_BUF_MAX_LEN];    

    // use an attribute container to send a json formatted message
    msg = attr_container_create("msg"); // "msg" is the attribute container tag; not forwarded to mqtt

    x += (((double)rand() / (double)RAND_MAX) - 0.5) / 2.0;
    z += (((double)rand() / (double)RAND_MAX) - 0.5) / 2.0; 

    // convert floaf to string; 
    gcvt(x, 5, str_x); 
    gcvt(z, 5, str_z); 
    snprintf(msg_buf, MSG_BUF_MAX_LEN, "{\"object_id\" : \"%s\", \"action\": \"create\", \"type\": \"object\", \"data\": {\"object_type\": \"sphere\", \"position\": {\"x\": \"%s\", \"y\": \"1\", \"z\": \"%s\"}, \"color\": \"#FF0000\"}}" , obj_name, str_x, str_z);
    //snprintf(msg_buf, MSG_BUF_MAX_LEN, MSG_FORMAT_STR, obj_name, str_x, str_z);

    // publish message
    attr_container_set_string(&msg, "raw_str", msg_buf);     

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
    char str_x[20], str_z[20];
    attr_container_t *msg;
    char msg_buf[MSG_BUF_MAX_LEN];    

    srand(time(0));
    int i = rand();
    snprintf(obj_name, STR_MAX_LEN, "sphere_%d", 1000 + rand() % 1000); // large random id to avoid name collisions
    snprintf(topic, STR_MAX_LEN, "realm/s/render/%s", obj_name);

    // use an attribute container to send a json formatted message
    msg = attr_container_create("msg"); // "msg" is the attribute container tag; not forwarded to mqtt

    // convert floaf to string; 
    gcvt(x, 5, str_x); 
    gcvt(z, 5, str_z); 
    snprintf(msg_buf, MSG_BUF_MAX_LEN, "{\"object_id\" : \"%s\", \"action\": \"create\", \"type\": \"object\", \"data\": {\"object_type\": \"sphere\", \"position\": {\"x\": \"%s\", \"y\": \"1\", \"z\": \"%s\"}, \"color\": \"#FF0000\"}}" , obj_name, str_x, str_z);

    // publish message
    attr_container_set_string(&msg, "raw_str", msg_buf);     

    mqtt_publish(topic, FMT_ATTR_CONTAINER, msg,
            attr_container_get_serialize_length(msg));

    attr_container_destroy(msg);

    start_timer();
}

void on_destroy()
{

}
