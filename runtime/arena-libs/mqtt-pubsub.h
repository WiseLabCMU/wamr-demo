#ifndef _MQTT_PUBSUB_H_
#define _MQTT_PUBSUB_H_

#include "shared_utils.h"

/**
 * @typedef request_handler_f
 *
 * @brief Define the signature of callback function for API
 * api_register_resource_handler() to handle request or for API
 * api_subscribe_event() to handle event.
 *
 * @param request pointer of the request to be handled
 *
 * @see api_register_resource_handler
 * @see api_subscribe_event
 */
typedef void (*request_handler_f)(request_t *request);

/*
 *****************
 * Event APIs
 *****************
 */

/**
 * @brief Publish to mqtt.
 *
 * @param topic topic 
 * @param fmt format of the payload
 * @param payload payload to send
 * @param payload_len length in bytes of the payload
 *
 * @return true if success, false otherwise
 */
bool mqtt_publish(const char *url, int fmt, void *payload,
        int payload_len);


/**
 * @brief Subscribe an mqtt topic.
 *
 * @param topic topic 
 * @param handler callback function to handle the event.
 *
 * @return true if success, false otherwise
 */
bool mqtt_subscribe(const char *topic, request_handler_f handler);

#endif
