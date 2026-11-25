#ifndef BM1373_PROBE_TASK_H_
#define BM1373_PROBE_TASK_H_

#include "asic_common.h"

void bm1373_probe_task(void *pvParameters);
void bm1373_probe_record_result(const task_result *result);

#endif /* BM1373_PROBE_TASK_H_ */
