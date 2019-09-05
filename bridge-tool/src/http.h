 /** @file http.h
 *  @brief Definitions for a small RESTful service.
 *
 *  Definitions for a small HTTP RESTful service.
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
#ifndef HTTP_H_
#define HTTP_H_

#include "mongoose.h"
#include "http.h"
#include "attr_container.h"

#define FILE_SUB "../../out/wasm-apps/event_subscriber.wasm"
#define MODULE_NAME_SUB "sub"

/**
 * Init http server
 * 
 * @param http_mg_conn the http connection
 * @return returns -1 on error, >=0 on success
 */
int http_init(struct mg_connection *http_mg_conn);

/**
 * Poll http requests
 * 
 */
void http_pool_requests();

/**
 * Default http event handler
 * 
 * @param nc the connection 
 * @param ev 
 * @param ev_data 
 */
void http_ev_handler(struct mg_connection *nc, int ev, void *ev_data);

/**
 * Reads response from runtime
 * 
 * @param http_mg_conn the connection to rest client 
 * @param runtime_conn connection to runtime
 * @return returns -1 on error, >=0 status code
 */
int http_get_request_response(struct mg_connection *http_mg_conn, int runtime_conn_fd);

/**
 * Output http response given a response object from the runtime
 * 
 * @param nc the connection to rest client 
 * @param obj response object received from runtime
 */
void http_output_response(struct mg_connection *nc, response_t *obj);

/**
 * Output http response given a a payload from the runtime
 * 
 * @param nc the connection to rest client 
 * @param payload payload object received from runtime
 * @param payload payload object received from runtime
 * @param format format of the payload object (should be FMT_ATTR_CONTAINER)
 * @param payload_len size of the payload
 */
void http_output(struct mg_connection *nc, attr_container_t *payload, int format, int payload_len);

#endif