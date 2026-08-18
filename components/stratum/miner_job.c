#include "miner_job.h"
#include "stratum_api.h"
#include "utils.h"
#include <string.h>
#include <pthread.h>

static miner_job_t s_job_pool[MINER_JOB_POOL_SIZE];
static uint32_t s_pool_head = 0;
static pthread_mutex_t s_pool_lock = PTHREAD_MUTEX_INITIALIZER;

void miner_job_pool_init(void)
{
    pthread_mutex_lock(&s_pool_lock);
    memset(s_job_pool, 0, sizeof(s_job_pool));
    s_pool_head = 0;
    pthread_mutex_unlock(&s_pool_lock);
}

miner_job_t *miner_job_pool_next(void)
{
    pthread_mutex_lock(&s_pool_lock);
    miner_job_t *slot = &s_job_pool[s_pool_head % MINER_JOB_POOL_SIZE];
    s_pool_head++;
    memset(slot, 0, sizeof(miner_job_t));
    pthread_mutex_unlock(&s_pool_lock);
    return slot;
}

void miner_job_from_v1_notify(miner_job_t *dest, const mining_notify *notify,
                              const uint8_t *extranonce1, uint8_t extranonce1_len,
                              uint8_t extranonce2_len, uint8_t pool_id,
                              double pool_diff, uint32_t version_mask)
{
    if (!dest || !notify) return;

    dest->type = JOB_TYPE_V1;
    dest->pool_id = pool_id;
    dest->pool_diff = pool_diff;
    dest->version_mask = version_mask;
    dest->extranonce1_len = (extranonce1_len > sizeof(dest->extranonce1)) ? sizeof(dest->extranonce1) : extranonce1_len;
    if (extranonce1 && dest->extranonce1_len > 0) {
        memcpy(dest->extranonce1, extranonce1, dest->extranonce1_len);
    }
    dest->extranonce2_len = extranonce2_len;

    if (notify->job_id) {
        strncpy(dest->job_id, notify->job_id, sizeof(dest->job_id) - 1);
    }
    dest->version = notify->version;
    dest->ntime = notify->ntime;
    dest->nbits = notify->target;
    dest->clean_jobs = notify->clean_jobs;

    if (notify->prev_block_hash) {
        hex2bin(notify->prev_block_hash, dest->prev_hash, 32);
        reverse_endianness_per_word(dest->prev_hash);
    }

    if (notify->merkle_branches && notify->n_merkle_branches > 0) {
        size_t count = notify->n_merkle_branches;
        if (count > MAX_MERKLE_BRANCHES) count = MAX_MERKLE_BRANCHES;
        dest->merkle_path_count = count;
        memcpy(dest->merkle_path, notify->merkle_branches, count * 32);
    }

    if (notify->coinbase_1) {
        size_t c1_len = strlen(notify->coinbase_1) / 2;
        if (c1_len > sizeof(dest->coinbase_prefix)) c1_len = sizeof(dest->coinbase_prefix);
        hex2bin(notify->coinbase_1, dest->coinbase_prefix, c1_len);
        dest->coinbase_prefix_len = c1_len;
    }

    if (notify->coinbase_2) {
        size_t c2_len = strlen(notify->coinbase_2) / 2;
        if (c2_len > sizeof(dest->coinbase_suffix)) c2_len = sizeof(dest->coinbase_suffix);
        hex2bin(notify->coinbase_2, dest->coinbase_suffix, c2_len);
        dest->coinbase_suffix_len = c2_len;
    }
}
