#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_transport.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "global_state.h"
#include "stratum_task.h"
#include "stratum_driver.h"
#include "stratum_v1_client.h"
#include "stratum_v2_client.h"
#include "connect.h"
#include "system.h"
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

#define TAG "stratum_task"
#define MAX_RETRY_ATTEMPTS 3
#define RECOVERY_PROBE_INTERVAL_MS 30000

static TaskHandle_t s_primary_task_handle = NULL;
static TaskHandle_t s_fallback_task_handle = NULL;
static volatile bool s_should_reconnect[MAX_POOLS] = {false};

void stratum_abort_pool(uint16_t pool_idx)
{
    if (pool_idx >= MAX_POOLS) return;
    s_should_reconnect[pool_idx] = true;
    const stratum_driver_t *v1 = stratum_get_driver(STRATUM_PROTOCOL_V1);
    const stratum_driver_t *v2 = stratum_get_driver(STRATUM_PROTOCOL_V2);
    if (v1 && v1->abort_connection) v1->abort_connection(pool_idx);
    if (v2 && v2->abort_connection) v2->abort_connection(pool_idx);
}

bool stratum_reconnect_requested(void)
{
    for (int i = 0; i < MAX_POOLS; i++) {
        if (s_should_reconnect[i]) return true;
    }
    return false;
}

void stratum_notify_pool_modified(GlobalState *gs, uint16_t pool_idx)
{
    if (!gs || pool_idx >= MAX_POOLS) return;

    gs->SYSTEM_MODULE.primary_pool_error[0] = '\0';
    gs->SYSTEM_MODULE.pool_banner[0] = '\0';

    stratum_abort_pool(pool_idx);

    if (pool_idx == gs->SYSTEM_MODULE.primary_pool_index) {
        if (s_primary_task_handle) {
            xTaskNotifyGive(s_primary_task_handle);
        }
    } else if (pool_idx == gs->SYSTEM_MODULE.secondary_pool_index) {
        if (s_fallback_task_handle && (gs->SYSTEM_MODULE.is_using_fallback || gs->SYSTEM_MODULE.use_fallback_stratum)) {
            xTaskNotifyGive(s_fallback_task_handle);
        }
    }
}

void stratum_notify_pool_selection_changed(GlobalState *gs)
{
    if (!gs) return;

    gs->SYSTEM_MODULE.primary_pool_error[0] = '\0';
    gs->SYSTEM_MODULE.pool_banner[0] = '\0';

    uint16_t prim_idx = gs->SYSTEM_MODULE.primary_pool_index;
    uint16_t sec_idx = gs->SYSTEM_MODULE.secondary_pool_index;

    if (gs->SYSTEM_MODULE.use_fallback_stratum) {
        stratum_abort_pool(prim_idx);
        if (s_fallback_task_handle) {
            xTaskNotifyGive(s_fallback_task_handle);
        }
    } else {
        stratum_abort_pool(sec_idx);
        if (s_primary_task_handle) {
            xTaskNotifyGive(s_primary_task_handle);
        }
    }
}

static void stratum_fallback_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;
    int retry_attempts = 0;

    ESP_LOGI(TAG, "Fallback worker task started (cold standby)");

    while (1) {
        // Cold standby: wait until notified to run
        if (!GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback &&
            !GLOBAL_STATE->SYSTEM_MODULE.use_fallback_stratum) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        if (GLOBAL_STATE->SYSTEM_MODULE.mining_paused || GLOBAL_STATE->SYSTEM_MODULE.hardware_fault) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!wifi_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // If Primary recovered while we were unblocking, go back to sleep
        if (!GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback &&
            !GLOBAL_STATE->SYSTEM_MODULE.use_fallback_stratum) {
            continue;
        }

        uint16_t sec_idx = GLOBAL_STATE->SYSTEM_MODULE.secondary_pool_index;
        const PoolConfig *pool = &GLOBAL_STATE->SYSTEM_MODULE.pools[sec_idx];
        if (!pool->url || pool->url[0] == '\0' || pool->port == 0) {
            ESP_LOGW(TAG, "Fallback pool %u not configured", sec_idx);
            GLOBAL_STATE->SYSTEM_MODULE.pools_unavailable = true;
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        const stratum_driver_t *driver = stratum_get_driver(pool->protocol);
        if (!driver || !driver->run) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "Fallback worker running pool %u (%s)", sec_idx, pool->url);
        s_should_reconnect[sec_idx] = false;
        esp_err_t err = driver->run(GLOBAL_STATE, sec_idx);

        // Case 1: Stepped down because Primary recovered
        if (!GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback &&
            !GLOBAL_STATE->SYSTEM_MODULE.use_fallback_stratum) {
            ESP_LOGI(TAG, "Fallback stepping down, primary pool is active");
            retry_attempts = 0;
            continue;
        }

        // Case 2: Disconnected or failed
        if (err == ESP_OK || GLOBAL_STATE->SYSTEM_MODULE.work_received > 0) {
            retry_attempts = 0;
            GLOBAL_STATE->SYSTEM_MODULE.work_received = 0;
            SYSTEM_reset_pool_session(GLOBAL_STATE);
        } else {
            retry_attempts++;
            ESP_LOGW(TAG, "Fallback pool %u (%s) failed (attempt %d/%d)",
                     sec_idx, pool->url, retry_attempts, MAX_RETRY_ATTEMPTS);
            if (retry_attempts >= MAX_RETRY_ATTEMPTS) {
                GLOBAL_STATE->SYSTEM_MODULE.pools_unavailable = true;
                ESP_LOGW(TAG, "Both primary and fallback pools unavailable");
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(RECOVERY_PROBE_INTERVAL_MS));
                retry_attempts = 0;
            } else {
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
    }
}

