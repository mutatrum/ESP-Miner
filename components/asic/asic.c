#include <string.h>

#include <esp_log.h>

#include "bm1397.h"
#include "bm1366.h"
#include "bm1368.h"
#include "bm1370.h"

#include "asic.h"
#include "device_config.h"

static const char *TAG = "asic";

uint8_t ASIC_init(GlobalState * GLOBAL_STATE)
{
    switch (GLOBAL_STATE->DEVICE_CONFIG.family.asic.model) {
        case BM1397:
            return BM1397_init(GLOBAL_STATE->POWER_MANAGEMENT_MODULE.frequency_value, GLOBAL_STATE->DEVICE_CONFIG.family.asic_count, GLOBAL_STATE->DEVICE_CONFIG.family.asic.difficulty);
        case BM1366:
            return BM1366_init(GLOBAL_STATE->POWER_MANAGEMENT_MODULE.frequency_value, GLOBAL_STATE->DEVICE_CONFIG.family.asic_count, GLOBAL_STATE->DEVICE_CONFIG.family.asic.difficulty);
        case BM1368:
            return BM1368_init(GLOBAL_STATE->POWER_MANAGEMENT_MODULE.frequency_value, GLOBAL_STATE->DEVICE_CONFIG.family.asic_count, GLOBAL_STATE->DEVICE_CONFIG.family.asic.difficulty);
        case BM1370:
            return BM1370_init(GLOBAL_STATE->POWER_MANAGEMENT_MODULE.frequency_value, GLOBAL_STATE->DEVICE_CONFIG.family.asic_count, GLOBAL_STATE->DEVICE_CONFIG.family.asic.difficulty);
        default:
    }
    return ESP_OK;
}

task_result * ASIC_process_work(GlobalState * GLOBAL_STATE)
{
    switch (GLOBAL_STATE->DEVICE_CONFIG.family.asic.model) {
        case BM1397:
            return BM1397_process_work(GLOBAL_STATE);
        case BM1366:
            return BM1366_process_work(GLOBAL_STATE);
        case BM1368:
            return BM1368_process_work(GLOBAL_STATE);
        case BM1370:
            return BM1370_process_work(GLOBAL_STATE);
    }
    return NULL;
}

int ASIC_set_max_baud(GlobalState * GLOBAL_STATE)
{
    switch (GLOBAL_STATE->DEVICE_CONFIG.family.asic.model) {
        case BM1397:
            return BM1397_set_max_baud();
        case BM1366:
            return BM1366_set_max_baud();
        case BM1368:
            return BM1368_set_max_baud();
        case BM1370:
            return BM1370_set_max_baud();
        }
    return 0;
}

void ASIC_set_job_difficulty_mask(GlobalState * GLOBAL_STATE, uint8_t mask)
{
    switch (GLOBAL_STATE->DEVICE_CONFIG.family.asic.model) {
        case BM1397:
            BM1397_set_job_difficulty_mask(mask);
            break;
        case BM1366:
            BM1366_set_job_difficulty_mask(mask);
            break;
        case BM1368:
            BM1368_set_job_difficulty_mask(mask);
            break;
        case BM1370:
            BM1370_set_job_difficulty_mask(mask);
            break;
    }
}

void ASIC_send_work(GlobalState * GLOBAL_STATE, void * next_job)
{
    switch (GLOBAL_STATE->DEVICE_CONFIG.family.asic.model) {
        case BM1397:
            BM1397_send_work(GLOBAL_STATE, next_job);
            break;
        case BM1366:
            BM1366_send_work(GLOBAL_STATE, next_job);
            break;
        case BM1368:
            BM1368_send_work(GLOBAL_STATE, next_job);
            break;
        case BM1370:
            BM1370_send_work(GLOBAL_STATE, next_job);
            break;
    }
}

void ASIC_set_version_mask(GlobalState * GLOBAL_STATE, uint32_t mask)
{
    switch (GLOBAL_STATE->DEVICE_CONFIG.family.asic.model) {
        case BM1397:
            BM1397_set_version_mask(mask);
            break;
        case BM1366:
            BM1366_set_version_mask(mask);
            break;
        case BM1368:
            BM1368_set_version_mask(mask);
            break;
        case BM1370:
            BM1370_set_version_mask(mask);
            break;
    }
}

bool ASIC_set_frequency(GlobalState * GLOBAL_STATE, float target_frequency)
{
    ESP_LOGI(TAG, "Setting ASIC frequency to %.2f MHz", target_frequency);
    bool success = false;
    
    switch (GLOBAL_STATE->DEVICE_CONFIG.family.asic.model) {
        case BM1366:
            success = BM1366_set_frequency(target_frequency);
            break;
        case BM1368:
            success = BM1368_set_frequency(target_frequency);
            break;
        case BM1370:
            success = BM1370_set_frequency(target_frequency);
            break;
        case BM1397:
            // BM1397 doesn't have a set_frequency function yet
            ESP_LOGE(TAG, "Frequency transition not implemented for BM1397");
            success = false;
            break;
    }
    
    if (success) {
        ESP_LOGI(TAG, "Successfully transitioned to new ASIC frequency: %.2f MHz", target_frequency);
    } else {
        ESP_LOGE(TAG, "Failed to transition to new ASIC frequency: %.2f MHz", target_frequency);
    }
    
    return success;
}

double ASIC_get_asic_job_frequency_ms(GlobalState * GLOBAL_STATE)
{
    // default works for all chips
    int asic_job_frequency_ms = 20;

    // use 1/128 for timeout be approximatly equivalent to Antminer SXX hcn setting 
    uint64_t frequency = GLOBAL_STATE->POWER_MANAGEMENT_MODULE.frequency_value;
    int chain_chip_count = GLOBAL_STATE->DEVICE_CONFIG.family.asic_count;
    GLOBAL_STATE->version_mask = 0x1fffe000;
    int versions_to_roll = GLOBAL_STATE->version_mask>>13;

    ESP_LOGI(TAG, "ASIC Job Frequency: %llu Hz, Chain Chip Count: %i, Versions to Roll: %i", frequency, chain_chip_count, versions_to_roll);

    switch (GLOBAL_STATE->DEVICE_CONFIG.family.asic.model) {
        case BM1366:
            BM1366_set_nonce_percent(frequency,chain_chip_count);
            asic_job_frequency_ms = BM1366_get_timeout(frequency,chain_chip_count,versions_to_roll);
            break;
        case BM1368:
            BM1368_set_nonce_percent(frequency,chain_chip_count);
            asic_job_frequency_ms = BM1368_get_timeout(frequency,chain_chip_count,versions_to_roll);
            break;
        case BM1370:
            BM1370_set_nonce_percent(frequency,chain_chip_count);
            asic_job_frequency_ms = BM1370_get_timeout(frequency,chain_chip_count,versions_to_roll);
            break;
        case BM1397:
            BM1397_set_nonce_percent(frequency,chain_chip_count);
            asic_job_frequency_ms = BM1397_get_timeout(frequency,chain_chip_count,versions_to_roll);
            break;
        default:
            ESP_LOGE(TAG, "Unknown ASIC");
            return ESP_FAIL; 
    }

    // set minimum job frequency 
    if (asic_job_frequency_ms < 20) asic_job_frequency_ms = 20;

    return 60000; //asic_job_frequency_ms;
}
