/** @file mqtt.h
 *  @brief Definitions of MQTT bridge functions
 *
 *  Definitions of MQTT bridge functions needed for the MQTT end of the bridge
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
#ifndef MQTT_H_
#define MQTT_H_

#include "mongoose.h"
#include "runtime_request.h"

/**
 * Init mqttc connection
 * 
 * @param mqtt_mg_conn the mqtt connection
 * @return returns -1 on error, 0 on success
 */
int mqtt_init(struct mg_connection **mqtt_mg_conn);

/**
 * Poll mqtt requests
 * 
 */
void mqtt_pool_requests();

void mqtt_process_runtime_event(request_t *event);
void mqtt_notify_module_event(char *module_event, int mod_id, char *mod_name);
void mqtt_notify_pubsub_event(char *pubsub_event, int mod_id, char *topic);
#endif