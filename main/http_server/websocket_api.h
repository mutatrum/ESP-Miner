#ifndef WEBSOCKET_API_H_
#define WEBSOCKET_API_H_

#include "esp_err.h"
#include "esp_http_server.h"

#define MAX_LIVE_CLIENTS (4)

esp_err_t websocket_api_handler(httpd_req_t *req);
void websocket_api_init(void *global_state);
void websocket_api_task(void *pvParameters);
void websocket_api_close_fn(httpd_handle_t hd, int sockfd);

#endif /* WEBSOCKET_API_H_ */