static void stratum_primary_task_loop(GlobalState *GLOBAL_STATE)
{
    int retry_attempts = 0;

    while (1) {
        if (GLOBAL_STATE->SYSTEM_MODULE.mining_paused || GLOBAL_STATE->SYSTEM_MODULE.hardware_fault) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (!wifi_is_connected()) {
            ESP_LOGI(TAG, "WiFi disconnected, waiting...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // If force fallback is toggled on, Primary steps down and sleeps
        if (GLOBAL_STATE->SYSTEM_MODULE.use_fallback_stratum) {
            uint16_t prim_idx = GLOBAL_STATE->SYSTEM_MODULE.primary_pool_index;
            stratum_abort_pool(prim_idx);
            GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback = true;
            if (s_fallback_task_handle) {
                xTaskNotifyGive(s_fallback_task_handle);
            }
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
            continue;
        }

        uint16_t prim_idx = GLOBAL_STATE->SYSTEM_MODULE.primary_pool_index;
        const PoolConfig *pool = &GLOBAL_STATE->SYSTEM_MODULE.pools[prim_idx];
        if (!pool->url || pool->url[0] == '\0' || pool->port == 0) {
            ESP_LOGW(TAG, "Primary pool %u not configured", prim_idx);
            GLOBAL_STATE->SYSTEM_MODULE.pools_unavailable = true;
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        const stratum_driver_t *driver = stratum_get_driver(pool->protocol);
        if (!driver || !driver->run) {
            ESP_LOGE(TAG, "No driver found for protocol %d", pool->protocol);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "Primary worker running pool %u (%s)", prim_idx, pool->url);
        s_should_reconnect[prim_idx] = false;
        esp_err_t err = driver->run(GLOBAL_STATE, prim_idx);

        if (err == ESP_OK || GLOBAL_STATE->SYSTEM_MODULE.work_received > 0) {
            retry_attempts = 0;
            GLOBAL_STATE->SYSTEM_MODULE.pools_unavailable = false;
            GLOBAL_STATE->SYSTEM_MODULE.primary_pool_error[0] = '\0';
            GLOBAL_STATE->SYSTEM_MODULE.work_received = 0;
            SYSTEM_reset_pool_session(GLOBAL_STATE);
        } else {
            retry_attempts++;
            ESP_LOGW(TAG, "Primary pool %u (%s) failed (attempt %d/%d)",
                     prim_idx, pool->url, retry_attempts, MAX_RETRY_ATTEMPTS);

            if (retry_attempts >= MAX_RETRY_ATTEMPTS) {
                retry_attempts = 0;

                strlcpy(GLOBAL_STATE->SYSTEM_MODULE.primary_pool_error,
                        GLOBAL_STATE->SYSTEM_MODULE.pool_connection_info,
                        sizeof(GLOBAL_STATE->SYSTEM_MODULE.primary_pool_error));

                uint16_t sec_idx = GLOBAL_STATE->SYSTEM_MODULE.secondary_pool_index;
                bool has_fallback = (GLOBAL_STATE->SYSTEM_MODULE.pools[sec_idx].url != NULL &&
                                     GLOBAL_STATE->SYSTEM_MODULE.pools[sec_idx].url[0] != '\0');

                if (has_fallback) {
                    GLOBAL_STATE->SYSTEM_MODULE.is_using_fallback = true;
                    ESP_LOGI(TAG, "Primary failed. Waking fallback pool %u (%s)",
                             sec_idx, GLOBAL_STATE->SYSTEM_MODULE.pools[sec_idx].url);
                    if (s_fallback_task_handle) {
                        xTaskNotifyGive(s_fallback_task_handle);
                    }
                } else {
                    GLOBAL_STATE->SYSTEM_MODULE.pools_unavailable = true;
                }

                ESP_LOGI(TAG, "Primary waiting %d seconds before retry...", RECOVERY_PROBE_INTERVAL_MS / 1000);
                ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(RECOVERY_PROBE_INTERVAL_MS));
            } else {
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
    }
}

void stratum_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;
    s_primary_task_handle = xTaskGetCurrentTaskHandle();

    ESP_LOGI(TAG, "Starting Stratum task topology");

    if (xTaskCreateWithCaps(stratum_fallback_task, "stratum_fb", 16384, (void *)GLOBAL_STATE, 16, &s_fallback_task_handle, MALLOC_CAP_SPIRAM) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create stratum fallback task");
    }

    stratum_primary_task_loop(GLOBAL_STATE);
}

int stratum_submit_share(GlobalState *GLOBAL_STATE, const asic_job_t *active_job,
                         uint32_t nonce, uint32_t rolled_version, uint64_t *sent_time_us)
{
    if (!GLOBAL_STATE || !active_job || active_job->pool_id >= MAX_POOLS) {
        return -1;
    }

    const stratum_driver_t *driver = stratum_get_driver_for_job(active_job);
    if (!driver || !driver->submit_share) {
        return -1;
    }

    int ret = driver->submit_share(GLOBAL_STATE, active_job, nonce, rolled_version, sent_time_us);

    if (ret < 0) {
        ESP_LOGW(TAG, "Failed to submit share to socket (ret: %d, errno %d: %s)", ret, errno, strerror(errno));
    }
    return ret;
}
