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
 * @return returns -1 on error, 0 on success
 */
int http_init(struct mg_connection **http_mg_conn);

/**
 * Default http event handler
 * 
 * @param nc the connection 
 * @param ev 
 * @param ev_data 
 */
void http_ev_handler(struct mg_connection *nc, int ev, void *ev_data);

/**
 * Printf http response 
 * 
 * @param ... format string and parameters to print
 */
void http_printf(struct mg_connection *nc, const char *fmt, ...);

/**
 * Output http response given a response object from the runtime
 * 
 * @param nc the connection to rest client 
 * @param obj response object received from runtime
 */
void http_output_runtime_response(struct mg_connection *nc, response_t *obj);

#endif