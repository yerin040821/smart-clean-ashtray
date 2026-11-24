/**
 * @file test_fsm.c
 * @brief SCA_S 10-State FSM unit tests (Unity) — `pio test -e native`
 */

#include <unity.h>
#include <string.h>
#include "state_machine.h"

static FsmContext_t ctx;
static SensorData_t d;

void setUp(void)
{
    FSM_Init(&ctx);
    memset(&d, 0, sizeof(d));
    d.water_pct = 100;
}

void tearDown(void) {}

/* run n steps of SENSOR_PERIOD_MS with the current snapshot */
static SystemState_t run(uint32_t ms, uint32_t req)
{
    SystemState_t s = ctx.state;
    for (uint32_t t = 0; t < ms; t += SENSOR_PERIOD_MS)
    {
        s = FSM_Step(&ctx, &d, req, SENSOR_PERIOD_MS);
        req = 0;
    }
    return s;
}

static void test_presence_opens_lid_and_closes_after_absence(void)
{
    d.presence = true;
    TEST_ASSERT_EQUAL(SYS_STATE_OCCUPIED, run(500, 0));
    ActuatorCmd_t o; FSM_Outputs(ctx.state, &o);
    TEST_ASSERT_TRUE(o.lid_open);
    d.presence = false;
    TEST_ASSERT_EQUAL(SYS_STATE_OCCUPIED, run(OCCUPIED_ABSENT_MS - 1000, 0));
    TEST_ASSERT_EQUAL(SYS_STATE_IDLE, run(1500, 0));
}

static void test_smoking_then_deodorize_then_idle(void)
{
    d.presence = true;
    run(500, 0);
    d.gas_diff = 120;
    TEST_ASSERT_EQUAL(SYS_STATE_SMOKING, run(500, 0));
    d.gas_diff = 10; d.presence = false;
    TEST_ASSERT_EQUAL(SYS_STATE_DEODORIZING, run(500, 0));
    ActuatorCmd_t o; FSM_Outputs(ctx.state, &o);
    TEST_ASSERT_TRUE(o.deodorize_on);
    TEST_ASSERT_FALSE(o.lid_open);
    TEST_ASSERT_EQUAL(SYS_STATE_IDLE, run(DEODORIZE_MS + 500, 0));
}

static void test_smoking_hysteresis(void)
{
    d.gas_diff = 60;
    TEST_ASSERT_EQUAL(SYS_STATE_SMOKING, run(500, 0));
    d.gas_diff = 40;   /* between 30 and 50 → still smoking */
    TEST_ASSERT_EQUAL(SYS_STATE_SMOKING, run(2000, 0));
}

static void test_flame_flicker_is_warning_not_fire(void)
{
    d.flame = true;
    TEST_ASSERT_EQUAL(SYS_STATE_WARNING, run(500, 0));
    d.flame = false;
    TEST_ASSERT_EQUAL(SYS_STATE_WARNING, run(WARNING_CLEAR_MS - 500, 0));
    TEST_ASSERT_EQUAL(SYS_STATE_IDLE, run(1000, 0));
}

static void test_flame_confirmed_fire_closes_lid_and_sprays(void)
{
    d.presence = true;
    run(500, 0);                                  /* OCCUPIED, lid open */
    d.flame = true;
    TEST_ASSERT_EQUAL(SYS_STATE_FIRE_DETECTED, run(FIRE_CONFIRM_MS, 0));
    ActuatorCmd_t o; FSM_Outputs(ctx.state, &o);
    TEST_ASSERT_FALSE(o.lid_open);
    TEST_ASSERT_TRUE(o.extinguish_pulse);
    TEST_ASSERT_EQUAL_UINT8(2, o.buzzer_mode);
}

static void test_fire_exits_only_after_clear_window(void)
{
    d.flame = true;
    run(FIRE_CONFIRM_MS, 0);
    d.flame = false;
    TEST_ASSERT_EQUAL(SYS_STATE_FIRE_DETECTED, run(FIRE_CLEAR_MS - 500, 0));
    d.flame = true;                               /* re-ignition resets the window */
    run(500, 0);
    d.flame = false;
    TEST_ASSERT_EQUAL(SYS_STATE_FIRE_DETECTED, run(FIRE_CLEAR_MS - 500, 0));
    TEST_ASSERT_EQUAL(SYS_STATE_DEODORIZING, run(1000, 0));
}

static void test_empty_water_estimate_never_blocks_fire_suppression(void)
{
    d.water_pct = 0;                              /* stale estimate (manual refill, no sensor) */
    TEST_ASSERT_EQUAL(SYS_STATE_IDLE, run(2000, 0));
    d.flame = true;
    TEST_ASSERT_EQUAL(SYS_STATE_FIRE_DETECTED, run(FIRE_CONFIRM_MS, 0));
    ActuatorCmd_t o; FSM_Outputs(ctx.state, &o);
    TEST_ASSERT_TRUE(o.extinguish_pulse);
}

