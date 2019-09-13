/** @file mqtt.c
 *  @brief MQTT bridge functions
 *
 *  Implements functionality needed for the MQTT end of the bridge
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
#include <stdlib.h>
#include "mongoose.h"
#include "coap_ext.h"
#include "http.h"
#include "attr_container.h"
#include "cJSON.h"
#include "runtime_request.h"
#include "runtime_conn.h"
#include "bridge_tool_utils.h"

static const char *s_address = "oz.andrew.cmu.edu:1883";
static const char *s_user_name = NULL;
static const char *s_password = NULL;

//static const char *s_topic = "alert/overheat";
//static struct mg_mqtt_topic_expression s_topic_expr = {NULL, 0};

static struct mg_mgr g_mqtt_mgr;

static void mqtt_ev_handler(struct mg_connection *nc, int ev, void *p);

/**
 * Init mqttc connection
 * 
 * @param mqtt_mg_conn the mqtt connection
 * @return returns -1 on error, 0 on success
 */
int mqtt_init(struct mg_connection **mqtt_mg_conn)
{
    mg_mgr_init(&g_mqtt_mgr, NULL);
    *mqtt_mg_conn = mg_connect(&g_mqtt_mgr, s_address, mqtt_ev_handler);
    if (*mqtt_mg_conn == NULL) {
        fprintf(stderr, "Error connecting to MQTT server: %s.\n", s_address);
        return -1;
    } 

    while (mg_mgr_poll(&g_mqtt_mgr, 1000) > 0); // establish the connection

    printf("Connected to MQTT server: %s.\n", s_address);
    return 0;
}

/**
 * Poll mqtt requests
 * 
 */
void mqtt_pool_requests()
{
    while (mg_mgr_poll(&g_mqtt_mgr, 1000) > 0); // drive mg mgr state machine
}

/**
 * Default mqtt event handler
 * 
 * @param nc the connection 
 * @param ev 
 * @param ev_data 
 */
static void mqtt_ev_handler(struct mg_connection *nc, int ev, void *p) {
  struct mg_mqtt_message *msg = (struct mg_mqtt_message *) p;
  (void) nc;

  cJSON *json=NULL;
  char *str_json=NULL, *payload=NULL, *req_url=NULL;
  int max_len;

  //if (ev != MG_EV_POLL) printf("USER HANDLER GOT EVENT %d\n", ev);

  switch (ev) {
    case MG_EV_CONNECT: {
      struct mg_send_mqtt_handshake_opts opts;
      memset(&opts, 0, sizeof(opts));
      opts.user_name = s_user_name;
      opts.password = s_password;

      mg_set_protocol_mqtt(nc);
      mg_send_mqtt_handshake_opt(nc, "dummy", opts);
      break;
    }
    case MG_EV_MQTT_CONNACK:
      if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
        printf("Got mqtt connection error: %d\n", msg->connack_ret_code);
      }
      break;
    case MG_EV_MQTT_PUBACK:
      printf("Message publishing acknowledged (msg_id: %d)\n", msg->message_id);
      break;
    case MG_EV_MQTT_SUBACK:
      printf("Subscription acknowledged.\n");
      break;
    case MG_EV_MQTT_PUBLISH: {
#if 0
        char hex[1024] = {0};
        mg_hexdump(nc->recv_mbuf.buf, msg->payload.len, hex, sizeof(hex));
        printf("Got incoming message %.*s:\n%s", (int)msg->topic.len, msg->topic.p, hex);
#else
      printf("Got incoming message %.*s: %.*s\n", (int) msg->topic.len,
             msg->topic.p, (int) msg->payload.len, msg->payload.p);
#endif
      //char json_msg[256];
      
      payload = malloc(msg->payload.len+1);
      memcpy(payload, msg->payload.p, msg->payload.len);
      payload[msg->payload.len] = '\0';
      
      if (NULL == (json = cJSON_Parse(payload))) { // did not receive valid json; create it
        const char fmt_str_payload[] = "{ \"msg\": \"%.*s\"}"; 
        int max_len = msg->payload.len+strlen(fmt_str_payload)+1;
        str_json = malloc(max_len);
        snprintf(str_json, max_len, fmt_str_payload, msg->payload.len, msg->payload.p);
        json = cJSON_Parse(str_json);
      }

      max_len = msg->topic.len + strlen("/event/") + 1;
      req_url = malloc(max_len);
      snprintf(req_url, max_len, "/event/%.*s", msg->topic.len, msg->topic.p);
      
      request(req_url, COAP_EVENT_PUB, json);
      
      free(req_url);
      free(payload);
      if (str_json != NULL) free(str_json);
      cJSON_Delete(json);

      break;
    }
    case MG_EV_CLOSE:
        printf("MQTT Connection closed\n");  
  }

}

void mqtt_process_runtime_event(struct mg_connection *nc, request_t *event)
{
    struct mg_mqtt_topic_expression s_topic_expr = {NULL, 0};

    //topic[len]='\0';
    if (event->action == COAP_EVENT_SUB) {
        s_topic_expr.topic = event->url;
        printf("Subscribing to MQTT topic '%s'\n", s_topic_expr.topic);
        mg_mqtt_subscribe(nc, &s_topic_expr, 1, 41);
        mqtt_pool_requests();
        return;
    }
    if (event->action == COAP_EVENT_UNSUB) {
        printf("Unsubscribing from MQTT topic '%s'\n", event->url);
        mg_mqtt_unsubscribe(nc, &event->url, 1, 41);
        mqtt_pool_requests();
        return;
    }
    if (event->action == COAP_EVENT_PUB) {
        printf("Forwarding to MQTT topic '%s'\n", event->url);
        mg_mqtt_publish(nc, event->url, 65, MG_MQTT_QOS(0), event->payload, event->payload_len);
        mqtt_pool_requests();
        return;
    }    
}