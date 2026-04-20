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
#include "nvs_config.h"

#define WEBSOCKET_API_RATE_LIMIT_MS 500

static const char *TAG = "websocket_api";
static GlobalState *GLOBAL_STATE = NULL;

// Snapshot of dynamic values for diff comparison
typedef struct {
    // Power management
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

    // Shares and Difficulty
    uint64_t sharesAccepted;
    uint64_t sharesRejected;
    uint64_t bestDiff;
    uint64_t bestSessionDiff;
    double poolDifficulty;
    float responseTime;
    float processTime;
    uint64_t uptimeSeconds;

    // Block Header
    int blockHeight;
    uint64_t networkDifficulty;
    char scriptsig[128];
    uint64_t coinbaseValueTotalSatoshis;
    uint64_t coinbaseValueUserSatoshis;
    int blockFound;
    bool showNewBlock;

    // System
    uint16_t power_fault;
    uint32_t free_heap;
    int8_t wifi_rssi;
    char wifi_status[256];
    char pool_connection_info[64];
    bool is_using_fallback;
    float core_voltage;
    float frequency_value;
    float actual_frequency;
    float fan_perc;
    uint16_t fan_rpm;
    uint16_t fan2_rpm;
    uint16_t stats_frequency;
    float cpuUsage;
    bool miningPaused;
    bool overheatMode;
    bool hardwareFault;
    char hardwareFaultMsg[64];
    int screenPage;

    // Pool details
    char stratumURL[128];
    char fallbackStratumURL[128];
    char stratumUser[64];
    char fallbackStratumUser[64];
    uint16_t stratumPort;
    uint16_t fallbackStratumPort;
} ws_api_snapshot_t;

