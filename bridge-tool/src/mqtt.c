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

static struct mg_mgr g_mqtt_mgr;

static void mqtt_ev_handler(struct mg_connection *nc, int ev, void *p);

static char s_rt_last_will_msg[200];
static char s_rt_last_will_topic[200];  // last will topic is also the runtime topic, where commands are received

static struct mg_str s_rt_topic;

/**
 * Init mqttc connection
 *
 * @param mqtt_mg_conn the mqtt connection
 * @return returns -1 on error, 0 on success
 */
int mqtt_init(struct mg_connection **mqtt_mg_conn) {
  char started_msg[200], topic_str[200];

  mg_mgr_init(&g_mqtt_mgr, NULL);
  *mqtt_mg_conn =
      mg_connect(&g_mqtt_mgr, g_bt_config.mqtt_server_address, mqtt_ev_handler);
  if (*mqtt_mg_conn == NULL) {
    fprintf(stderr, "Error connecting to MQTT server: %s.\n",
            g_bt_config.mqtt_server_address);
    return -1;
  }

  while (mg_mgr_poll(&g_mqtt_mgr, 10) > 0); // establish the connection

  // init start message
  snprintf(started_msg, sizeof(started_msg), FMTSTR_RT_CMD_RT_START_JSON,
           g_bt_config.rt_uuid, RTCMD_RT_START);
  snprintf(topic_str, sizeof(topic_str), "%s%s", g_bt_config.rt_topic_prefix,
           g_bt_config.rt_uuid);

  // publish start msg
  mg_mqtt_publish(*mqtt_mg_conn, topic_str, 65, MG_MQTT_QOS(0), started_msg,
                  strlen(started_msg));

  while (mg_mgr_poll(&g_mqtt_mgr, 10) > 0); // publish

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

void handle_module_install(struct mg_connection *nc,
                             cJSON *req_json) {
  char str_filepath[100] = "", str_module_name[50] = "";
  const cJSON *json_module_name = NULL;
  const cJSON *json_wasm_file = NULL;

  json_module_name = cJSON_GetObjectItemCaseSensitive(req_json, REQ_NAME_VAR);
  if (cJSON_IsString(json_module_name) &&
      (json_module_name->valuestring != NULL)) {
    strncpy(str_module_name, json_module_name->valuestring,
            sizeof(str_module_name));
  } else {
    printf("Could not parse json for 'name'.\n");
    return;
  }
  json_wasm_file = cJSON_GetObjectItemCaseSensitive(req_json, REQ_FILENAME_VAR);
  if (cJSON_IsString(json_wasm_file) && (json_wasm_file->valuestring != NULL)) {
    snprintf(str_filepath, sizeof(str_filepath), "%s/%s",
             g_bt_config.rt_wasm_files_folder, json_wasm_file->valuestring);
  } else {
    printf("Could not parse jason for 'wasm_file'.\n");
    return;
  }

  printf("installing from file: %s\n", str_filepath);
  if (install(str_filepath, NULL, 0, str_module_name, NULL, 0, 0, 0) < 0) {
    printf("Error installing (wasm file not found?).\n");
  }
}

void handle_module_uninstall(struct mg_connection *nc,
                             cJSON *req_json) {
  char str_module_name[50] = "";
  const cJSON *json_module_name = NULL;

  json_module_name = cJSON_GetObjectItemCaseSensitive(req_json, REQ_NAME_VAR);
  if (cJSON_IsString(json_module_name) &&
      (json_module_name->valuestring != NULL)) {
    strncpy(str_module_name, json_module_name->valuestring,
            sizeof(str_module_name));
  } else {
    printf("Could not parse json for 'name'.\n");
    return;
  }

  printf("uninstalling module: %s\n", str_module_name);
  if (uninstall(str_module_name, NULL) < 0) {
    printf("Error installing (wasm file not found?).\n");
  }
}

void handle_rt_topic_message(struct mg_connection *nc,
                             struct mg_mqtt_message *msg) {
  const cJSON *json_cmd = NULL;

  // make sure payload in null-terminated
  char *msg_payload = malloc(msg->payload.len + 1);
  memcpy(msg_payload, msg->payload.p, msg->payload.len);
  msg_payload[ msg->payload.len] = '\0';

  cJSON *req_json = cJSON_Parse(msg_payload);
  if (req_json == NULL) {
    printf("Could not parse request json.\n");
    return;
  }

  json_cmd = cJSON_GetObjectItemCaseSensitive(req_json, REQ_CMD_VAR);
  if (cJSON_IsString(json_cmd) &&
      (json_cmd->valuestring != NULL)) {
        if (strncmp(json_cmd->valuestring, RTCMD_MOD_INST, strlen(RTCMD_MOD_INST)) == 0 ) {
            handle_module_install(nc, req_json);
            free(msg_payload);
            cJSON_Delete(req_json);
            return;
        } if (strncmp(json_cmd->valuestring, RTCMD_MOD_UNINST, strlen(RTCMD_MOD_UNINST)) == 0 ) {
            handle_module_uninstall(nc, req_json);
            free(msg_payload);
            cJSON_Delete(req_json);
            return;
        }

  }

  printf("Message to %.*s (ignored)\n", msg->topic.len, msg->topic.p);
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

  cJSON *json = NULL;
  char *str_json = NULL, *payload = NULL, *req_url = NULL;
  int max_len;

  // if (ev != MG_EV_POLL) printf("USER HANDLER GOT EVENT %d\n", ev);

  switch (ev) {
  case MG_EV_CONNECT: {
    struct mg_send_mqtt_handshake_opts opts;
    memset(&opts, 0, sizeof(opts));

    // init last will message
    snprintf(s_rt_last_will_msg, sizeof(s_rt_last_will_msg),
             FMTSTR_RT_CMD_RT_START_JSON, g_bt_config.rt_uuid, RTCMD_RT_STOP);
    snprintf(s_rt_last_will_topic, sizeof(s_rt_last_will_topic), "%s%s",
             g_bt_config.rt_topic_prefix, g_bt_config.rt_uuid);

    s_rt_topic = mg_mk_str(s_rt_last_will_topic); // create mg_str to for compares as messages arrive

    opts.user_name = NULL;
    opts.password = NULL;
    opts.will_topic = s_rt_last_will_topic;
    opts.will_message = s_rt_last_will_msg;

    mg_set_protocol_mqtt(nc);
    mg_send_mqtt_handshake_opt(nc, g_bt_config.rt_uuid, opts);
    break;
  }
  case MG_EV_MQTT_CONNACK:
    if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
      printf("Got mqtt connection error: %d\n", msg->connack_ret_code);
    }

    // subscribe to mqtt runtime topic
    struct mg_mqtt_topic_expression s_topic_expr = {NULL, 0};
    s_topic_expr.topic = s_rt_last_will_topic;
    mg_mqtt_subscribe(nc, &s_topic_expr, 1, 41);

    break;
  case MG_EV_MQTT_PUBACK:
    printf("Message publishing acknowledged (msg_id: %d)\n", msg->message_id);
    break;
  case MG_EV_MQTT_SUBACK:
    printf("Subscription acknowledged.\n");
    break;
  case MG_EV_MQTT_PUBLISH: {
    printf("Got incoming message; topic:'%.*s'; msg:'%.*s'\n", (int)msg->topic.len,
           msg->topic.p, (int)msg->payload.len, msg->payload.p);

    // message to runtime topic
    if (mg_str_starts_with(msg->topic, s_rt_topic) == 1) {
      handle_rt_topic_message(nc, msg);
      return;
    }

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

    request(req_url, COAP_EVENT_PUB, json);

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

void mqtt_process_runtime_event(struct mg_connection *nc, request_t *event) {
  struct mg_mqtt_topic_expression s_topic_expr = {NULL, 0};
  attr_container_t *payload = (attr_container_t *)event->payload;
  cJSON *json = NULL, *raw_str = NULL;
  char *msg_str = NULL;

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

    mg_mqtt_publish(nc, event->url, 65, MG_MQTT_QOS(0), msg_str,
                    strlen(msg_str));
    mqtt_pool_requests();
    if (json != NULL)
      cJSON_Delete(json);
    if (msg_str != NULL)
      free(msg_str);

    return;
  }
}