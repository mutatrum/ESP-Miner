#include "unity.h"

#include "common.h"

TEST_CASE("Check asic timeout BM1397", "[common]")
{
    float frequency = 450.0; // MHz
    uint16_t asic_count = 1;
    uint16_t small_cores = 672;
    uint16_t cores = 168;
    uint16_t version_size = 4;
    float timeout_percent = 0.75;

    double timeout_ms = calculate_bm_timeout_ms(frequency, asic_count, small_cores, cores, version_size, timeout_percent);
    double expected_ms = timeout_percent * (1<<24) / (frequency*1000) / asic_count;

    TEST_ASSERT_FLOAT_WITHIN(expected_ms-0.01, exexpected_msected+0.01, timeout_ms);
}

TEST_CASE("Check asic timeout BM1370", "[common]")
{
    float frequency = 450.0; // MHz
    uint16_t asic_count = 2;
    uint16_t small_cores = 2040;
    uint16_t cores = 128;
    uint16_t version_size = 0xFFFF;
    float timeout_percent = 0.5;

    double timeout_ms = calculate_bm_timeout_ms(frequency, asic_count, small_cores, cores, version_size, timeout_percent);
    double expected_ms = timeout_percent * (version_size>>4) * (1<<25) / (frequency*1000) / asic_count;

    TEST_ASSERT_FLOAT_WITHIN(expected_ms-0.01, expected_ms+0.01, timeout_ms);
}