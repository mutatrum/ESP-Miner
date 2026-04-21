#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "websocket_api.h"
#include "websocket.h"
#include "http_server.h"
#include "global_state.h"
#include "connect.h"
#include "power.h"
#include "system_api_json.h"

#define WEBSOCKET_API_RATE_LIMIT_MS 500

static const char *TAG = "websocket_api";
static GlobalState *GLOBAL_STATE = NULL;

// Snapshot of high-frequency dynamic values for diff comparison
typedef struct {
    float power;
    float voltage;
    float current;
    float temp;
    float temp2;
    float vrTemp;
    float hashRate;
    float hashRate_1m;
    float hashRate_10m;
    float hashRate_1h;
    float errorPercentage;
    float expectedHashrate;

    uint64_t sharesAccepted;
    uint64_t sharesRejected;
    uint64_t bestDiff;
    uint64_t bestSessionDiff;
    double poolDifficulty;
    float responseTime;
    float processTime;
    uint64_t uptimeSeconds;

    uint32_t free_heap;
    uint32_t freeHeapInternal;
    uint32_t freeHeapSpiram;
    int8_t wifi_rssi;
    float cpuUsage;
    bool miningPaused;
} ws_api_snapshot_t;

static void take_snapshot(ws_api_snapshot_t *snapshot, GlobalState *g)
{
    // Power & Temps
    snapshot->power = g->POWER_MANAGEMENT_MODULE.power;
    snapshot->voltage = g->POWER_MANAGEMENT_MODULE.voltage;
    snapshot->current = g->POWER_MANAGEMENT_MODULE.current;
    snapshot->temp = g->POWER_MANAGEMENT_MODULE.chip_temp_avg;
    snapshot->temp2 = g->POWER_MANAGEMENT_MODULE.chip_temp2_avg;
    snapshot->vrTemp = g->POWER_MANAGEMENT_MODULE.vr_temp;
    snapshot->expectedHashrate = g->POWER_MANAGEMENT_MODULE.expected_hashrate;

    // Mining / Hashrate
    snapshot->hashRate = g->SYSTEM_MODULE.current_hashrate;
    snapshot->hashRate_1m = g->SYSTEM_MODULE.hashrate_1m;
    snapshot->hashRate_10m = g->SYSTEM_MODULE.hashrate_10m;
    snapshot->hashRate_1h = g->SYSTEM_MODULE.hashrate_1h;
    snapshot->errorPercentage = g->SYSTEM_MODULE.error_percentage;

    // Shares & Performance
    snapshot->sharesAccepted = g->SYSTEM_MODULE.shares_accepted;
    snapshot->sharesRejected = g->SYSTEM_MODULE.shares_rejected;
    snapshot->bestDiff = g->SYSTEM_MODULE.best_nonce_diff;
    snapshot->bestSessionDiff = g->SYSTEM_MODULE.best_session_nonce_diff;
    snapshot->poolDifficulty = g->pool_difficulty;
    snapshot->responseTime = g->SYSTEM_MODULE.response_time;
    snapshot->processTime = g->SYSTEM_MODULE.process_time;
    snapshot->uptimeSeconds = (uint32_t)((esp_timer_get_time() - g->SYSTEM_MODULE.start_time) / 1000000);

    // Dynamic System Stats
    snapshot->free_heap = esp_get_free_heap_size();
    snapshot->freeHeapInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    snapshot->freeHeapSpiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    int8_t rssi = -90;
    get_wifi_current_rssi(&rssi);
    snapshot->wifi_rssi = rssi;
    
    snapshot->cpuUsage = g->SYSTEM_MODULE.cpu_usage;
    snapshot->miningPaused = g->SYSTEM_MODULE.mining_paused;
}

