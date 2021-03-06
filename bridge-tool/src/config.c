 /** @file config.c
 *  @brief Read configuration
 * 
 *  Read configuration from .ini file
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>  

#include "runtime_conn.h"
#include "config.h"

bt_config_t g_bt_config;

static const char g_config_file_path[] = "config.ini";

static int conf_handler(void* user, const char* section, const char* name,
                   const char* value)
{
    bt_config_t* pconfig = (bt_config_t*)user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("mqtt", "server_address")) {
        strncpy(pconfig->mqtt_server_address, value, sizeof(pconfig->mqtt_server_address));
        printf("mqtt_server_address = %s\n", pconfig->mqtt_server_address);
    } else if (MATCH("mqtt", "keepalive_ms")) {
        pconfig->mqtt_keepalive_ms = atol(value);
        printf("mqtt_keepalive_ms = %u\n", pconfig->mqtt_keepalive_ms);    
    } else if (MATCH("mqtt", "user-name")) {
        strncpy(pconfig->mqtt_user_name, value, sizeof(pconfig->mqtt_user_name));
        printf("mqtt_user_name = %s\n", pconfig->mqtt_user_name);
    } else if (MATCH("mqtt", "password")) {
        strncpy(pconfig->mqtt_password, value, sizeof(pconfig->mqtt_password));
        printf("mqtt_password = %s\n", pconfig->mqtt_password);
    } else if (MATCH("http", "port")) {
        strncpy(pconfig->http_port, value, sizeof(pconfig->http_port));
        printf("http_port = %s\n", pconfig->http_port);    
    } else if (MATCH("http", "doc-root")) {
        strncpy(pconfig->http_doc_root, value, sizeof(pconfig->http_doc_root));
        printf("http_doc_root = %s\n", pconfig->http_doc_root);    
    } else if (MATCH("http", "enable-directory-listing")) {
        strncpy(pconfig->http_enable_directory_listing, value, sizeof(pconfig->http_enable_directory_listing));
        printf("http_enable_directory_listing = %s\n", pconfig->http_enable_directory_listing);   
    } else if (MATCH("runtime", "address")) {
        strncpy(pconfig->rt_address, value, sizeof(pconfig->rt_address));
        printf("rt_address = %s\n", pconfig->rt_address);
    } else if (MATCH("runtime", "port")) {
        pconfig->rt_port = atol(value);
        printf("rt_port = %u\n", pconfig->rt_port);
    } else if (MATCH("runtime", "reconnect-attempts")) {
        pconfig->rt_reconnect_attempts = atol(value);
        printf("rt_reconnect_attempts = %u\n", pconfig->rt_reconnect_attempts);
    } else if (MATCH("runtime", "connection-mode")) {
        pconfig->rt_connection_mode = CONNECTION_MODE_TCP; // default mode
        if (strncmp(value, "CONNECTION_MODE_UART", strlen("CONNECTION_MODE_UART")) == 0) pconfig->rt_connection_mode = CONNECTION_MODE_UART;
        printf("rt_connection_mode = %u (CONNECTION_MODE_TCP=%d; CONNECTION_MODE_UART=%d)\n", pconfig->rt_connection_mode, CONNECTION_MODE_TCP, CONNECTION_MODE_UART);
    } else if (MATCH("runtime", "uart-dev")) {
        strncpy(pconfig->rt_uart_dev, value, sizeof(pconfig->rt_uart_dev));
        printf("rt_uart_dev = %s\n", pconfig->rt_uart_dev);
    } else if (MATCH("runtime", "uart-baudrate")) {
        pconfig->rt_uart_baudrate =  atol(value);
        printf("rt_uart_baudrate = %u\n", pconfig->rt_uart_baudrate);
    } else if (MATCH("runtime", "wasm-files-folder")) {
        strncpy(pconfig->rt_wasm_files_folder, value, sizeof(pconfig->rt_wasm_files_folder));
        printf("rt_wasm_files_folder = %s\n", pconfig->rt_wasm_files_folder);
    } else if (MATCH("runtime", "topic-prefix")) {
        strncpy(pconfig->rt_topic_prefix, value, sizeof(pconfig->rt_topic_prefix));
        printf("rt_topic_prefix = %s\n", pconfig->rt_topic_prefix);
    } else if (MATCH("runtime", "uuid")) {
        strncpy(pconfig->rt_uuid, value, sizeof(pconfig->rt_uuid));
        printf("rt_uuid = %s\n", pconfig->rt_uuid);
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

int read_config() {
    if (ini_parse(g_config_file_path, conf_handler, &g_bt_config) < 0) {
        printf("Can't load 'config.ini'\n");
        return -1;
    }   
    return 0;
}