#ifndef BM1373_H_
#define BM1373_H_

#include "asic_common.h"
#include "asic_job.h"

typedef struct GlobalState GlobalState;

#define BM1373_SMALL_CORE_BITS 4

#define BM1373_SERIALTX_DEBUG false
#define BM1373_SERIALRX_DEBUG false
#define BM1373_DEBUG_WORK false //causes insane amount of debug output

uint8_t BM1373_init(GlobalState * GLOBAL_STATE);
void BM1373_send_work(GlobalState * GLOBAL_STATE, const asic_job_t * next_job);
void BM1373_set_version_mask(uint32_t version_mask);
int BM1373_set_max_baud(void);
float BM1373_send_hash_frequency(float frequency);
task_result * BM1373_process_work(GlobalState * GLOBAL_STATE);
void BM1373_read_registers(void);
void BM1373_set_nonce_space(double nonce_percent, float frequency, uint16_t asic_count, uint16_t cores);
void BM1373_set_difficulty(uint8_t difficulty_power);
void BM1373_set_hash_counting_number(uint32_t hcn);
void BM1373_set_misc_control(uint32_t value);
void BM1373_set_nonce_bin_overflow(uint8_t value);
void BM1373_set_probe_job_interval_ms(int interval_ms);
int BM1373_get_probe_job_interval_ms(void);

#endif /* BM1373_H_ */