static void take_snapshot(ws_api_snapshot_t *snapshot, GlobalState *g)
{
    // Power management
    snapshot->power = g->POWER_MANAGEMENT_MODULE.power;
    snapshot->voltage = g->POWER_MANAGEMENT_MODULE.voltage;
    snapshot->current = g->POWER_MANAGEMENT_MODULE.current;
    snapshot->temp = g->POWER_MANAGEMENT_MODULE.chip_temp_avg;
    snapshot->temp2 = g->POWER_MANAGEMENT_MODULE.chip_temp2_avg;
    snapshot->vrTemp = g->POWER_MANAGEMENT_MODULE.vr_temp;
    snapshot->core_voltage = g->POWER_MANAGEMENT_MODULE.core_voltage;
    snapshot->frequency_value = g->POWER_MANAGEMENT_MODULE.frequency_value;
    snapshot->actual_frequency = g->POWER_MANAGEMENT_MODULE.actual_frequency;
    snapshot->expectedHashrate = g->POWER_MANAGEMENT_MODULE.expected_hashrate;
    snapshot->fan_perc = g->POWER_MANAGEMENT_MODULE.fan_perc;
    snapshot->fan_rpm = g->POWER_MANAGEMENT_MODULE.fan_rpm;
    snapshot->fan2_rpm = g->POWER_MANAGEMENT_MODULE.fan2_rpm;

    // Mining / hashrate
    snapshot->hashRate = g->SYSTEM_MODULE.current_hashrate;
    snapshot->hashRate_1m = g->SYSTEM_MODULE.hashrate_1m;
    snapshot->hashRate_10m = g->SYSTEM_MODULE.hashrate_10m;
    snapshot->hashRate_1h = g->SYSTEM_MODULE.hashrate_1h;
    snapshot->errorPercentage = g->SYSTEM_MODULE.error_percentage;

    // Shares and Difficulty
    snapshot->sharesAccepted = g->SYSTEM_MODULE.shares_accepted;
    snapshot->sharesRejected = g->SYSTEM_MODULE.shares_rejected;
    snapshot->bestDiff = g->SYSTEM_MODULE.best_nonce_diff;
    snapshot->bestSessionDiff = g->SYSTEM_MODULE.best_session_nonce_diff;
    snapshot->poolDifficulty = g->pool_difficulty;
    snapshot->responseTime = g->SYSTEM_MODULE.response_time;
    strncpy(snapshot->pool_connection_info, g->SYSTEM_MODULE.pool_connection_info, sizeof(snapshot->pool_connection_info) - 1);
    snapshot->pool_connection_info[sizeof(snapshot->pool_connection_info) - 1] = '\0';
    snapshot->is_using_fallback = g->SYSTEM_MODULE.is_using_fallback;

    // Block Header
    snapshot->blockHeight = g->block_height;
    snapshot->networkDifficulty = g->network_nonce_diff;
    strncpy(snapshot->scriptsig, g->scriptsig, sizeof(snapshot->scriptsig));
    snapshot->scriptsig[sizeof(snapshot->scriptsig) - 1] = '\0';
    snapshot->coinbaseValueTotalSatoshis = g->coinbase_value_total_satoshis;
    snapshot->coinbaseValueUserSatoshis = g->coinbase_value_user_satoshis;
    snapshot->blockFound = g->SYSTEM_MODULE.block_found;
    snapshot->showNewBlock = g->SYSTEM_MODULE.show_new_block;

    // System
    snapshot->power_fault = g->SYSTEM_MODULE.power_fault;
    snapshot->uptimeSeconds = (uint32_t)((esp_timer_get_time() - g->SYSTEM_MODULE.start_time) / 1000000);
    snapshot->free_heap = (uint32_t)esp_get_free_heap_size();

    int8_t rssi = -90;
    get_wifi_current_rssi(&rssi);
    snapshot->wifi_rssi = rssi;

    strncpy(snapshot->wifi_status, g->SYSTEM_MODULE.wifi_status, sizeof(snapshot->wifi_status) - 1);
    snapshot->wifi_status[sizeof(snapshot->wifi_status) - 1] = '\0';

    snapshot->stats_frequency = nvs_config_get_u16(NVS_CONFIG_STATISTICS_FREQUENCY);

    snapshot->cpuUsage = g->SYSTEM_MODULE.cpu_usage;
    snapshot->processTime = g->SYSTEM_MODULE.process_time;
    snapshot->miningPaused = g->SYSTEM_MODULE.mining_paused;
    snapshot->overheatMode = g->SYSTEM_MODULE.overheat_mode;
    snapshot->hardwareFault = g->SYSTEM_MODULE.hardware_fault;
    strncpy(snapshot->hardwareFaultMsg, g->SYSTEM_MODULE.hardware_fault_msg, sizeof(snapshot->hardwareFaultMsg) - 1);
    snapshot->hardwareFaultMsg[sizeof(snapshot->hardwareFaultMsg) - 1] = '\0';
    snapshot->screenPage = g->SYSTEM_MODULE.screen_page;

    // Pool details
    strncpy(snapshot->stratumURL, g->SYSTEM_MODULE.pool_url ? g->SYSTEM_MODULE.pool_url : "", sizeof(snapshot->stratumURL) - 1);
    snapshot->stratumURL[sizeof(snapshot->stratumURL) - 1] = '\0';
    strncpy(snapshot->fallbackStratumURL, g->SYSTEM_MODULE.fallback_pool_url ? g->SYSTEM_MODULE.fallback_pool_url : "", sizeof(snapshot->fallbackStratumURL) - 1);
    snapshot->fallbackStratumURL[sizeof(snapshot->fallbackStratumURL) - 1] = '\0';
    strncpy(snapshot->stratumUser, g->SYSTEM_MODULE.pool_user ? g->SYSTEM_MODULE.pool_user : "", sizeof(snapshot->stratumUser) - 1);
    snapshot->stratumUser[sizeof(snapshot->stratumUser) - 1] = '\0';
    strncpy(snapshot->fallbackStratumUser, g->SYSTEM_MODULE.fallback_pool_user ? g->SYSTEM_MODULE.fallback_pool_user : "", sizeof(snapshot->fallbackStratumUser) - 1);
    snapshot->fallbackStratumUser[sizeof(snapshot->fallbackStratumUser) - 1] = '\0';
    snapshot->stratumPort = g->SYSTEM_MODULE.pool_port;
    snapshot->fallbackStratumPort = g->SYSTEM_MODULE.fallback_pool_port;
}

