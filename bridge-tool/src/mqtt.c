/** @file mqtt.c
 *  @brief MQTT bridge functions
 *
 *  Implements functionality needed for the MQTT end of the bridge
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
#include <stdlib.h>
#include "mqtt.h"
#include "attr_container.h"
#include "bridge_tool_utils.h"
#include "cJSON.h"
#include "coap_ext.h"
#include "config.h"
#include "http.h"
#include "mongoose.h"
#include "runtime_conn.h"
#include "runtime_request.h"
#include "http_mqtt_req.h"
#include "module_list.h"

static struct mg_mgr g_mqtt_mgr;

static void mqtt_ev_handler(struct mg_connection *nc, int ev, void *p);

static char s_rt_last_will_msg[200];
static char s_rt_topic[200];  // runtime topic, to where events are sent

// needed for event notification :(
static struct mg_connection *s_mqtt_mg_conn;

/**
 * Init mqttc connection
 *
 * @param mqtt_mg_conn the mqtt connection
 * @return returns -1 on error, 0 on success
 */
int mqtt_init(struct mg_connection **mqtt_mg_conn) {
  mg_mgr_init(&g_mqtt_mgr, NULL);
  *mqtt_mg_conn = s_mqtt_mg_conn =
      mg_connect(&g_mqtt_mgr, g_bt_config.mqtt_server_address, mqtt_ev_handler);
  if (*mqtt_mg_conn == NULL) {
    fprintf(stderr, "Error connecting to MQTT server: %s.\n",
            g_bt_config.mqtt_server_address);
    return -1;
  }

  while (mg_mgr_poll(&g_mqtt_mgr, 100) > 0); // establish the connection

  printf("Connected to MQTT server: %s.\n", g_bt_config.mqtt_server_address);
  return 0;
}

/**
 * Poll mqtt requests
 *
 */
void mqtt_pool_requests() {
  while (mg_mgr_poll(&g_mqtt_mgr, 1) > 0); // drive mg mgr state machine
}

/**
 * Default mqtt event handler
 *
 * @param nc the connection
 * @param ev
 * @param ev_data
 */
static void mqtt_ev_handler(struct mg_connection *nc, int ev, void *p) {
  struct mg_mqtt_message *msg = (struct mg_mqtt_message *)p;
  (void)nc;
  char started_msg[200], topic_str[200];

  cJSON *json = NULL;
  char *str_json = NULL, *payload = NULL, *req_url = NULL;
  int max_len;

  switch (ev) {
  case MG_EV_CONNECT: {
    struct mg_send_mqtt_handshake_opts opts;
    memset(&opts, 0, sizeof(opts));

    // init last will message
    snprintf(s_rt_last_will_msg, sizeof(s_rt_last_will_msg),
             FMTSTR_EVENT_RT_START_JSON, g_bt_config.rt_uuid, g_bt_config.rt_uuid, EVENT_RT_STOP);
    snprintf(s_rt_topic, sizeof(s_rt_topic), "%s/%s",
             g_bt_config.rt_topic_prefix, g_bt_config.rt_uuid);

    opts.user_name = NULL;
    opts.password = NULL;
    opts.will_topic = s_rt_topic;
    opts.will_message = s_rt_last_will_msg;

    mg_set_protocol_mqtt(nc);
    mg_send_mqtt_handshake_opt(nc, g_bt_config.rt_uuid, opts);
    break;
  }
  case MG_EV_MQTT_CONNACK:
    if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
      printf("Got mqtt connection error: %d\n", msg->connack_ret_code);
    }

    // init start message 
    snprintf(started_msg, sizeof(started_msg), FMTSTR_EVENT_RT_START_JSON,
            g_bt_config.rt_uuid, g_bt_config.rt_uuid, EVENT_RT_START);
    snprintf(topic_str, sizeof(topic_str), "%s%s", g_bt_config.rt_topic_prefix,
            g_bt_config.rt_uuid);

    // publish start msg
    mg_mqtt_publish(nc, g_bt_config.rt_topic_prefix, 65, MG_MQTT_QOS(0), started_msg,
                    strlen(started_msg));

    break;
  case MG_EV_MQTT_PUBACK:
    printf("Message publishing acknowledged (msg_id: %d)\n", msg->message_id);
    break;
  case MG_EV_MQTT_SUBACK:
    //printf("Subscription acknowledged.\n");
    break;
  case MG_EV_MQTT_PUBLISH: {
    printf("Got incoming message to topic:'%.*s' \n", (int)msg->topic.len, msg->topic.p);

    // ignore messages to self...
    if (mg_vcmp(&msg->topic, s_rt_topic) == 0) return;

    payload = malloc(msg->payload.len + 1);
    memcpy(payload, msg->payload.p, msg->payload.len);
    payload[msg->payload.len] = '\0';

    if (NULL == (json = cJSON_Parse(
                     payload))) { // did not receive valid json; create it
      const char fmt_str_payload[] = "{ \"raw_str\": \"%.*s\"}";
      int max_len = msg->payload.len + strlen(fmt_str_payload) + 1;
      str_json = malloc(max_len);
      snprintf(str_json, max_len, fmt_str_payload, msg->payload.len,
               msg->payload.p);
      json = cJSON_Parse(str_json);
    }

    max_len = msg->topic.len + strlen("/event/") + 1;
    req_url = malloc(max_len);
    snprintf(req_url, max_len, "/event/%.*s", msg->topic.len, msg->topic.p);

    rt_req_request(req_url, COAP_EVENT_PUB, json);

    free(req_url);
    free(payload);
    if (str_json != NULL)
      free(str_json);
    cJSON_Delete(json);

    break;
  }
  case MG_EV_CLOSE:
    printf("MQTT Connection closed\n");
  }
}

