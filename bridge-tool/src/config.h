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

#define STR_MAXLEN 100

typedef struct
{
    char mqtt_server_address[STR_MAXLEN];
    uint32_t mqtt_keepalive_ms;
    char mqtt_user_name[STR_MAXLEN];
    char mqtt_password[STR_MAXLEN];

    char http_port[STR_MAXLEN];
    char http_doc_root[STR_MAXLEN];
    char http_enable_directory_listing[STR_MAXLEN];

    char rt_address[STR_MAXLEN];
    uint32_t rt_port;
    uint32_t rt_reconnect_attempts;
    uint32_t rt_connection_mode;
    char rt_uart_dev[STR_MAXLEN];
    uint32_t rt_uart_baudrate;
    char rt_wasm_files_folder[STR_MAXLEN];
} bt_config_t;

extern bt_config_t g_bt_config;

int read_config();

#endif