static cJSON* build_diff(ws_api_snapshot_t *old, ws_api_snapshot_t *new, uint32_t bits, GlobalState *g) {
    bool force_all = (old == NULL);
    
    // If full state requested (on connect), use the shared system API helper
    if (force_all) {
        return system_api_get_full_json(g);
    }

    cJSON *data = cJSON_CreateObject();
    bool changed = false;

    // Telemetry Diffing
    if (bits & WS_EVENT_POWER_UPDATED) {
        if (old->power != new->power) { cJSON_AddFloatToObject(data, "power", new->power); changed = true; }
        if (old->voltage != new->voltage) { cJSON_AddFloatToObject(data, "voltage", new->voltage); changed = true; }
        if (old->current != new->current) { cJSON_AddFloatToObject(data, "current", new->current); changed = true; }
        if (old->temp != new->temp) { cJSON_AddFloatToObject(data, "temp", new->temp); changed = true; }
        if (old->temp2 != new->temp2) { cJSON_AddFloatToObject(data, "temp2", new->temp2); changed = true; }
        if (old->vrTemp != new->vrTemp) { cJSON_AddFloatToObject(data, "vrTemp", new->vrTemp); changed = true; }
        if (old->expectedHashrate != new->expectedHashrate) { cJSON_AddFloatToObject(data, "expectedHashrate", new->expectedHashrate); changed = true; }
    }

    if (bits & (WS_EVENT_HASHRATE_UPDATED | WS_EVENT_STRATUM_UPDATED)) {
        if (old->hashRate != new->hashRate) { cJSON_AddFloatToObject(data, "hashRate", new->hashRate); changed = true; }
        if (old->hashRate_1m != new->hashRate_1m) { cJSON_AddFloatToObject(data, "hashRate_1m", new->hashRate_1m); changed = true; }
        if (old->hashRate_10m != new->hashRate_10m) { cJSON_AddFloatToObject(data, "hashRate_10m", new->hashRate_10m); changed = true; }
        if (old->hashRate_1h != new->hashRate_1h) { cJSON_AddFloatToObject(data, "hashRate_1h", new->hashRate_1h); changed = true; }
        if (old->errorPercentage != new->errorPercentage) { cJSON_AddFloatToObject(data, "errorPercentage", new->errorPercentage); changed = true; }
        
        if (bits & WS_EVENT_HASHRATE_UPDATED) {
            system_api_add_telemetry(data, g); // Re-sync common telemetry bits
            system_api_add_hashrate_monitor(data, g); // Restore live heatmap
            changed = true;
        }
    }

    if (bits & WS_EVENT_STRATUM_UPDATED) {
        if (old->sharesAccepted != new->sharesAccepted) { cJSON_AddNumberToObject(data, "sharesAccepted", new->sharesAccepted); changed = true; }
        if (old->sharesRejected != new->sharesRejected) { cJSON_AddNumberToObject(data, "sharesRejected", new->sharesRejected); changed = true; }
        if (old->bestDiff != new->bestDiff) { cJSON_AddNumberToObject(data, "bestDiff", new->bestDiff); changed = true; }
        if (old->bestSessionDiff != new->bestSessionDiff) { cJSON_AddNumberToObject(data, "bestSessionDiff", new->bestSessionDiff); changed = true; }
        if (old->poolDifficulty != new->poolDifficulty) { cJSON_AddNumberToObject(data, "poolDifficulty", new->poolDifficulty); changed = true; }
        if (old->responseTime != new->responseTime) { cJSON_AddFloatToObject(data, "responseTime", new->responseTime); changed = true; }
        if (old->processTime != new->processTime) { cJSON_AddFloatToObject(data, "processTime", new->processTime); changed = true; }
    }

    // Settings / Config Group Relay (No per-field diffing for rarely changed items)
    if (bits & (WS_EVENT_SYSTEM_UPDATED | WS_EVENT_STRATUM_UPDATED)) {
        system_api_add_config(data, g);
        changed = true;
    }

    // Dynamic System Stats Diffing
    if (bits & WS_EVENT_SYSTEM_UPDATED) {
        if (old->free_heap != new->free_heap) { cJSON_AddNumberToObject(data, "freeHeap", new->free_heap); changed = true; }
        if (old->freeHeapInternal != new->freeHeapInternal) { cJSON_AddNumberToObject(data, "freeHeapInternal", new->freeHeapInternal); changed = true; }
        if (old->freeHeapSpiram != new->freeHeapSpiram) { cJSON_AddNumberToObject(data, "freeHeapSpiram", new->freeHeapSpiram); changed = true; }
        if (old->wifi_rssi != new->wifi_rssi) { cJSON_AddNumberToObject(data, "wifiRSSI", new->wifi_rssi); changed = true; }
        if (old->uptimeSeconds != new->uptimeSeconds) { cJSON_AddNumberToObject(data, "uptimeSeconds", new->uptimeSeconds); changed = true; }
        if (old->cpuUsage != new->cpuUsage) { cJSON_AddFloatToObject(data, "cpuUsage", new->cpuUsage); changed = true; }
        if (old->miningPaused != new->miningPaused) { cJSON_AddBoolToObject(data, "miningPaused", new->miningPaused); changed = true; }
    }

    if (!changed) {
        cJSON_Delete(data);
        return NULL;
    }

    return data;
}