static void add_hashrate_monitor(cJSON *root, GlobalState *g) {
    if (!g->HASHRATE_MONITOR_MODULE.is_initialized) return;

    cJSON *hashrate_monitor = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "hashrateMonitor", hashrate_monitor);
    
    cJSON *asics_array = cJSON_CreateArray();
    cJSON_AddItemToObject(hashrate_monitor, "asics", asics_array);

    for (int asic_nr = 0; asic_nr < g->DEVICE_CONFIG.family.asic_count; asic_nr++) {
        cJSON *asic = cJSON_CreateObject();
        cJSON_AddItemToArray(asics_array, asic);
        cJSON_AddFloatToObject(asic, "total", g->HASHRATE_MONITOR_MODULE.total_measurement[asic_nr].hashrate);

        int hash_domains = g->DEVICE_CONFIG.family.asic.hash_domains;
        cJSON* hash_domain_array = cJSON_CreateArray();
        for (int domain_nr = 0; domain_nr < hash_domains; domain_nr++) {
            cJSON_AddItemToArray(hash_domain_array, cJSON_CreateFloat(g->HASHRATE_MONITOR_MODULE.domain_measurements[asic_nr][domain_nr].hashrate));
        }
        cJSON_AddItemToObject(asic, "domains", hash_domain_array);
        cJSON_AddNumberToObject(asic, "errorCount", g->HASHRATE_MONITOR_MODULE.error_measurement[asic_nr].value);
    }
}

static void add_block_header_arrays(cJSON *root, GlobalState *g) {
    if (g->block_height <= 0) return;

    cJSON *block_signals_array = cJSON_CreateArray();
    for (int i = 0; i < g->block_signals_count; i++) {
        cJSON_AddItemToArray(block_signals_array, cJSON_CreateString(g->block_signals[i]));
    }
    cJSON_AddItemToObject(root, "blockSignals", block_signals_array);

    cJSON *outputs_array = cJSON_CreateArray();
    for (int i = 0; i < g->coinbase_output_count; i++) {
        cJSON *output_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(output_obj, "value", g->coinbase_outputs[i].value_satoshis);
        cJSON_AddStringToObject(output_obj, "address", g->coinbase_outputs[i].address);
        cJSON_AddItemToArray(outputs_array, output_obj);
    }
    cJSON_AddItemToObject(root, "coinbaseOutputs", outputs_array);
}

static void add_rejected_reasons(cJSON *root, GlobalState *g) {
    cJSON *error_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "sharesRejectedReasons", error_array);
    
    for (int i = 0; i < g->SYSTEM_MODULE.rejected_reason_stats_count; i++) {
        cJSON *error_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(error_obj, "message", g->SYSTEM_MODULE.rejected_reason_stats[i].message);
        cJSON_AddNumberToObject(error_obj, "count", g->SYSTEM_MODULE.rejected_reason_stats[i].count);
        cJSON_AddItemToArray(error_array, error_obj);
    }
}

