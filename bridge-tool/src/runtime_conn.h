/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdint.h>
#include "shared_utils.h"
#include "attr_container.h"
#include "mongoose.h"

#ifndef RUNTIME_CONN_H_
#define RUNTIME_CONN_H_

#ifdef __cplusplus
extern "C" {
#endif

#define BUF_SIZE 1024
#define TIMEOUT_EXIT_CODE -2
#define URL_MAX_LEN 256
#define DEFAULT_TIMEOUT_MS 5000
#define DEFAULT_ALIVE_TIME_MS 0

#define CONNECTION_MODE_TCP 1
#define CONNECTION_MODE_UART 2

#define RESPONSE_OUTPUT_CONSOLE 0
#define RESPONSE_OUTPUT_HTTP 1

typedef enum REPLY_PACKET_TYPE {
    REPLY_TYPE_EVENT = 0, REPLY_TYPE_RESPONSE = 1
} REPLY_PACKET_TYPE;

/* IMRT link message between host and WAMR */
typedef struct {
    unsigned short message_type;
    unsigned long payload_size;
    char *payload;
} imrt_link_message_t;

/* The receive phase of IMRT link message */
typedef enum {
    Phase_Non_Start, Phase_Leading, Phase_Type, Phase_Size, Phase_Payload
} recv_phase_t;

/* The receive context of IMRT link message */
typedef struct {
    recv_phase_t phase;
    int size_in_phase;
    imrt_link_message_t message;
} imrt_link_recv_context_t;

/**
 * @brief Send data to WAMR.
 *
 * @param fd the connection fd to WAMR
 * @param buf the buffer that contains content to be sent
 * @param len size of the buffer to be sent
 *
 * @return true if success, false if fail
 */
bool host_tool_send_data(const char *buf, unsigned int len);

/**
 * @brief Handle one byte of IMRT link message
 *
 * @param ch the one byte from WAMR to be handled
 * @param ctx the receive context
 *
 * @return -1 invalid sync byte
 *          1 byte added to buffer, waiting more for complete packet
 *          0 completed packet
 *          2 in receiving payload
 */
int on_imrt_link_byte_arrive(unsigned char ch, imrt_link_recv_context_t *ctx);

/**
 * @brief Initialize TCP connection with remote server.
 *
 * @param address the network address of peer
 * @param port the network port of peer
 * @param fd pointer of integer to save the socket fd once return success
 *
 * @return true if success, false if fail
 */
bool tcp_init(const char *address, uint16_t port, int *fd);

/**
 * @brief Initialize UART connection with remote.
 *
 * @param device name of the UART device
 * @param baudrate baudrate of the device
 * @param fd pointer of integer to save the uart fd once return success
 *
 * @return true if success, false if fail
 */
bool uart_init(const char *device, int baudrate, int *fd);

/**
 * @brief Parse UART baudrate from an integer
 *
 * @param the baudrate interger to be parsed
 *
 * @return true if success, false if fail
 *
 * @par
 * @code
 * int baudrate = parse_baudrate(9600);
 * ...
 * uart_term.c_cflag = baudrate;
 * ...
 * @endcode
 */
int parse_baudrate(int baud);

/**
 * @brief Send data over UDP.
 *
 * @param address network address of the remote
 * @param port network port of the remote
 * @param buf the buffer that contains content to be sent
 * @param len size of the buffer to be sent
 *
 * @return true if success, false if fail
 */
bool udp_send(const char *address, int port, const char *buf, int len);

/**
 * @brief Init connetion to runtime
 *
 */
int runtime_conn_init();

/**
 * @brief Close connetion to runtime
 *
 */
void runtime_conn_close();

/**
 * @brief Process data received from runtime
 *
 */
int process_rcvd_data(const char *buf, int len, imrt_link_recv_context_t *ctx);

/**
 * @brief Parse data received from runtime
 *
 */
response_t *parse_response_from_imrtlink(imrt_link_message_t *message, response_t *response);

/**
 * @brief Parse event data received from runtime
 *
 */
request_t *parse_event_from_imrtlink(imrt_link_message_t *message, request_t *request);

/**
 * @brief Output a response received from the runtime
 *
 */
void output_response(response_t *obj);

/**
 * @brief Output a payload received from the runtime
 *
 */
void output(const char *header, attr_container_t *payload, int foramt,int payload_len);

/**
 * Reads response from runtime. If connection to rest client is given, forwards the response to it
 * 
 * @param http_mg_conn the connection to rest client; if !=NULL outputs to the connection; if =NULL outputs to console
 * @return returns -1 on error, -2 on timeout; >=0 status code
 */
int get_request_response(struct mg_connection *http_mg_conn);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* DEPS_APP_MGR_HOST_TOOL_SRC_TRANSPORT_H_ */
