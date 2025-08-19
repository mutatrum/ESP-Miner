#ifndef ASIC_H
#define ASIC_H

#include <esp_err.h>
#include "global_state.h"
#include "common.h"

uint8_t ASIC_init();
task_result * ASIC_process_work();
int ASIC_set_max_baud();
void ASIC_send_work(void * next_job);
void ASIC_set_version_mask(uint32_t mask);
bool ASIC_set_frequency(float frequency);
double ASIC_get_asic_job_frequency_ms();

#endif // ASIC_H
