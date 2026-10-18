/**
 * @file test_sub_transitions.cpp
 * @brief [Sub UT] State Transition Unit Test Suite (Matrix & Boundary Condition Verification)
 * @details Tests isolated state boundaries, thresholds, and hysteresis recovery in isolation.
 */

#include "test_fsm_common.h"

/* --------------------------------------------------------------------------
 * Sub UT 1: IDLE State Boundary & Transitions
 * -------------------------------------------------------------------------- */
void test_sub_idle_boundaries(void)
{
    FsmTestContext_t ctx;
    FSM_TEST_INIT(&ctx, SYS_STATE_IDLE);

    /* Test 1-1: Exact threshold edge (diff = 50 -> must remain IDLE) */
    APL_VERIFY_STEP(&ctx, 350, 300, 25.0f, 0.0f, 10, 3000, false,
                    SYS_STATE_IDLE, "Smoke threshold boundary at exactly 50");

    /* Test 1-2: Threshold crossed (diff = 51 -> must transition to SMOKING) */
    APL_VERIFY_STEP(&ctx, 351, 300, 25.0f, 0.0f, 10, 3000, false,
                    SYS_STATE_SMOKING, "Smoke threshold exceeded by +1");
}

/* --------------------------------------------------------------------------
 * Sub UT 2: SMOKING State Hysteresis Recovery
 * -------------------------------------------------------------------------- */
void test_sub_smoking_hysteresis(void)
{
    FsmTestContext_t ctx;
    FSM_TEST_INIT(&ctx, SYS_STATE_SMOKING);

    /* Low hysteresis threshold: 50 - 20 = 30.
     * diff = 30 -> still in deadband, must remain SMOKING */
    APL_VERIFY_STEP(&ctx, 330, 300, 25.0f, 0.0f, 10, 3000, false,
                    SYS_STATE_SMOKING, "Hysteresis deadband boundary at diff=30");

    /* diff = 29 -> below deadband, must recover to IDLE */
    APL_VERIFY_STEP(&ctx, 329, 300, 25.0f, 0.0f, 10, 3000, false,
                    SYS_STATE_IDLE, "Hysteresis cleared at diff=29 -> return IDLE");
}

/* --------------------------------------------------------------------------
 * Sub UT 3: ASH_FULL State Threshold & Recovery
 * -------------------------------------------------------------------------- */
void test_sub_ash_full_boundaries(void)
{
    FsmTestContext_t ctx;
    FSM_TEST_INIT(&ctx, SYS_STATE_IDLE);

    /* Echo = 301us -> above threshold, remains IDLE */
    APL_VERIFY_STEP(&ctx, 300, 300, 25.0f, 0.0f, 75, 301, false,
                    SYS_STATE_IDLE, "Echo just above threshold (301us)");

    /* Echo = 300us -> threshold met, must transition to ASH_FULL */
    APL_VERIFY_STEP(&ctx, 300, 300, 25.0f, 0.0f, 85, 300, false,
                    SYS_STATE_ASH_FULL, "Echo threshold met (300us) -> ASH_FULL");

    /* Hysteresis: Recovery threshold = 300 + 50 = 350us.
     * Echo = 350us -> still full */
    APL_VERIFY_STEP(&ctx, 300, 300, 25.0f, 0.0f, 70, 350, false,
                    SYS_STATE_ASH_FULL, "Hysteresis deadband at 350us");

    /* Echo = 351us -> tank emptied, returns to IDLE */
    APL_VERIFY_STEP(&ctx, 300, 300, 25.0f, 0.0f, 20, 351, false,
                    SYS_STATE_IDLE, "Tank emptied (351us) -> return IDLE");
}

/* --------------------------------------------------------------------------
 * Sub UT 4: Safety Error Priority (Float Switch Interlock)
 * -------------------------------------------------------------------------- */
void test_sub_safety_interlock_priority(void)
{
    FsmTestContext_t ctx;

    /* From SMOKING state, sudden water depletion must preempt immediately */
    FSM_TEST_INIT(&ctx, SYS_STATE_SMOKING);
    APL_VERIFY_STEP(&ctx, 400, 300, 25.0f, 0.0f, 10, 3000, true,
                    SYS_STATE_ERROR, "Water empty during SMOKING -> preempt to ERROR");

    /* Water refilled -> must recover to IDLE */
    APL_VERIFY_STEP(&ctx, 300, 300, 25.0f, 0.0f, 10, 3000, false,
                    SYS_STATE_IDLE, "Water refilled -> recover to IDLE");
}
