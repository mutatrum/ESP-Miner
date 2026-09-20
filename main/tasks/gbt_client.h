#ifndef GBT_CLIENT_H_
#define GBT_CLIENT_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "mining.h"

struct GlobalState;
typedef struct GlobalState GlobalState;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Main worker loop for Direct GBT Solo Mining.
 * Dispatched from stratum_task when pool protocol is STRATUM_PROTOCOL_GBT.
 */
esp_err_t gbt_run(GlobalState *gs, uint16_t pool_idx);

/**
 * Submit share / found block to the connected Bitcoin Core node via submitblock.
 */
int gbt_submit_share(GlobalState *gs,
                     const bm_job *active_job,
                     uint32_t nonce,
                     uint32_t rolled_version,
                     uint64_t *sent_time_us);

/**
 * Direct submitblock RPC execution.
 */
esp_err_t gbt_submit_block(GlobalState *gs,
                           const bm_job *active_job,
                           uint32_t nonce,
                           uint32_t rolled_version);

/**
 * Probe Bitcoin Core node connectivity for pool health and failover.
 */
bool gbt_probe_pool(GlobalState *gs, uint16_t pool_idx);

#ifdef __cplusplus
}
#endif

#endif // GBT_CLIENT_H_
