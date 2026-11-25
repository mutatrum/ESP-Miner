#include "bm1373_probe_task.h"

#include "bm1373.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "global_state.h"

#include <arpa/inet.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "bm1373_probe";

#define PROBE_SETTLE_MS 2000
#define PROBE_STEP_DURATION_MS 30000
#define PROBE_SEEN_RING_SIZE 128

typedef struct {
    const char *name;
    uint32_t hcn;
    uint32_t misc_control;
    uint8_t nonce_bin_overflow;
    int job_interval_ms;
} probe_step_cfg_t;

typedef struct {
    uint32_t nonce;
    uint32_t rolled_version;
    uint64_t timestamp_us;
    char job_id[64];
    bool valid;
} seen_result_t;

typedef struct {
    uint32_t total;
    uint32_t unique;
    uint32_t dups;
    uint32_t asic0_count;
    uint32_t asic1_count;
    uint32_t dup_dt_min_ms;
    uint32_t dup_dt_max_ms;
    uint64_t bucket_mask[4];
    uint64_t dup_bucket_mask[4];
    float asic0_ghs;
    float asic1_ghs;
} probe_step_stats_t;

static const probe_step_cfg_t PROBE_STEPS[] = {
    { "baseline_1EB5",  0x00001EB5, 0xFF00C100, 0x04,  250 },
    { "hcn_9000_lt8",   0x00009000, 0xFF00C100, 0x04, 2000 },
    { "hcn_C000_gt8",   0x0000C000, 0xFF00C100, 0x04, 2000 },
    { "hcn_2145A_432c", 0x0002145A, 0xFF00C100, 0x04, 2000 },
    { "misc_F000C100",  0x00001EB5, 0xF000C100, 0x04,  250 },
    { "core0D_05",      0x00001EB5, 0xFF00C100, 0x05,  250 },
    { "hcn_2145A_0D05", 0x0002145A, 0xFF00C100, 0x05, 2000 },
};

#define NUM_PROBE_STEPS (sizeof(PROBE_STEPS) / sizeof(PROBE_STEPS[0]))

static pthread_mutex_t s_probe_lock = PTHREAD_MUTEX_INITIALIZER;
static bool s_collecting = false;
static probe_step_stats_t s_cur_stats;
static probe_step_stats_t s_all_stats[NUM_PROBE_STEPS];
static seen_result_t s_seen_ring[PROBE_SEEN_RING_SIZE];
static size_t s_seen_head = 0;

static inline void set_bucket_bit(uint64_t mask[4], uint8_t bucket)
{
    mask[bucket >> 6] |= (1ULL << (bucket & 63));
}

static inline bool get_bucket_bit(const uint64_t mask[4], uint8_t bucket)
{
    return (mask[bucket >> 6] & (1ULL << (bucket & 63))) != 0;
}

static void format_bucket_ranges(const uint64_t mask[4], char *buf, size_t buf_len)
{
    size_t pos = 0;
    buf[0] = '\0';
    int start = -1;

    for (int b = 0; b <= 256; b++) {
        bool set = (b < 256) && get_bucket_bit(mask, (uint8_t)b);
        if (set && start < 0) {
            start = b;
        } else if (!set && start >= 0) {
            int end = b - 1;
            int n;
            if (start == end) {
                n = snprintf(buf + pos, buf_len - pos, "%s%d", pos > 0 ? "," : "", start);
            } else {
                n = snprintf(buf + pos, buf_len - pos, "%s%d-%d", pos > 0 ? "," : "", start, end);
            }
            if (n > 0 && pos + (size_t)n < buf_len) {
                pos += (size_t)n;
            }
            start = -1;
        }
    }

    if (pos == 0) {
        snprintf(buf, buf_len, "none");
    }
}

