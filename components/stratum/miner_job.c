#include "miner_job.h"
#include "stratum_api.h"
#include "utils.h"
#include <string.h>
#include <pthread.h>

static miner_job_t s_job_pool[MINER_JOB_POOL_SIZE];
static bool s_job_in_use[MINER_JOB_POOL_SIZE];
static pthread_mutex_t s_pool_lock = PTHREAD_MUTEX_INITIALIZER;

void miner_job_pool_init(void)
{
    pthread_mutex_lock(&s_pool_lock);
    memset(s_job_pool, 0, sizeof(s_job_pool));
    memset(s_job_in_use, 0, sizeof(s_job_in_use));
    pthread_mutex_unlock(&s_pool_lock);
}

miner_job_t *miner_job_pool_acquire(void)
{
    pthread_mutex_lock(&s_pool_lock);
    miner_job_t *slot = NULL;
    for (size_t i = 0; i < MINER_JOB_POOL_SIZE; i++) {
        if (!s_job_in_use[i]) {
            s_job_in_use[i] = true;
            slot = &s_job_pool[i];
            break;
        }
    }
    pthread_mutex_unlock(&s_pool_lock);

    if (slot != NULL) {
        memset(slot, 0, sizeof(miner_job_t));
    }
    return slot;
}

void miner_job_pool_release(miner_job_t *job)
{
    if (job == NULL) return;

    pthread_mutex_lock(&s_pool_lock);
    ptrdiff_t idx = job - s_job_pool;
    if (idx >= 0 && idx < MINER_JOB_POOL_SIZE) {
        s_job_in_use[idx] = false;
    }
    pthread_mutex_unlock(&s_pool_lock);
}
