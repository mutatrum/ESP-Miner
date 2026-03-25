#ifndef WEBSOCKET_LOG_H
#define WEBSOCKET_LOG_H

#include <stdarg.h>

void websocket_log_task(void *pvParameters);
int websocket_log_to_queue(const char *format, va_list args);

#endif // WEBSOCKET_LOG_H