static void test_fire_has_priority_over_sensor_fault(void)
{
    d.sensor_fault = true;
    TEST_ASSERT_EQUAL(SYS_STATE_ERROR, run(500, 0));
    d.flame = true;
    TEST_ASSERT_EQUAL(SYS_STATE_FIRE_DETECTED, run(FIRE_CONFIRM_MS, 0));
    d.flame = false;
    TEST_ASSERT_EQUAL(SYS_STATE_ERROR, run(FIRE_CLEAR_MS + 500, 0));
}

static void test_sensor_fault_error_and_recovery(void)
{
    d.sensor_fault = true;
    TEST_ASSERT_EQUAL(SYS_STATE_ERROR, run(500, 0));
    d.presence = true;                            /* usage ignored while sensor is faulty */
    TEST_ASSERT_EQUAL(SYS_STATE_ERROR, run(1000, 0));
    d.sensor_fault = false; d.presence = false;
    TEST_ASSERT_EQUAL(SYS_STATE_IDLE, run(500, 0));
}

static void test_ash_full_and_clear(void)
{
    d.ash_pct = 85;
    TEST_ASSERT_EQUAL(SYS_STATE_ASH_FULL, run(500, 0));
    d.ash_pct = 75;                               /* hysteresis */
    TEST_ASSERT_EQUAL(SYS_STATE_ASH_FULL, run(500, 0));
    d.ash_pct = 20;
    TEST_ASSERT_EQUAL(SYS_STATE_IDLE, run(500, 0));
}

static void test_clean_request_runs_cycle(void)
{
    TEST_ASSERT_EQUAL(SYS_STATE_CLEANING, run(500, FSM_REQ_CLEAN));
    d.presence = true;                            /* ignored while cleaning */
    TEST_ASSERT_EQUAL(SYS_STATE_CLEANING, run(CLEANING_MS - 1000, 0));
    d.presence = false;
    TEST_ASSERT_EQUAL(SYS_STATE_IDLE, run(1500, 0));
}

static void test_sleep_only_from_idle_and_wakes_on_presence(void)
{
    d.presence = true;
    run(500, 0);
    TEST_ASSERT_EQUAL(SYS_STATE_OCCUPIED, run(500, FSM_REQ_SLEEP));   /* refused when occupied */
    d.presence = false;
    run(OCCUPIED_ABSENT_MS + 500, 0);
    TEST_ASSERT_EQUAL(SYS_STATE_SLEEP, run(500, FSM_REQ_SLEEP));
    ActuatorCmd_t o; FSM_Outputs(ctx.state, &o);
    TEST_ASSERT_FALSE(o.lid_open);
    d.presence = true;
    TEST_ASSERT_EQUAL(SYS_STATE_OCCUPIED, run(500, 0));
}

static void test_flame_during_sleep_wakes_to_warning(void)
{
    TEST_ASSERT_EQUAL(SYS_STATE_SLEEP, run(500, FSM_REQ_SLEEP));
    d.flame = true;
    TEST_ASSERT_EQUAL(SYS_STATE_WARNING, run(500, 0));
}

static void test_gas_spike_is_warning(void)
{
    d.gas_diff = GAS_SPIKE_THRESHOLD;
    TEST_ASSERT_EQUAL(SYS_STATE_WARNING, run(500, 0));
}

static void test_state_names_cover_all_states(void)
{
    for (int s = 0; s < SYS_STATE_COUNT; s++)
        TEST_ASSERT_NOT_EQUAL(0, strcmp(FSM_StateName((SystemState_t)s), "UNKNOWN"));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", FSM_StateName(SYS_STATE_COUNT));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_presence_opens_lid_and_closes_after_absence);
    RUN_TEST(test_smoking_then_deodorize_then_idle);
    RUN_TEST(test_smoking_hysteresis);
    RUN_TEST(test_flame_flicker_is_warning_not_fire);
    RUN_TEST(test_flame_confirmed_fire_closes_lid_and_sprays);
    RUN_TEST(test_fire_exits_only_after_clear_window);
    RUN_TEST(test_empty_water_estimate_never_blocks_fire_suppression);
    RUN_TEST(test_fire_has_priority_over_sensor_fault);
    RUN_TEST(test_sensor_fault_error_and_recovery);
    RUN_TEST(test_ash_full_and_clear);
    RUN_TEST(test_clean_request_runs_cycle);
    RUN_TEST(test_sleep_only_from_idle_and_wakes_on_presence);
    RUN_TEST(test_flame_during_sleep_wakes_to_warning);
    RUN_TEST(test_gas_spike_is_warning);
    RUN_TEST(test_state_names_cover_all_states);
    return UNITY_END();
}
