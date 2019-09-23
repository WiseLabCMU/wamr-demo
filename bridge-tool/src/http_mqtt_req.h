/** @file http-mqtt-request.h
 *  @brief Definitions for requests received through http or mqtt
 *
 *  Definitions for requests received through http or mqtt
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
#ifndef HTTP_MQTT_REQ_H_
#define HTTP_MQTT_REQ_H_

/* request vars (http and mqtt) */
#define REQ_NAME_VAR "name"
#define REQ_FILENAME_VAR "wasm_file"
#define REQ_CMD_VAR "cmd"

// events  (mqtt)
#define FMTSTR_EVENT_RT_START_JSON "{ \"id\":\"%s\", \"label\": \"%s\", \"cmd\": \"%s\" }"
#define EVENT_RT_START "rt-start"
#define EVENT_RT_STOP "rt-stop"

#define FMTSTR_EVENT_MOD_INST_JSON "{ \"id\":\"%s\", \"label\": \"%s\", \"parent\":\"%s\", \"cmd\": \"%s\", \"module-name\": \"%s\"}"
#define EVENT_MOD_INST "module-inst"
#define EVENT_MOD_UNINST "module-uninst"

#define FMTSTR_EVENT_PUSBSUB_JSON { \"id\":\"%s\", \"label\": \"%s\", \"parent\":\"%s\", \"cmd\": \"%s\", \"topic\": \"%s\"}"
#define EVENT_PUB_START "pub-start"
#define EVENT_PUB_STOP "pub-stop"
#define EVENT_SUB_START "sub-start"
#define EVENT_SUB_STOP "sub-stop"

#endif