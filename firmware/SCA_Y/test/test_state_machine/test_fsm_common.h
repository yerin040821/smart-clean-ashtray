/**
 * @file test_fsm_common.h
 * @brief Common Test Macros and Helpers for APL (Application Layer) FSM Verification
 * @details Industry-standard hierarchical test harness matching ASPICE SWE.4 requirements.
 *          Provides clean parametric step verification macros using Unity framework.
 */

#ifndef TEST_FSM_COMMON_H
#define TEST_FSM_COMMON_H

#include <unity.h>
#include "state_machine.h"
#include "task_state.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Helper context holding transient FSM test execution variables
 */
typedef struct {
    SystemState_t current_state;
    uint32_t      warning_timer_ms;
    uint32_t      step_counter;
} FsmTestContext_t;

/**
 * @brief Initialize FSM test context to clean state
 */
static inline void FSM_TEST_INIT(FsmTestContext_t *ctx, SystemState_t initial_state)
{
    if (ctx)
    {
        ctx->current_state = initial_state;
        ctx->warning_timer_ms = 0;
        ctx->step_counter = 0;
    }
}

/**
 * @brief Step verification helper function
 * @param ctx Pointer to active test context
 * @param raw MQ135 Raw ADC
 * @param base MQ135 Baseline ADC
 * @param temp Current Temperature in C
 * @param temp_rate Temperature Rate of Change (dT/dt) in C/s
 * @param ash_pct Ash level percentage (0~100)
 * @param echo_us Ultrasonic Echo duration in us
 * @param water_empty Water float switch state (true = empty)
 * @param expected_state Expected next FSM state
 * @param step_name Human-readable step description for debugging
 */
static inline void FSM_TEST_STEP_ASSERT(FsmTestContext_t *ctx,
                                        uint16_t raw, uint16_t base,
                                        float temp, float temp_rate,
                                        uint8_t ash_pct, uint32_t echo_us,
                                        bool water_empty,
                                        SystemState_t expected_state,
                                        const char *step_name)
{
    SensorData_t data;
    data.mq135_raw = raw;
    data.mq135_baseline = base;
    data.mq135_diff = (int16_t)raw - (int16_t)base;
    data.temp_current = temp;
    data.temp_rate = temp_rate;
    data.ash_level_pct = ash_pct;
    data.ash_echo_us = echo_us;
    data.water_empty = water_empty;

    ctx->step_counter++;
    SystemState_t next_state = Process_FSM_State_Transition(ctx->current_state, &data, &ctx->warning_timer_ms);

    char msg[128];
    snprintf(msg, sizeof(msg), "[Step %u: %s] Expected State %d, but got %d",
             (unsigned)ctx->step_counter, step_name ? step_name : "UNKNOWN",
             expected_state, next_state);

    TEST_ASSERT_EQUAL_MESSAGE(expected_state, next_state, msg);

    /* Advance context state */
    ctx->current_state = next_state;
}

/**
 * @brief Macro for clean, declarative parametric test step execution
 */
#define APL_VERIFY_STEP(ctx, raw, base, temp, rate, ash, echo, empty, expected, desc) \
    FSM_TEST_STEP_ASSERT(ctx, raw, base, temp, rate, ash, echo, empty, expected, desc)

#ifdef __cplusplus
}
#endif

#endif /* TEST_FSM_COMMON_H */
