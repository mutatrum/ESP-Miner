#include "scoreboard.h"
#include "esp_log.h"
#include <stdio.h>

static const char * TAG = "scoreboard";

void scoreboard_add(Scoreboard *scoreboard, double difficulty, const char *job_id, const char *extranonce2, uint32_t ntime, uint32_t nonce, uint32_t version_bits)
{
    int i = (scoreboard->count < MAX_SCOREBOARD) ? scoreboard->count : MAX_SCOREBOARD - 1;

    if (scoreboard->count == MAX_SCOREBOARD && i >= 0 && difficulty <= scoreboard->entries[i].difficulty) {
        return;
    }

    ScoreboardEntry new_entry = {
        .difficulty = difficulty,
        .ntime = ntime,
        .nonce = nonce,
        .version_bits = version_bits,
    };
    strncpy(new_entry.job_id, job_id, sizeof(new_entry.job_id) - 1);
    new_entry.job_id[sizeof(new_entry.job_id) - 1] = '\0';
    strncpy(new_entry.extranonce2, extranonce2, sizeof(new_entry.extranonce2) - 1);
    new_entry.extranonce2[sizeof(new_entry.extranonce2) - 1] = '\0';

    if (xSemaphoreTake(scoreboard->mutex, portMAX_DELAY) == pdTRUE) {
        while (i > 0 && difficulty > scoreboard->entries[i - 1].difficulty) {
            scoreboard->entries[i] = scoreboard->entries[i - 1];
            i--;
        }
    
        scoreboard->entries[i] = new_entry;
        if (scoreboard->count < MAX_SCOREBOARD) {
            scoreboard->count++;
        }
        xSemaphoreGive(scoreboard->mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take mutex");
        return;
    }
    
    ESP_LOGI(TAG, "New #%d: Difficulty: %.1f, Job ID: %s, extranonce2: %s, ntime: %d, nonce: %08X, version_bits: %08X",
        i+1, new_entry.difficulty, new_entry.job_id, new_entry.extranonce2, new_entry.ntime, (unsigned int)new_entry.nonce, (unsigned int)new_entry.version_bits);
}
