#include "lib_export.h"
#include "sensor_api.h"
#include "connection_api.h"

static NativeSymbol extended_native_symbol_defs[] = {
//#include "runtime_sensor.inl"
#include "connection.inl"

//EXPORT_WASM_API(attr_container_dump),
//EXPORT_WASM_API(api_subscribe_event)
        };

#include "ext_lib_export.h"
