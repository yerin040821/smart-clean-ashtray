/**
 * @file test_fsm.cpp
 * @brief Unified Test Suite Runner for APL State Machine Verification
 * @details Executes Sub UT (Unit Boundary Matrix) and Top UT (Mission Scenario Tests).
 */

#include <unity.h>
#include "test_fsm_common.h"

/* Forward declarations of Sub UTs */
void test_sub_idle_boundaries(void);
void test_sub_smoking_hysteresis(void);
void test_sub_ash_full_boundaries(void);
void test_sub_safety_interlock_priority(void);

/* Forward declarations of Top UTs */
void test_top_scenario_normal_smoking(void);
void test_top_scenario_fire_ignition_and_extinguish(void);
void test_top_scenario_ash_full_and_water_fault(void);

void setUp(void) {}
void tearDown(void) {}

#if defined(ARDUINO) && !defined(NATIVE_TEST)
#include <Arduino.h>
void setup() {
    delay(2000);
    UNITY_BEGIN();

    /* [Sub UT] Unit Boundary Matrix */
    RUN_TEST(test_sub_idle_boundaries);
    RUN_TEST(test_sub_smoking_hysteresis);
    RUN_TEST(test_sub_ash_full_boundaries);
    RUN_TEST(test_sub_safety_interlock_priority);

    /* [Top UT] Mission Profile Scenarios */
    RUN_TEST(test_top_scenario_normal_smoking);
    RUN_TEST(test_top_scenario_fire_ignition_and_extinguish);
    RUN_TEST(test_top_scenario_ash_full_and_water_fault);

    UNITY_END();
}

void loop() {}

#else

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* [Sub UT] Unit Boundary Matrix */
    RUN_TEST(test_sub_idle_boundaries);
    RUN_TEST(test_sub_smoking_hysteresis);
    RUN_TEST(test_sub_ash_full_boundaries);
    RUN_TEST(test_sub_safety_interlock_priority);

    /* [Top UT] Mission Profile Scenarios */
    RUN_TEST(test_top_scenario_normal_smoking);
    RUN_TEST(test_top_scenario_fire_ignition_and_extinguish);
    RUN_TEST(test_top_scenario_ash_full_and_water_fault);

    return UNITY_END();
}
#endif
