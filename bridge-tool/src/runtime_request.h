#ifndef RUNTIME_REQ_H_
#define RUNTIME_REQ_H_

#include "app_manager_export.h" /* for Module_WASM_App */
#include "cJSON.h"

typedef enum {
    INSTALL, UNINSTALL, QUERY, REQUEST, REGISTER, UNREGISTER
} op_type;

/* Package Type */
typedef enum {
    Wasm_Module_Bytecode = 0, Wasm_Module_AoT, Package_Type_Unknown = 0xFFFF
} PackageType;

int install(char *filename, char *app_file_buf, int app_size, char *name, char *module_type, int heap_size, int timers, int watchdog_interval);
int uninstall(char *name, char *module_type);
int query(char *name);
int request(char *url, int action, cJSON *json);
int subscribe(char *urls);
int unsubscribe(char *urls);
int send_request(request_t *request, bool is_install_wasm_bytecode_app);

PackageType get_package_type(const char *buf, int size);

#endif