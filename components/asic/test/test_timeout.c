#include "unity.h"

#include "asic_common.h"

TEST_CASE("Check asic timeout 1x BM1397", "[common]")
{
    float frequency = 450.0; // MHz
    uint16_t asic_count = 1;
    uint16_t small_cores = 672;
    uint16_t cores = 168;
    uint16_t version_size = 4;
    float timeout_percent = 0.75;
    float default_timeout_ms = 20;

    double timeout_ms = calculate_bm_timeout_ms(frequency, asic_count, small_cores, cores, version_size, timeout_percent, default_timeout_ms);
    double expected_ms = timeout_percent * ( 1<<24 ) / (frequency * 1000) / asic_count;

    TEST_ASSERT_FLOAT_WITHIN(0.01, expected_ms, timeout_ms);
}

TEST_CASE("Check asic timeout 2x BM1370", "[common]")
{
    float frequency = 450.0; // MHz
    uint16_t asic_count = 2;
    uint16_t small_cores = 2040;
    uint16_t cores = 128;
    uint16_t version_size = 0xFFFF;
    float timeout_percent = 0.5;
    float default_timeout_ms = 500;

    double timeout_ms = calculate_bm_timeout_ms(frequency, asic_count, small_cores, cores, version_size, timeout_percent, default_timeout_ms);
    double expected_ms = timeout_percent * (version_size / 16.0) * (1 << 25) / (frequency * 1000) / asic_count;

    TEST_ASSERT_FLOAT_WITHIN(0.01, expected_ms, timeout_ms);
}

TEST_CASE("Check default asic timeout 0x BM1370", "[common]")
{
    float frequency = 450.0; // MHz
    uint16_t asic_count = 0; // 0 chip example
    uint16_t small_cores = 2040;
    uint16_t cores = 128;
    uint16_t version_size = 0xFFFF;
    float timeout_percent = 0.5;
    float default_timeout_ms = 500;

    double timeout_ms = calculate_bm_timeout_ms(frequency, asic_count, small_cores, cores, version_size, timeout_percent, default_timeout_ms);

    TEST_ASSERT_FLOAT_WITHIN(0.01, default_timeout_ms, timeout_ms);
}

TEST_CASE("Check asic timeout 3x BM1370", "[common]")
{
    float frequency = 450.0; // MHz
    uint16_t asic_count = 3;
    uint16_t small_cores = 2040;
    uint16_t cores = 128;
    uint16_t version_size = 0xFF;
    float timeout_percent = 0.5;
    float default_timeout_ms = 500;

    double timeout_ms = calculate_bm_timeout_ms(frequency, asic_count, small_cores, cores, version_size, timeout_percent, default_timeout_ms);
    double expected_ms = timeout_percent * (version_size / 16.0) * (1 << 25) / (frequency * 1000) / (asic_count+1);

    TEST_ASSERT_FLOAT_WITHIN(0.01, expected_ms, timeout_ms);
}

TEST_CASE("Check max asic timeout 1x BM1370", "[common]")
{
    float frequency = 450.0; // MHz
    uint16_t asic_count = 1;
    uint16_t small_cores = 2040;
    uint16_t cores = 128;
    uint16_t version_size = 0xFFFF;
    float timeout_percent = 1.0;
    float default_timeout_ms = 500;

    double timeout_ms = calculate_bm_timeout_ms(frequency, asic_count, small_cores, cores, version_size, timeout_percent, default_timeout_ms);
    double expected_ms = 4096.0 * (1 << 25) / (frequency * 1000) / (asic_count);

    TEST_ASSERT_FLOAT_WITHIN(0.01, expected_ms, timeout_ms);
}