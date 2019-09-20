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
#include "attr_container.h"

#define HTTP_CONTINUE_100 100
#define HTTP_OK_200 200
#define HTTP_CREATED_201 201
#define HTTP_ACCEPTED_202 202
#define HTTP_NO_CONTENT_204 204
#define HTTP_RST_CONTENT_205 205
#define HTTP_BAD_REQUEST_400 400
#define HTTP_UNAUTHORIZED_401 401
#define HTTP_FORBIDDEN_403 403
#define HTTP_NOT_FOUND_404 404
#define HTTP_METHOD_NOT_ALLOWED_405 405
#define HTTP_NOT_ACCEPTABLE_406 406
#define HTTP_PRECONDITION_FAILED_412 412
#define HTTP_REQUEST_ENTITY_TOO_LARGE_413 413
#define HTTP_UNSUPPORTED_MEDIA_TYPE_415 415
#define HTTP_INTERNAL_SERVER_ERROR_500 500
#define HTTP_NOT_IMPLEMENTED_501 501
#define HTTP_BAD_GATEWAY_502 502
#define HTTP_SERVICE_UNAVAILABLE_503 503
#define HTTP_GATEWAY_TIMEOUT_504 504

#define CT_HEADER_JSON "Content-Type: application/json"
#define CT_HEADER_TEXT "Content-Type: text/plain"

#define FMT_STR_JSON_ERROR_MSG "{ \"error message\": \"%s\"}"
/**
 * Init http server
 * 
 * @param http_mg_conn the http connection
 * @return returns -1 on error, 0 on success
 */
int http_init(struct mg_connection **http_mg_conn);

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
void http_output_runtime_response(struct mg_connection *nc, response_t *obj);

/**
 * Output http response given a a payload from the runtime
 * 
 * @param nc the connection to rest client 
 * @param payload payload object received from runtime
 * @param payload payload object received from runtime
 * @param format format of the payload object (should be FMT_ATTR_CONTAINER)
 * @param payload_len size of the payload
 */
void http_output_attr_container(struct mg_connection *nc, attr_container_t *payload, int format, int payload_len);

#endif