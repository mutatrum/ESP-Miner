#include "unity.h"
#include "TPS546.h"

TEST_CASE("float_2_slinear11 round trip", "[slinear11]")
{
    float test_values[] = {0.0f, 1.0f, 10.0f, 100.0f, 0.5f, 0.25f, 2.5f, -1.0f, -10.0f, -0.5f};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < num_tests; i++) {
        float original = test_values[i];
        uint16_t encoded = float_2_slinear11(original);
        float decoded = slinear11_2_float(encoded);

        // Allow some tolerance due to quantization
        TEST_ASSERT_FLOAT_WITHIN(0.01f * fabsf(original), original, decoded);
    }
}

TEST_CASE("slinear11_2_float specific values", "[slinear11]")
{
    // Test known values
    // 1.0: mantissa 512, exp 9, 512 * 2^-9 = 1
    uint16_t val_1 = 0x4A00; // exp 9 (0x9<<11=0x4800), mant 512 (0x200)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, slinear11_2_float(val_1));

    // 10.0: mantissa 640, exp 6, 640 * 2^-6 = 10
    uint16_t val_10 = 0x3280; // exp 6 (0x6<<11=0x3000), mant 640 (0x280)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, slinear11_2_float(val_10));

    // -1.0: mantissa -512, exp 9
    uint16_t val_neg1 = 0x4E00; // exp 9, mant 0x400 | 512 = 0x600
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, slinear11_2_float(val_neg1));
}

TEST_CASE("float_2_slinear11 specific values", "[slinear11]")
{
    // Test encoding
    TEST_ASSERT_EQUAL_UINT16(0x4A00, float_2_slinear11(1.0f));
    TEST_ASSERT_EQUAL_UINT16(0x3280, float_2_slinear11(10.0f));
    TEST_ASSERT_EQUAL_UINT16(0x4E00, float_2_slinear11(-1.0f));
}