static cJSON* build_diff(ws_api_snapshot_t *old, ws_api_snapshot_t *new, uint32_t bits, GlobalState *g) {
    cJSON *root = cJSON_CreateObject();
    bool changed = false;

    // Power Group
    if (bits & WS_EVENT_POWER_UPDATED) {
        if (old->power != new->power) { cJSON_AddFloatToObject(root, "power", new->power); changed = true; }
        if (old->voltage != new->voltage) { cJSON_AddFloatToObject(root, "voltage", new->voltage); changed = true; }
        if (old->current != new->current) { cJSON_AddFloatToObject(root, "current", new->current); changed = true; }
        if (old->temp != new->temp) { cJSON_AddFloatToObject(root, "temp", new->temp); changed = true; }
        if (old->temp2 != new->temp2) { cJSON_AddFloatToObject(root, "temp2", new->temp2); changed = true; }
        if (old->vrTemp != new->vrTemp) { cJSON_AddFloatToObject(root, "vrTemp", new->vrTemp); changed = true; }
        if (old->core_voltage != new->core_voltage) { cJSON_AddFloatToObject(root, "coreVoltageActual", new->core_voltage); changed = true; }
        if (old->frequency_value != new->frequency_value) { cJSON_AddFloatToObject(root, "frequency", new->frequency_value); changed = true; }
        if (old->actual_frequency != new->actual_frequency) { cJSON_AddFloatToObject(root, "actualFrequency", new->actual_frequency); changed = true; }
        if (old->expectedHashrate != new->expectedHashrate) { cJSON_AddFloatToObject(root, "expectedHashrate", new->expectedHashrate); changed = true; }
        if (old->power_fault != new->power_fault) { cJSON_AddNumberToObject(root, "power_fault", new->power_fault); changed = true; }
    }

    // Fan Group
    if (bits & WS_EVENT_FAN_UPDATED) {
        if (old->fan_perc != new->fan_perc) { cJSON_AddFloatToObject(root, "fanspeed", new->fan_perc); changed = true; }
        if (old->fan_rpm != new->fan_rpm) { cJSON_AddNumberToObject(root, "fanrpm", new->fan_rpm); changed = true; }
        if (old->fan2_rpm != new->fan2_rpm) { cJSON_AddNumberToObject(root, "fan2rpm", new->fan2_rpm); changed = true; }
    }

    // Hashrate Group
    if (bits & (WS_EVENT_HASHRATE_UPDATED | WS_EVENT_STRATUM_UPDATED)) {
        if (old->hashRate != new->hashRate) { cJSON_AddFloatToObject(root, "hashRate", new->hashRate); changed = true; }
        if (old->hashRate_1m != new->hashRate_1m) { cJSON_AddFloatToObject(root, "hashRate_1m", new->hashRate_1m); changed = true; }
        if (old->hashRate_10m != new->hashRate_10m) { cJSON_AddFloatToObject(root, "hashRate_10m", new->hashRate_10m); changed = true; }
        if (old->hashRate_1h != new->hashRate_1h) { cJSON_AddFloatToObject(root, "hashRate_1h", new->hashRate_1h); changed = true; }
        if (old->errorPercentage != new->errorPercentage) { cJSON_AddFloatToObject(root, "errorPercentage", new->errorPercentage); changed = true; }
        
        if (bits & WS_EVENT_HASHRATE_UPDATED) {
            add_hashrate_monitor(root, g);
            changed = true;
        }
    }

    // Stratum/Shares Group
    if (bits & WS_EVENT_STRATUM_UPDATED) {
        if (old->sharesAccepted != new->sharesAccepted) { cJSON_AddNumberToObject(root, "sharesAccepted", new->sharesAccepted); changed = true; }
        if (old->sharesRejected != new->sharesRejected) { 
            cJSON_AddNumberToObject(root, "sharesRejected", new->sharesRejected); 
            add_rejected_reasons(root, g);
            changed = true; 
        }
        if (old->bestDiff != new->bestDiff) { cJSON_AddNumberToObject(root, "bestDiff", new->bestDiff); changed = true; }
        if (old->bestSessionDiff != new->bestSessionDiff) { cJSON_AddNumberToObject(root, "bestSessionDiff", new->bestSessionDiff); changed = true; }
        if (old->poolDifficulty != new->poolDifficulty) { cJSON_AddNumberToObject(root, "poolDifficulty", new->poolDifficulty); changed = true; }
        if (old->responseTime != new->responseTime) { cJSON_AddFloatToObject(root, "responseTime", new->responseTime); changed = true; }
        if (old->processTime != new->processTime) { cJSON_AddFloatToObject(root, "processTime", new->processTime); changed = true; }
        
        // Block Header
        if (old->blockHeight != new->blockHeight) { 
            cJSON_AddNumberToObject(root, "blockHeight", new->blockHeight); 
            add_block_header_arrays(root, g);
            changed = true; 
        }
        if (old->networkDifficulty != new->networkDifficulty) { cJSON_AddNumberToObject(root, "networkDifficulty", new->networkDifficulty); changed = true; }
        if (strcmp(old->scriptsig, new->scriptsig) != 0) { cJSON_AddStringToObject(root, "scriptsig", new->scriptsig); changed = true; }
        if (old->coinbaseValueTotalSatoshis != new->coinbaseValueTotalSatoshis) { cJSON_AddNumberToObject(root, "coinbaseValueTotalSatoshis", new->coinbaseValueTotalSatoshis); changed = true; }
        if (old->coinbaseValueUserSatoshis != new->coinbaseValueUserSatoshis) { cJSON_AddNumberToObject(root, "coinbaseValueUserSatoshis", new->coinbaseValueUserSatoshis); changed = true; }
        if (old->blockFound != new->blockFound) { cJSON_AddNumberToObject(root, "blockFound", new->blockFound); changed = true; }
        if (old->showNewBlock != new->showNewBlock) { cJSON_AddBoolToObject(root, "showNewBlock", new->showNewBlock); changed = true; }
    }

    // System Group - always include system metrics when sending any update
    if (old->free_heap != new->free_heap) { cJSON_AddNumberToObject(root, "freeHeap", new->free_heap); changed = true; }
    if (old->wifi_rssi != new->wifi_rssi) { cJSON_AddNumberToObject(root, "wifiRSSI", new->wifi_rssi); changed = true; }
    if (strcmp(old->wifi_status, new->wifi_status) != 0) { cJSON_AddStringToObject(root, "wifiStatus", new->wifi_status); changed = true; }
    if (old->uptimeSeconds != new->uptimeSeconds) { cJSON_AddNumberToObject(root, "uptimeSeconds", new->uptimeSeconds); changed = true; }
    if (old->stats_frequency != new->stats_frequency) { cJSON_AddNumberToObject(root, "statsFrequency", new->stats_frequency); changed = true; }
    if (old->cpuUsage != new->cpuUsage) { cJSON_AddFloatToObject(root, "cpuUsage", new->cpuUsage); changed = true; }
    if (old->miningPaused != new->miningPaused) { cJSON_AddBoolToObject(root, "miningPaused", new->miningPaused); changed = true; }
    if (old->overheatMode != new->overheatMode) { cJSON_AddBoolToObject(root, "overheatMode", new->overheatMode); changed = true; }
    if (old->hardwareFault != new->hardwareFault) { cJSON_AddBoolToObject(root, "hardwareFault", new->hardwareFault); changed = true; }
    if (strcmp(old->hardwareFaultMsg, new->hardwareFaultMsg) != 0) { cJSON_AddStringToObject(root, "hardwareFaultMsg", new->hardwareFaultMsg); changed = true; }
    if (old->screenPage != new->screenPage) { cJSON_AddNumberToObject(root, "screenPage", new->screenPage); changed = true; }

    // Pool Status & Failover (Always check for changes)
    if (old->is_using_fallback != new->is_using_fallback) { cJSON_AddBoolToObject(root, "isUsingFallbackStratum", new->is_using_fallback); changed = true; }
    if (strcmp(old->pool_connection_info, new->pool_connection_info) != 0) { cJSON_AddStringToObject(root, "poolConnectionInfo", new->pool_connection_info); changed = true; }
    if (strcmp(old->stratumURL, new->stratumURL) != 0) { cJSON_AddStringToObject(root, "stratumURL", new->stratumURL); changed = true; }
    if (strcmp(old->fallbackStratumURL, new->fallbackStratumURL) != 0) { cJSON_AddStringToObject(root, "fallbackStratumURL", new->fallbackStratumURL); changed = true; }
    if (strcmp(old->stratumUser, new->stratumUser) != 0) { cJSON_AddStringToObject(root, "stratumUser", new->stratumUser); changed = true; }
    if (strcmp(old->fallbackStratumUser, new->fallbackStratumUser) != 0) { cJSON_AddStringToObject(root, "fallbackStratumUser", new->fallbackStratumUser); changed = true; }
    if (old->stratumPort != new->stratumPort) { cJSON_AddNumberToObject(root, "stratumPort", new->stratumPort); changed = true; }
    if (old->fallbackStratumPort != new->fallbackStratumPort) { cJSON_AddNumberToObject(root, "fallbackStratumPort", new->fallbackStratumPort); changed = true; }

    if (!changed && cJSON_GetArraySize(root) == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    return root;
}

static void broadcast_json(cJSON *root)
{
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON");
        return;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)json_str;
    ws_pkt.len = strlen(json_str);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    websocket_broadcast(WS_TYPE_API, &ws_pkt);

    free(json_str);
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
        
        cJSON *diff_data = build_diff(&last_snapshot, &new_snapshot, bits, GLOBAL_STATE);
        if (diff_data != NULL) {
            cJSON *msg = cJSON_CreateObject();
            if (msg != NULL) {
                cJSON_AddStringToObject(msg, "event", "update");
                cJSON_AddItemToObject(msg, "data", diff_data);
                broadcast_json(msg);
                cJSON_Delete(msg);
            }
        }
        
        last_snapshot = new_snapshot;

        // Rate limit: prevent spamming the client if producer tasks update too frequently
        vTaskDelay(pdMS_TO_TICKS(WEBSOCKET_API_RATE_LIMIT_MS));
    }
}
