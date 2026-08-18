#ifndef STRATUM_TASK_H_
#define STRATUM_TASK_H_

#include "global_state.h"
#include <stdbool.h>

// Unified persistent Stratum Task entry point.
// Manages network connectivity, pool failover/heartbeat, and dispatches to V1/V2 protocol drivers.
void stratum_task(void *pvParameters);

// Request the active stratum client to reconnect (e.g. on primary pool recovery or pool settings change).
void stratum_request_reconnect(void);

// Probe a pool to check if it is reachable without disrupting active mining.
bool stratum_probe_pool(GlobalState *gs, uint16_t pool_idx);

// Promptly trigger a heartbeat check of the primary pool (e.g. when primary config changes while on fallback).
void stratum_trigger_heartbeat_check(void);

#endif /* STRATUM_TASK_H_ */
