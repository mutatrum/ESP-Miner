#ifndef WEBSOCKET_H_
#define WEBSOCKET_H_

#include "esp_err.h"
#include "esp_http_server.h"

#define MESSAGE_QUEUE_SIZE (128)
#define MAX_WEBSOCKET_CLIENTS (10)

typedef enum {
    WS_TYPE_LOGS,
    WS_TYPE_API
} WebSocketClientType;

esp_err_t websocket_add_client(int fd, WebSocketClientType type);
void websocket_remove_client(int fd);
void websocket_broadcast(WebSocketClientType type, httpd_ws_frame_t *pkt);
int websocket_get_active_client_count(WebSocketClientType type);
void websocket_init(httpd_handle_t server);
esp_err_t websocket_handler(httpd_req_t *req);

void websocket_close_fn(httpd_handle_t hd, int sockfd);

#endif /* WEBSOCKET_H_ */
