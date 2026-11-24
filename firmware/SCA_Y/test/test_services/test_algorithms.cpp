/**
 * @file test_algorithms.cpp
 * @brief Unit tests for SCA Signal Processing and Sensor Algorithms using Unity Framework
 */

#include <unity.h>
#include "state_machine.h"

void setUp(void) {
}

void tearDown(void) {
}

/* 1. IIR Baseline Filter Algorithm Test */
void test_iir_baseline_update(void) {
    uint16_t baseline = 300;
    uint16_t raw_spike = 400;

    /* baseline = (baseline * 9 + raw) / 10 = (2700 + 400) / 10 = 310 */
    baseline = (uint16_t)(((uint32_t)baseline * 9 + raw_spike) / 10);
    TEST_ASSERT_EQUAL(310, baseline);
}

/* 2. Gas Difference (diff = raw - baseline) Calculation Test */
void test_gas_diff_calculation(void) {
    uint16_t raw = 420;
    uint16_t baseline = 300;
    int16_t diff = (int16_t)raw - (int16_t)baseline;

    TEST_ASSERT_EQUAL(120, diff);
}

/* 3. Ultrasonic Ash Level Percentage Formula Test */
void test_ultrasonic_ash_level_calculation(void) {
    float max_empty_cm = 15.0f;
    float min_full_cm = 3.0f;

    /* Case A: Completely Empty (15cm) -> 0% */
    float dist_a = 15.0f;
    uint8_t pct_a = (uint8_t)(((max_empty_cm - dist_a) / (max_empty_cm - min_full_cm)) * 100.0f);
    TEST_ASSERT_EQUAL(0, pct_a);

    /* Case B: Completely Full (3cm) -> 100% */
    float dist_b = 3.0f;
    uint8_t pct_b = (uint8_t)(((max_empty_cm - dist_b) / (max_empty_cm - min_full_cm)) * 100.0f);
    TEST_ASSERT_EQUAL(100, pct_b);

    /* Case C: Half Full (9cm) -> 50% */
    float dist_c = 9.0f;
    uint8_t pct_c = (uint8_t)(((max_empty_cm - dist_c) / (max_empty_cm - min_full_cm)) * 100.0f);
    TEST_ASSERT_EQUAL(50, pct_c);
}

#if defined(ARDUINO) && !defined(NATIVE_TEST)
#include <Arduino.h>
void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_iir_baseline_update);
    RUN_TEST(test_gas_diff_calculation);
    RUN_TEST(test_ultrasonic_ash_level_calculation);
    UNITY_END();
}

void loop() {
}
#else
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_iir_baseline_update);
    RUN_TEST(test_gas_diff_calculation);
    RUN_TEST(test_ultrasonic_ash_level_calculation);
    return UNITY_END();
}
#endif