static void send_api_update(ws_api_snapshot_t *old, ws_api_snapshot_t *new, uint32_t bits, int fd)
{
    cJSON *data = build_diff(old, new, bits, GLOBAL_STATE);
    if (data == NULL) return;

    cJSON *msg = cJSON_CreateObject();
    if (msg == NULL) {
        cJSON_Delete(data);
        return;
    }

    cJSON_AddStringToObject(msg, "event", "update");
    cJSON_AddItemToObject(msg, "data", data);

    char *json_str = cJSON_PrintUnformatted(msg);
    if (json_str != NULL) {
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.payload = (uint8_t *)json_str;
        ws_pkt.len = strlen(json_str);
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;

        if (fd == -1) {
            websocket_broadcast(WS_TYPE_API, &ws_pkt);
        } else {
            websocket_send_to_client(fd, &ws_pkt);
        }
        free(json_str);
    } else {
        ESP_LOGE(TAG, "Failed to print JSON update");
    }

    cJSON_Delete(msg);
}

void websocket_api_on_connect(int fd)
{
    if (GLOBAL_STATE == NULL) {
        ESP_LOGW(TAG, "Cannot send initial state, GLOBAL_STATE not yet initialized");
        return;
    }

    ws_api_snapshot_t current_snapshot;
    take_snapshot(&current_snapshot, GLOBAL_STATE);
    send_api_update(NULL, &current_snapshot, WS_EVENT_ALL, fd);
}

void websocket_api_task(void *pvParameters)
{
    GLOBAL_STATE = (GlobalState *)pvParameters;
    ESP_LOGI(TAG, "websocket_api_task starting");

    // Wait until network is connected before proceeding
    while (!GLOBAL_STATE->SYSTEM_MODULE.is_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ws_api_snapshot_t last_snapshot;
    memset(&last_snapshot, 0, sizeof(last_snapshot));
    take_snapshot(&last_snapshot, GLOBAL_STATE); // Initialize last_snapshot

    while (true) {
        // Wait for notification from producer tasks
        uint32_t bits = xEventGroupWaitBits(GLOBAL_STATE->ws_event_group, WS_EVENT_ALL, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        
        if (bits == 0) {
            // Wake up on timeout to update system metrics (heap, rssi, uptime)
            bits = WS_EVENT_SYSTEM_UPDATED;
        }
        
        if (websocket_get_active_client_count(WS_TYPE_API) == 0) {
            // If no active clients, just update the last snapshot and continue
            // This ensures that when a client connects, the first diff is accurate
            take_snapshot(&last_snapshot, GLOBAL_STATE);
            continue;
        }

        ws_api_snapshot_t new_snapshot;
        take_snapshot(&new_snapshot, GLOBAL_STATE);
        
        send_api_update(&last_snapshot, &new_snapshot, bits, -1);
        
        last_snapshot = new_snapshot;

        // Rate limit: prevent spamming the client if producer tasks update too frequently
        vTaskDelay(pdMS_TO_TICKS(WEBSOCKET_API_RATE_LIMIT_MS));
    }
}
