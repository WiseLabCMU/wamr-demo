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

// commands received  (mqtt)
#define FMTSTR_RT_CMD_RT_START_JSON "{ \"id\":\"%s\", \"cmd\": \"%s\" }"
#define RTCMD_RT_START "rt-start"
#define RTCMD_RT_STOP "rt-stop"
#define FMTSTR_RT_CMD_MOD_INST_JSON "{ \"id\":\"%s\", \"cmd\": \"%s\" \"module-name\": \"%s\"}"
#define RTCMD_MOD_INST "module-inst"
#define RTCMD_MOD_UNINST "module-uninst"

#endif