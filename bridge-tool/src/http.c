/** @file http.c
 *  @brief Simple HTTP RESTful service.
 *
 *  Implements the rest interface to interact with a runtime
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
#include <stdlib.h>
#include <pthread.h> 
#include "mongoose.h"
#include "coap_ext.h"
#include "http.h"
#include "attr_container.h"
#include "cJSON.h"
#include "runtime_request.h"
#include "runtime_conn.h"
#include "bridge_tool_utils.h"
#include "config.h"
#include "http_mqtt_req.h"

static struct mg_serve_http_opts s_http_server_opts;

static struct mg_mgr g_http_mgr;

static void *http_pool_requests(void *arg);
static void http_printf_with_status(struct mg_connection *nc, int http_status, const char *content_type_header, const char *fmt, ...);
static int coap_to_http_status(int coap_status);
static void http_handle_modules(struct mg_connection *nc, struct http_message *hm);
static int http_handle_module_install(struct mg_connection *nc, struct http_message *hm);
static int http_handle_module_uninstall(struct mg_connection *nc, struct http_message *hm);
static char *http_attr_container_to_str(attr_container_t *payload, int format, int payload_len);

char *last_response_str=NULL;
int last_response_status;

/**
 * Init http server
 * 
 * @param http_mg_conn the http connection
 * @return returns -1 on error, 0 on success
 */
int http_init(struct mg_connection **http_mg_conn)
{
    pthread_t thread_id;        
    s_http_server_opts.document_root = g_bt_config.http_doc_root;
    s_http_server_opts.enable_directory_listing = g_bt_config.http_enable_directory_listing;

    mg_mgr_init(&g_http_mgr, NULL);
    *http_mg_conn = mg_bind(&g_http_mgr, g_bt_config.http_port, http_ev_handler);
    if (*http_mg_conn == NULL) {
        fprintf(stderr, "Error starting server on port %s:\n", g_bt_config.http_port);
        return -1;
    }
    mg_set_protocol_http_websocket(*http_mg_conn);

    /* starts a new thread to pool for http requests */
	if (pthread_create( &thread_id , (const pthread_attr_t *)NULL , http_pool_requests , NULL))
	{
		printf("Could not create thread.\n");
		return -1;
	}
    return 0;
}

/**
 * Poll http requests
 * 
 */
void *http_pool_requests(void *arg)
{
  printf("Started RESTful server on port %s.\n", g_bt_config.http_port);

  for (;;) {
   mg_mgr_poll(&g_http_mgr, 10);
  }

  mg_mgr_free(&g_http_mgr);
  return NULL;
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
      if (mg_vcmp(&hm->uri, "/cwasm/v1/modules") == 0) {
          http_handle_modules(nc, hm);
      } else {
        mg_serve_http(nc, hm, s_http_server_opts); /* Serve static content */
      }
      break;
  }
}