void bm1373_probe_record_result(const task_result *result)
{
    if (!result) {
        return;
    }

    pthread_mutex_lock(&s_probe_lock);
    if (!s_collecting) {
        pthread_mutex_unlock(&s_probe_lock);
        return;
    }

    uint32_t nonce_h = ntohl(result->nonce);
    uint8_t bucket = (uint8_t)((nonce_h >> 17) & 0xFF);

    s_cur_stats.total++;
    if (result->asic_nr == 0) {
        s_cur_stats.asic0_count++;
    } else {
        s_cur_stats.asic1_count++;
    }
    set_bucket_bit(s_cur_stats.bucket_mask, bucket);

    bool is_dup = false;
    uint32_t dt_ms = 0;
    for (size_t i = 0; i < PROBE_SEEN_RING_SIZE; i++) {
        if (!s_seen_ring[i].valid) {
            continue;
        }
        if (s_seen_ring[i].nonce == result->nonce &&
            s_seen_ring[i].rolled_version == result->rolled_version &&
            strncmp(s_seen_ring[i].job_id, result->job.job_id, sizeof(s_seen_ring[i].job_id)) == 0) {
            is_dup = true;
            if (result->timestamp_us >= s_seen_ring[i].timestamp_us) {
                dt_ms = (uint32_t)((result->timestamp_us - s_seen_ring[i].timestamp_us) / 1000ULL);
            }
            break;
        }
    }

    if (is_dup) {
        s_cur_stats.dups++;
        set_bucket_bit(s_cur_stats.dup_bucket_mask, bucket);
        if (s_cur_stats.dup_dt_min_ms == 0 || dt_ms < s_cur_stats.dup_dt_min_ms) {
            s_cur_stats.dup_dt_min_ms = dt_ms;
        }
        if (dt_ms > s_cur_stats.dup_dt_max_ms) {
            s_cur_stats.dup_dt_max_ms = dt_ms;
        }
    } else {
        s_cur_stats.unique++;
        seen_result_t *slot = &s_seen_ring[s_seen_head];
        slot->nonce = result->nonce;
        slot->rolled_version = result->rolled_version;
        slot->timestamp_us = result->timestamp_us;
        strlcpy(slot->job_id, result->job.job_id, sizeof(slot->job_id));
        slot->valid = true;
        s_seen_head = (s_seen_head + 1) % PROBE_SEEN_RING_SIZE;
    }

    pthread_mutex_unlock(&s_probe_lock);
}

static void apply_step_registers(const probe_step_cfg_t *cfg)
{
    BM1373_set_probe_job_interval_ms(cfg->job_interval_ms);
    BM1373_set_nonce_bin_overflow(cfg->nonce_bin_overflow);
    vTaskDelay(pdMS_TO_TICKS(10));
    BM1373_set_misc_control(cfg->misc_control);
    vTaskDelay(pdMS_TO_TICKS(10));
    BM1373_set_hash_counting_number(cfg->hcn);
}

