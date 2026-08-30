#include "unity.h"
#include "miner_job.h"

TEST_CASE("miner_job_pool acquire and release lifecycle", "[stratum]")
{
    miner_job_pool_init();

    miner_job_t *slots[MINER_JOB_POOL_SIZE];

    // 1. Acquire all slots
    for (int i = 0; i < MINER_JOB_POOL_SIZE; i++) {
        slots[i] = miner_job_pool_acquire();
        TEST_ASSERT_NOT_NULL(slots[i]);
    }

    // 2. Pool is full, next acquire should return NULL
    miner_job_t *overflow = miner_job_pool_acquire();
    TEST_ASSERT_NULL(overflow);

    // 3. Release slot 2 and verify it can be re-acquired
    miner_job_pool_release(slots[2]);
    miner_job_t *reacquired = miner_job_pool_acquire();
    TEST_ASSERT_EQUAL_PTR(slots[2], reacquired);

    // 4. NULL release safety
    miner_job_pool_release(NULL);

    // 5. Re-init frees all slots
    miner_job_pool_init();
    for (int i = 0; i < MINER_JOB_POOL_SIZE; i++) {
        slots[i] = miner_job_pool_acquire();
        TEST_ASSERT_NOT_NULL(slots[i]);
    }
}