static int http_handle_module_uninstall(struct mg_connection *nc, struct http_message *hm) {
    char str_module_name[50]="";

    struct mg_str *hdr = mg_get_http_header(hm, "Content-Type");
        if (mg_vcmp(hdr, "application/x-www-form-urlencoded") == 0) {
            if (mg_get_http_var(&hm->body, REQ_NAME_VAR, str_module_name, sizeof(str_module_name)) <= 0) {
                http_printf_with_status(nc, HTTP_BAD_REQUEST_400, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Missing module name.");
                return -1;
            }  
        } else if (mg_vcmp(hdr, "application/json") == 0) {
            cJSON *json_module_name = NULL;
            // make sure boy  is null-terminated
            char *http_body = malloc(hm->body.len+1);
            memcpy(http_body,hm->body.p, hm->body.len);
            http_body[hm->body.len]='\0';
            cJSON *req_json = cJSON_Parse(http_body);
            if (req_json == NULL) {
                http_printf_with_status(nc, HTTP_BAD_REQUEST_400, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Could not parse request json.");
                free(http_body);
                return -1;
            }
            json_module_name = cJSON_GetObjectItemCaseSensitive(req_json, REQ_NAME_VAR);
            if (cJSON_IsString(json_module_name) && (json_module_name->valuestring != NULL)) {
                strncpy(str_module_name, json_module_name->valuestring, sizeof(str_module_name));
            } else {
                http_printf_with_status(nc, HTTP_BAD_REQUEST_400, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Could not parse json for 'name'.");
                cJSON_Delete(req_json);
                free(http_body);
                return -1;
            }
            cJSON_Delete(req_json);
            free(http_body);         
        } else {
            http_printf_with_status(nc, HTTP_UNSUPPORTED_MEDIA_TYPE_415, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Only support x-www-form-urlencoded and json media types.");
            return -1;
        }
    printf("uninstalling module: %s\n", str_module_name);
    if (rt_req_uninstall(str_module_name, NULL) < 0) {
        http_printf_with_status(nc, HTTP_BAD_REQUEST_400, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Error installing (wasm file not found?).");
        return -1;
    }
    return 0;
}

static int http_handle_module_install(struct mg_connection *nc, struct http_message *hm) {
    char str_filepath[100]="", str_module_name[50]="", str_wasm_file[50]=""; 

        struct mg_str *hdr = mg_get_http_header(hm, "Content-Type");
        if (mg_vcmp(hdr, "application/x-www-form-urlencoded") == 0) {
            if (mg_get_http_var(&hm->body, REQ_NAME_VAR, str_module_name, sizeof(str_module_name)) <= 0) {
                http_printf_with_status(nc, HTTP_BAD_REQUEST_400, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Missing module name.");
                return -1;
            }  
            if (mg_get_http_var(&hm->body, REQ_FILENAME_VAR, str_wasm_file, sizeof(str_wasm_file)) <= 0) {
                http_printf_with_status(nc, HTTP_BAD_REQUEST_400, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Missing wasm file name.");
                return -1;
            } else {
                snprintf(str_filepath, sizeof(str_filepath), "%s/%s", g_bt_config.rt_wasm_files_folder, str_wasm_file);
            }
        } else if (mg_vcmp(hdr, "application/json") == 0) {
            const cJSON *json_module_name = NULL;
            const cJSON *json_wasm_file = NULL;
            // make sure boy is null-terminated
            char *http_body = malloc(hm->body.len+1);
            memcpy(http_body,hm->body.p, hm->body.len);
            http_body[hm->body.len]='\0';
            cJSON *req_json = cJSON_Parse(http_body);
            if (req_json == NULL) {
                http_printf_with_status(nc, HTTP_BAD_REQUEST_400, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Could not parse request json.");
                free(http_body);
                return -1;
            }
            json_module_name = cJSON_GetObjectItemCaseSensitive(req_json, REQ_NAME_VAR);
            if (cJSON_IsString(json_module_name) && (json_module_name->valuestring != NULL)) {
                strncpy(str_module_name, json_module_name->valuestring, sizeof(str_module_name));
            } else {
                http_printf_with_status(nc, HTTP_BAD_REQUEST_400, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Could not parse json for 'name'.");
                cJSON_Delete(req_json);
                free(http_body);
                return -1;
            }
            json_wasm_file = cJSON_GetObjectItemCaseSensitive(req_json, REQ_FILENAME_VAR);
            if (cJSON_IsString(json_wasm_file) && (json_wasm_file->valuestring != NULL)) {
                snprintf(str_filepath, sizeof(str_filepath), "%s/%s", g_bt_config.rt_wasm_files_folder, json_wasm_file->valuestring);
            } else {
                http_printf_with_status(nc, HTTP_BAD_REQUEST_400, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Could not parse jason for 'wasm_file'.");
                cJSON_Delete(req_json);
                free(http_body);
                return -1;
            }
            cJSON_Delete(req_json);
            free(http_body);         
        } else {
            http_printf_with_status(nc, HTTP_UNSUPPORTED_MEDIA_TYPE_415, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Only support x-www-form-urlencoded and json media types.");
            return -1;
        }

    printf("installing from file: %s\n", str_filepath);
    if (rt_req_install(str_filepath, NULL, 0, str_module_name, NULL, 0, 0, 0) < 0) {
        http_printf_with_status(nc, HTTP_BAD_REQUEST_400, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Error installing (wasm file not found?).");
        return -1;
    }
    return 0;
}

static void http_handle_modules(struct mg_connection *nc, struct http_message *hm) 
{
    if (mg_vcmp(&hm->method, "POST") == 0) {
        http_handle_module_install(nc, hm);
        
    } else if (mg_vcmp(&hm->method, "DELETE") == 0) {
        http_handle_module_uninstall(nc, hm);
    } else if (mg_vcmp(&hm->method, "GET") == 0) {
        if (rt_req_query(NULL) >= 0);
    } else { 
        http_printf_with_status(nc, HTTP_METHOD_NOT_ALLOWED_405, CT_HEADER_JSON, FMT_STR_JSON_ERROR_MSG, "Method not supported.");
        return;
    }
    rt_conn_wait_pending_response();
    if (last_response_str != NULL) {
        //printf("Sending response to http client: %s\n", last_response_str);
        http_printf_with_status(nc, last_response_status, CT_HEADER_JSON, "%s", last_response_str);
        free(last_response_str);
        last_response_str=NULL;
    } else {
        http_printf(nc, "%s", "HTTP/1.1 500 Internal Server Error\r\n\r\n");
    }
}

static int coap_to_http_status(int coap_status)
{
    switch (coap_status) {
        case CREATED_2_01: return HTTP_CREATED_201;
        case DELETED_2_02: 
        case VALID_2_03: 
        case CHANGED_2_04: return HTTP_ACCEPTED_202; 
        case CONTENT_2_05: return HTTP_OK_200;
        case CONTINUE_2_31: return HTTP_CONTINUE_100; 
        case BAD_REQUEST_4_00: 
        case BAD_OPTION_4_02: return HTTP_BAD_REQUEST_400; 
        case UNAUTHORIZED_4_01: return HTTP_UNAUTHORIZED_401; 
        case FORBIDDEN_4_03: return HTTP_FORBIDDEN_403; 
        case NOT_FOUND_4_04: return HTTP_NOT_FOUND_404;
        case METHOD_NOT_ALLOWED_4_05: return HTTP_METHOD_NOT_ALLOWED_405; 
        case NOT_ACCEPTABLE_4_06: return HTTP_NOT_ACCEPTABLE_406; 
        case PRECONDITION_FAILED_4_12: return HTTP_PRECONDITION_FAILED_412; 
        case REQUEST_ENTITY_TOO_LARGE_4_13: return HTTP_REQUEST_ENTITY_TOO_LARGE_413;
        case UNSUPPORTED_MEDIA_TYPE_4_15: return HTTP_UNSUPPORTED_MEDIA_TYPE_415; 
        case INTERNAL_SERVER_ERROR_5_00: return HTTP_INTERNAL_SERVER_ERROR_500; 
 		case PROXYING_NOT_SUPPORTED_5_05:            
        case NOT_IMPLEMENTED_5_01: return HTTP_NOT_IMPLEMENTED_501; 
        case BAD_GATEWAY_5_02: return HTTP_BAD_GATEWAY_502; 
        case SERVICE_UNAVAILABLE_5_03: return HTTP_SERVICE_UNAVAILABLE_503; 
        case GATEWAY_TIMEOUT_5_04: return HTTP_GATEWAY_TIMEOUT_504;
    }
    return HTTP_NOT_IMPLEMENTED_501;
}

void http_printf(struct mg_connection *nc, const char *fmt, ...)
{
    va_list argptr;
    va_start(argptr,fmt);
    mg_printf(nc, fmt, argptr);   
    va_end(argptr);
}

static void http_printf_with_status(struct mg_connection *nc, int http_status, const char *content_type_header, const char *fmt, ...)
{
    char buffer[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer,sizeof(buffer), fmt, args);
    va_end(args);    

    /*  send response */
    mg_send_head(nc, http_status, strlen(buffer), content_type_header);
    mg_printf(nc, "%s", buffer);
}

void http_output_runtime_response(struct mg_connection *nc, response_t *obj)
{
    // COAP status to HTTP status
    last_response_status = coap_to_http_status(obj->status);  

    last_response_str = http_attr_container_to_str(obj->payload, obj->fmt, obj->payload_len);

    if (last_response_str == NULL) {
        http_printf(nc, "%s", "HTTP/1.1 500 Internal Server Error\r\n\r\n");
        return;
    }
}

static char *http_attr_container_to_str(attr_container_t *payload, int format, int payload_len)
{
    cJSON *json = NULL;
    char *json_str = NULL;

    if (format != FMT_ATTR_CONTAINER || payload == NULL || payload_len <= 0)
        return NULL;

    if ((json = attr2json(payload)) == NULL)
        return NULL;

    if ((json_str = cJSON_Print(json)) == NULL) {
        cJSON_Delete(json);
        return NULL;
    }
    
    cJSON_Delete(json);
    return json_str; // must be freed 
}


