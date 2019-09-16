
#include "attr_container.h"
#include "request.h"
#include "shared_utils.h"
#include "wasm_app.h"
#include "mqtt-pubsub.h"

bool mqtt_publish(const char *url, int fmt, void *payload, int payload_len)
{
    int size;
    request_t request[1];
    init_request(request, (char *)url, COAP_EVENT_PUB, fmt, payload, payload_len); // publishes a COAP_EVENT_PUB (api_publish_event publishes a )
    char * buffer = pack_request(request, &size);
    if (buffer == NULL)
        return false;
    wasm_post_request((int32)buffer, size);

    free_req_resp_packet(buffer);

    return true;
}

bool mqtt_subscribe(const char * topic, request_handler_f handler)
{
    const char arena_suffix[] = "/arena/";
    char url_buf[256];
    int i=strlen(arena_suffix);
    strncpy(url_buf, arena_suffix, i);
    while (*topic != '\0' && i<sizeof(url_buf)-1) url_buf[i++] = *(topic++);
    url_buf[i++] = '\0';
    printf("registering %s:", url_buf);
    return register_url_handler(url_buf, handler, Reg_Event_Arena);
}