void mqtt_process_runtime_event(request_t *event) {
  struct mg_mqtt_topic_expression topic_expr = {NULL, 0};
  attr_container_t *payload = (attr_container_t *)event->payload;
  cJSON *json = NULL, *raw_str = NULL;
  char *msg_str = NULL;

  if (event->action == COAP_EVENT_SUB) {
    topic_expr.topic = event->url;
    printf("Subscribing to MQTT topic '%s'\n", topic_expr.topic);
    mg_mqtt_subscribe(s_mqtt_mg_conn, &topic_expr, 1, 41);
    mqtt_pool_requests();
    mqtt_notify_pubsub_event(EVENT_SUB_START, event->sender, event->url);
    return;
  }
  if (event->action == COAP_EVENT_UNSUB) {
    printf("Unsubscribing from MQTT topic '%s'\n", event->url);
    mg_mqtt_unsubscribe(s_mqtt_mg_conn, &event->url, 1, 41);
    mqtt_pool_requests();
    mqtt_notify_pubsub_event(EVENT_SUB_STOP, event->sender, event->url);
    return;
  }
  if (event->action == COAP_EVENT_PUB) {
    printf("Forwarding to MQTT topic '%s'\n", event->url);

    payload = (attr_container_t *)event->payload;

    if (payload == NULL || event->payload_len <= 0) {
      printf("Error getting msg payload!\n");
      return;
    }

    if ((json = attr2json(payload)) == NULL) {
      printf("Error getting msg payload as json!\n");
      msg_str = strndup(event->payload, event->payload_len);
    } else {
      raw_str = cJSON_GetObjectItemCaseSensitive(json, "raw_str");
      if (cJSON_IsString(raw_str) && (raw_str->valuestring != NULL)) {
        printf("found raw_str\n");
        msg_str = strdup(raw_str->valuestring);
      } else {
        if ((msg_str = cJSON_Print(json)) == NULL) {
          msg_str = strndup(event->payload, event->payload_len);
        }
      }
    }

    if (topic_list_check_and_add(event->url, event->sender) == 1) {
      mqtt_notify_pubsub_event(EVENT_PUB_START, event->sender, event->url);
    }

    mg_mqtt_publish(s_mqtt_mg_conn, event->url, 65, MG_MQTT_QOS(0), msg_str,
                    strlen(msg_str));
    mqtt_pool_requests();
    if (json != NULL)
      cJSON_Delete(json);
    if (msg_str != NULL)
      free(msg_str);

    return;
  }
}

void mqtt_notify_module_event(char *module_event, int mod_id, char *mod_name) 
{
  char module_id[100], event_msg[200];

  snprintf(module_id, sizeof(module_id), "%s.%s", g_bt_config.rt_uuid, mod_name);

  snprintf(event_msg, sizeof(event_msg), FMTSTR_EVENT_MOD_INST_JSON, module_id, mod_name, 
           g_bt_config.rt_uuid, module_event, mod_name);

  mg_mqtt_publish(s_mqtt_mg_conn, s_rt_topic, 65, MG_MQTT_QOS(0), event_msg, strlen(event_msg));
  mqtt_pool_requests();
}

void mqtt_notify_pubsub_event(char *pubsub_event, int mod_id, char *topic) 
{
  char pubsub_id[200], parent[200], event_msg[200];

  char *mod_name = module_list_get_name_by_id(mod_id);
  if (mod_name==NULL) {
    printf("Could not find mod_id= %d\n", mod_id);
    return;
  }

  snprintf(parent, sizeof(parent), "%s.%s", g_bt_config.rt_uuid, mod_name);
  snprintf(pubsub_id, sizeof(pubsub_id), "%s.%s", parent, topic);

  snprintf(event_msg, sizeof(event_msg), FMTSTR_EVENT_PUSBSUB_JSON, pubsub_id, topic, 
          parent, pubsub_event, topic);

  mg_mqtt_publish(s_mqtt_mg_conn, s_rt_topic, 65, MG_MQTT_QOS(0), event_msg, strlen(event_msg));
  mqtt_pool_requests();
}