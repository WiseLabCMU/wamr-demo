 /** @file http.h
 *  @brief Definitions reading configuration 
 *
 *  Definitions reading configuration from a .ini file
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
#ifndef BT_CONFIG_H_
#define BT_CONFIG_H_

#include <stdint.h>
#include "ini.h"

typedef struct
{
    const char* mqtt_server;
    uint32_t mqtt_port;
    uint32_t mqtt_keepalive_ms;
    const char* mqtt_user_name;
    const char* mqtt_password;

    uint32_t http_port;
    const char* http_doc_root;
    const char* http_enable_directory_listing;

    const char* rt_address;
    uint32_t rt_port;
    uint32_t rt_reconnect_attempts;
    uint32_t rt_connection_mode;
    const char *rt_uart_dev;
    uint32_t rt_uart_baudrate;
    const char* rt_wasm_files_folder;
} bt_config_t;

extern bt_config_t g_bt_config;

int read_config();

#endif