void bm1373_probe_task(void *pvParameters)
{
    GlobalState *gs = (GlobalState *)pvParameters;

    ESP_LOGI(TAG, "BM1373 probe waiting for ASIC + Stratum work...");
    while (!gs->ASIC_initalized || !gs->has_work) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "Work active — settling 5 s before starting %u probe steps (%u s each)",
             (unsigned)NUM_PROBE_STEPS, PROBE_STEP_DURATION_MS / 1000);
    vTaskDelay(pdMS_TO_TICKS(5000));

    for (size_t i = 0; i < NUM_PROBE_STEPS; i++) {
        const probe_step_cfg_t *cfg = &PROBE_STEPS[i];

        ESP_LOGI(TAG, "=== STEP %u/%u: %s (HCN=0x%08" PRIX32 " MISC=0x%08" PRIX32 " CORE0D=0x%02X JOB_MS=%d) ===",
                 (unsigned)(i + 1), (unsigned)NUM_PROBE_STEPS, cfg->name,
                 cfg->hcn, cfg->misc_control, cfg->nonce_bin_overflow, cfg->job_interval_ms);

        pthread_mutex_lock(&s_probe_lock);
        s_collecting = false;
        memset(&s_cur_stats, 0, sizeof(s_cur_stats));
        memset(s_seen_ring, 0, sizeof(s_seen_ring));
        s_seen_head = 0;
        pthread_mutex_unlock(&s_probe_lock);

        apply_step_registers(cfg);
        vTaskDelay(pdMS_TO_TICKS(PROBE_SETTLE_MS));

        pthread_mutex_lock(&s_probe_lock);
        s_collecting = true;
        pthread_mutex_unlock(&s_probe_lock);

        vTaskDelay(pdMS_TO_TICKS(PROBE_STEP_DURATION_MS));

        pthread_mutex_lock(&s_probe_lock);
        s_collecting = false;
        s_all_stats[i] = s_cur_stats;
        pthread_mutex_unlock(&s_probe_lock);

        if (gs->HASHRATE_MONITOR_MODULE.is_initialized) {
            pthread_mutex_lock(&gs->HASHRATE_MONITOR_MODULE.lock);
            int asic_count = gs->DEVICE_CONFIG.family.asic_count;
            if (asic_count > 0) {
                s_all_stats[i].asic0_ghs = gs->HASHRATE_MONITOR_MODULE.total_measurement[0].hashrate;
            }
            if (asic_count > 1) {
                s_all_stats[i].asic1_ghs = gs->HASHRATE_MONITOR_MODULE.total_measurement[1].hashrate;
            }
            pthread_mutex_unlock(&gs->HASHRATE_MONITOR_MODULE.lock);
        }

        char buckets_str[192];
        char dup_buckets_str[192];
        format_bucket_ranges(s_all_stats[i].bucket_mask, buckets_str, sizeof(buckets_str));
        format_bucket_ranges(s_all_stats[i].dup_bucket_mask, dup_buckets_str, sizeof(dup_buckets_str));

        ESP_LOGI(TAG, "RESULT %s: total=%" PRIu32 " uniq=%" PRIu32 " dups=%" PRIu32 " (dt=%" PRIu32 "-%" PRIu32 "ms) "
                 "asic0=%" PRIu32 "(%.0fGH/s) asic1=%" PRIu32 "(%.0fGH/s) buckets=[%s] dup_buckets=[%s]",
                 cfg->name, s_all_stats[i].total, s_all_stats[i].unique, s_all_stats[i].dups,
                 s_all_stats[i].dup_dt_min_ms, s_all_stats[i].dup_dt_max_ms,
                 s_all_stats[i].asic0_count, s_all_stats[i].asic0_ghs,
                 s_all_stats[i].asic1_count, s_all_stats[i].asic1_ghs,
                 buckets_str, dup_buckets_str);
    }

    // Restore stock BM1373 settings
    apply_step_registers(&PROBE_STEPS[0]);
    BM1373_set_probe_job_interval_ms(0);

    ESP_LOGI(TAG, "================ BM1373 PROBE SUMMARY ================");
    for (size_t i = 0; i < NUM_PROBE_STEPS; i++) {
        char buckets_str[192];
        char dup_buckets_str[192];
        format_bucket_ranges(s_all_stats[i].bucket_mask, buckets_str, sizeof(buckets_str));
        format_bucket_ranges(s_all_stats[i].dup_bucket_mask, dup_buckets_str, sizeof(dup_buckets_str));
        ESP_LOGI(TAG, "%-14s | HCN=%08" PRIX32 " MISC=%08" PRIX32 " 0D=%02X | tot=%3" PRIu32 " uniq=%3" PRIu32
                 " dup=%3" PRIu32 " dt=%2" PRIu32 "-%2" PRIu32 "ms | A0=%.0f A1=%.0f | b=[%s] db=[%s]",
                 PROBE_STEPS[i].name, PROBE_STEPS[i].hcn, PROBE_STEPS[i].misc_control,
                 PROBE_STEPS[i].nonce_bin_overflow,
                 s_all_stats[i].total, s_all_stats[i].unique, s_all_stats[i].dups,
                 s_all_stats[i].dup_dt_min_ms, s_all_stats[i].dup_dt_max_ms,
                 s_all_stats[i].asic0_ghs, s_all_stats[i].asic1_ghs,
                 buckets_str, dup_buckets_str);
    }
    ESP_LOGI(TAG, "======================================================");

    vTaskDelete(NULL);
}
