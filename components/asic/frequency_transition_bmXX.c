#include "frequency_transition_bmXX.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char * TAG = "frequency_transition";

static float current_frequency = 56.25;

bool do_frequency_transition(float target_frequency, set_hash_frequency_fn set_frequency_fn, const char * asic_type) {
    if (set_frequency_fn == NULL) {
        ESP_LOGE(TAG, "Invalid function pointer provided");
        return false;
    }

    ESP_LOGI(TAG, "Ramping up frequency from %g MHz to %g MHz", current_frequency, target_frequency);

    float step = 6.25;
    float current = current_frequency;
    float target = target_frequency;

    float direction = (target > current) ? step : -step;

    float current_step_boundary = round(current / step) * step;
    float target_step_boundary = round(target / step) * step;

    if (current_step_boundary != target_step_boundary) {
        while ((direction > 0 && current < target) || (direction < 0 && current > target) || fmod(current, step) != 0) {
            if (fmod(current, step) != 0) {
                current = (direction > 0) ? ceil(current / step) * step : floor(current / step) * step;
            } else {
                float next_step = fmin(fabs(direction), fabs(target - current));
                current += (direction > 0) ? next_step : -next_step;
            }

            set_frequency_fn(current);
            
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
    
    set_frequency_fn(target);
    current_frequency = target;
    
    ESP_LOGI(TAG, "Successfully transitioned %s to %g MHz", asic_type, target);
    return true;
}
