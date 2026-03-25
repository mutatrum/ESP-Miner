#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "websocket.h"
#include "websocket_log.h"

static const char *TAG = "websocket_log";
static QueueHandle_t log_queue = NULL;

int websocket_log_to_queue(const char *format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    // Calculate the required buffer size +1 for \n
    int needed_size = vsnprintf(NULL, 0, format, args_copy) + 1;
    va_end(args_copy);

    // Allocate the buffer dynamically
    char *log_buffer = (char *)calloc(needed_size, sizeof(char));
    if (log_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for log buffer");
        return 0;
    }

    // Format the string into the allocated buffer
    va_copy(args_copy, args);
    vsnprintf(log_buffer, needed_size, format, args_copy);
    va_end(args_copy);

    // Ensure the log message ends with a newline
    size_t len = strlen(log_buffer);
    if (len > 0 && log_buffer[len - 1] != '\n') {
        log_buffer[len] = '\n';
        log_buffer[len + 1] = '\0';
    }

    // Print to standard output
    fputs(log_buffer, stdout);

    // Send to queue for WebSocket broadcasting
    if (log_queue != NULL) {
        if (xQueueSendToBack(log_queue, &log_buffer, pdMS_TO_TICKS(100)) != pdPASS) {
            ESP_LOGW(TAG, "Failed to send log to queue, freeing buffer");
            free(log_buffer);
        }
    } else {
        free(log_buffer);
    }

    return 0;
}

void websocket_log_task(void *pvParameters)
{
    ESP_LOGI(TAG, "websocket_log_task starting");

    log_queue = xQueueCreateWithCaps(MESSAGE_QUEUE_SIZE, sizeof(char*), MALLOC_CAP_SPIRAM);
    if (log_queue == NULL) {
        ESP_LOGE(TAG, "Error creating log queue");
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        char *message;
        if (xQueueReceive(log_queue, &message, pdMS_TO_TICKS(1000)) != pdPASS) {
            continue;
        }

        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.payload = (uint8_t *)message;
        ws_pkt.len = strlen(message);
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;

        websocket_broadcast(WS_TYPE_LOGS, &ws_pkt);

        free(message);
    }
}
