/** @file http.c
 *  @brief Simple HTTP RESTful service.
 *
 *  Implements the rest interface to interact with a runtime
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

static const char *s_http_port = "8000";
static const char *s_doc_root = ".";

static struct mg_serve_http_opts s_http_server_opts;

static struct mg_mgr g_mgr;

static void coap_to_http_status(char *dest_status_str, int status_str_size, int coap_status);

/**
 * Init http server
 * 
 * @param http_mg_conn the http connection
 * @return returns -1 on error, >=0 on success
 */
int http_init(struct mg_connection *http_mg_conn)
{
    s_http_server_opts.document_root = s_doc_root;
    s_http_server_opts.enable_directory_listing = "no";

    mg_mgr_init(&g_mgr, NULL);
    http_mg_conn = mg_bind(&g_mgr, s_http_port, http_ev_handler);
    if (http_mg_conn == NULL) {
        fprintf(stderr, "Error starting server on port %s:\n", s_http_port);
        return -1;
    }
    mg_set_protocol_http_websocket(http_mg_conn);

    printf("Starting RESTful server on port %s.\n", s_http_port);
    return 0;
}

/**
 * Poll http requests
 * 
 */
void http_pool_requests()
{
    while (mg_mgr_poll(&g_mgr, 1000) > 0); // drive mg smgr state machine
}

/**
 * Default http event handler
 * 
 * @param nc the connection 
 * @param ev 
 * @param ev_data 
 */
void http_ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  switch (ev) {
    case MG_EV_HTTP_REQUEST:
      if (mg_vcmp(&hm->uri, "/api/v1/test") == 0) {
        install(FILE_SUB, MODULE_NAME_SUB, NULL, NULL, 0, 0);
        get_request_response(nc);

      } else if (mg_vcmp(&hm->uri, "/api/v1/query") == 0) {
        /* Send headers */
        mg_printf(nc, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

        /*  send response as a JSON object */
        mg_printf_http_chunk(nc, "{ \"result\": \"ok\" }");
        mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
      } else {
        mg_serve_http(nc, hm, s_http_server_opts); /* Serve static content */
      }
      break;
    default:
      break;
  }
}

static void coap_to_http_status(char *dest_status_str, int status_str_size, int coap_status)
{
    switch (coap_status) {
        case CREATED_2_01: snprintf(dest_status_str, status_str_size, "201 Created"); 
            break;            
        case DELETED_2_02: 
        case VALID_2_03: 
        case CHANGED_2_04: snprintf(dest_status_str, status_str_size, "202 Accepted"); 
             break;         
        case CONTENT_2_05: snprintf(dest_status_str, status_str_size, "200 OK"); 
        case CONTINUE_2_31: snprintf(dest_status_str, status_str_size, "100 Continue"); 
            break;     

        case BAD_REQUEST_4_00: 
        case BAD_OPTION_4_02: snprintf(dest_status_str, status_str_size, "400 Bad Request"); 
            break;     
        case UNAUTHORIZED_4_01: snprintf(dest_status_str, status_str_size, "401 Unauthorized"); 
            break;     
        case FORBIDDEN_4_03: snprintf(dest_status_str, status_str_size, "403 Forbidden"); 
            break;     
        case NOT_FOUND_4_04: snprintf(dest_status_str, status_str_size, "404 Not Found"); 
            break;     
        case METHOD_NOT_ALLOWED_4_05: snprintf(dest_status_str, status_str_size, "405 Method Not Allowed"); 
            break;     
        case NOT_ACCEPTABLE_4_06: snprintf(dest_status_str, status_str_size, "406 Not Acceptable"); 
            break;     
        case PRECONDITION_FAILED_4_12: snprintf(dest_status_str, status_str_size, "412 Precondition Failed"); 
            break;     
        case REQUEST_ENTITY_TOO_LARGE_4_13: snprintf(dest_status_str, status_str_size, "413 Request Entity Too Large"); 
            break;     
        case UNSUPPORTED_MEDIA_TYPE_4_15: snprintf(dest_status_str, status_str_size, "415 Unsupported Media Type"); 
            break;     

        case INTERNAL_SERVER_ERROR_5_00: snprintf(dest_status_str, status_str_size, "500 Internal Server Error"); 
            break;     
 		case PROXYING_NOT_SUPPORTED_5_05:            
        case NOT_IMPLEMENTED_5_01: snprintf(dest_status_str, status_str_size, "501 Not Implemented"); 
            break;     
        case BAD_GATEWAY_5_02: snprintf(dest_status_str, status_str_size, "502 Bad Gateway"); 
            break;     
        case SERVICE_UNAVAILABLE_5_03: snprintf(dest_status_str, status_str_size, "503 Service Unavailable"); 
            break;     
        case GATEWAY_TIMEOUT_5_04: snprintf(dest_status_str, status_str_size, "504 Gateway Timeout"); 
            break;     
        snprintf(dest_status_str, status_str_size, "501 Not Implemented"); 
            break;            
    }
}

void http_output_response(struct mg_connection *nc, response_t *obj)
{
    char status[100];
    // COAP status to HTTP status
    coap_to_http_status(status, sizeof(status), obj->status);

    /* Send headers */
    mg_printf(nc, "HTTP/1.1 %s\r\nTransfer-Encoding: chunked\r\n\r\n", status);
    http_output(nc, obj->payload, obj->fmt, obj->payload_len);
    mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
}

void http_output(struct mg_connection *nc, attr_container_t *payload, int format, int payload_len)
{
    cJSON *json = NULL;
    char *json_str = NULL;

    if (format != FMT_ATTR_CONTAINER || payload == NULL || payload_len <= 0)
        return;

    if ((json = attr2json(payload)) == NULL)
        return;

    if ((json_str = cJSON_Print(json)) == NULL) {
        cJSON_Delete(json);
        return;
    }

    mg_printf_http_chunk(nc, "%s", json_str);

    free(json_str);
    cJSON_Delete(json);
}