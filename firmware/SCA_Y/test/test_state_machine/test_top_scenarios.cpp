/**
 * @file test_top_scenarios.cpp
 * @brief [Top UT] Lifecycle Scenario Verification (End-to-End Mission Profiles)
 * @details Tests multi-step time-series operational profiles matching real-world events.
 */

#include "test_fsm_common.h"

/* --------------------------------------------------------------------------
 * Top Scenario 1: Normal Smoking Lifecycle
 * Sequence: Standby -> User Exhales Smoke -> Smoke Dissipates -> Standby
 * -------------------------------------------------------------------------- */
void test_top_scenario_normal_smoking(void)
{
    FsmTestContext_t ctx;
    FSM_TEST_INIT(&ctx, SYS_STATE_IDLE);

    /* T=0s: Clean Air Standby */
    APL_VERIFY_STEP(&ctx, 305, 300, 24.0f, 0.0f, 10, 3000, false,
                    SYS_STATE_IDLE, "T=0s: Baseline Standby");

    /* T=1s: Smoke plume arrives (diff = 80 > 50) */
    APL_VERIFY_STEP(&ctx, 380, 300, 24.2f, 0.2f, 15, 3000, false,
                    SYS_STATE_SMOKING, "T=1s: Smoke detected -> Enter SMOKING");

    /* T=2s: Smoke peaks */
    APL_VERIFY_STEP(&ctx, 450, 300, 24.5f, 0.3f, 20, 3000, false,
                    SYS_STATE_SMOKING, "T=2s: Continuous smoking");

    /* T=3s: Air filtration clearing (diff = 40, within deadband) */
    APL_VERIFY_STEP(&ctx, 340, 300, 24.3f, -0.2f, 20, 3000, false,
                    SYS_STATE_SMOKING, "T=3s: Smoke clearing, holding deadband");

    /* T=4s: Clean air fully recovered (diff = 15 < 30) */
    APL_VERIFY_STEP(&ctx, 315, 300, 24.0f, -0.3f, 20, 3000, false,
                    SYS_STATE_IDLE, "T=4s: Fully cleared -> Return to IDLE");
}

/* --------------------------------------------------------------------------
 * Top Scenario 2: Cigarette Butt Flare-up & Fire Extinguishing Lifecycle
 * Sequence: Smoking -> Sudden Temp Rise (dT/dt) -> WARNING -> 3s Overheat ->
 *           FIRE_DETECTED (Mist Pulse) -> Cooled Down -> Standby
 * -------------------------------------------------------------------------- */
void test_top_scenario_fire_ignition_and_extinguish(void)
{
    FsmTestContext_t ctx;
    FSM_TEST_INIT(&ctx, SYS_STATE_IDLE);

    /* Step 1: User smokes */
    APL_VERIFY_STEP(&ctx, 420, 300, 25.0f, 0.0f, 10, 3000, false,
                    SYS_STATE_SMOKING, "Smoking active");

    /* Step 2: Unextinguished butt causes sudden thermal spike (dT/dt = 2.8 > 2.0) */
    APL_VERIFY_STEP(&ctx, 480, 300, 27.8f, 2.8f, 10, 3000, false,
                    SYS_STATE_WARNING, "Thermal spike detected -> Enter WARNING");

    /* Step 3: Temperature crosses 50C for 1st second (timer = 1000ms) */
    APL_VERIFY_STEP(&ctx, 550, 300, 52.0f, 1.0f, 10, 3000, false,
                    SYS_STATE_WARNING, "T=1s Over 50C (Warning hold)");

    /* Step 4: Temperature stays over 50C for 2nd second (timer = 2000ms) */
    APL_VERIFY_STEP(&ctx, 560, 300, 54.0f, 0.5f, 10, 3000, false,
                    SYS_STATE_WARNING, "T=2s Over 50C (Warning hold)");

    /* Step 5: Temperature stays over 50C for 3rd second (timer = 3000ms) -> FIRE! */
    APL_VERIFY_STEP(&ctx, 580, 300, 56.0f, 0.5f, 10, 3000, false,
                    SYS_STATE_FIRE_DETECTED, "T=3s Over 50C -> Escalate to FIRE_DETECTED");

    /* Step 6: Water pump mist pulses, temperature drops to 45C (still above safe 40C) */
    APL_VERIFY_STEP(&ctx, 400, 300, 45.0f, -5.0f, 10, 3000, false,
                    SYS_STATE_FIRE_DETECTED, "Cooling down in progress -> Keep spraying");

    /* Step 7: Fire fully extinguished, temp falls below safe threshold (38C < 40C) */
    APL_VERIFY_STEP(&ctx, 310, 300, 38.0f, -7.0f, 10, 3000, false,
                    SYS_STATE_IDLE, "Extinguished confirmed -> Recover to IDLE");
}

/* --------------------------------------------------------------------------
 * Top Scenario 3: Ash Full & Multi-Fault Safety Interlock Preemption
 * Sequence: Ash Tank Full (ASH_FULL) -> Sudden Water Empty -> ERROR Preempts
 * -------------------------------------------------------------------------- */
void test_top_scenario_ash_full_and_water_fault(void)
{
    FsmTestContext_t ctx;
    FSM_TEST_INIT(&ctx, SYS_STATE_IDLE);

    /* 1. Ashtray fills up (echo <= 300us) */
    APL_VERIFY_STEP(&ctx, 300, 300, 25.0f, 0.0f, 95, 270, false,
                    SYS_STATE_ASH_FULL, "Ash accumulated -> ASH_FULL");

    /* 2. User smokes while full -> smoke mode takes priority */
    APL_VERIFY_STEP(&ctx, 390, 300, 25.0f, 0.0f, 95, 270, false,
                    SYS_STATE_SMOKING, "Smoking while ash full -> SMOKING priority");

    /* 3. Smoke clears -> returns to ASH_FULL, not IDLE */
    APL_VERIFY_STEP(&ctx, 310, 300, 25.0f, 0.0f, 95, 270, false,
                    SYS_STATE_ASH_FULL, "Smoke gone -> Returns back to ASH_FULL");

    /* 4. Water float switch trips -> Safety ERROR must immediately preempt */
    APL_VERIFY_STEP(&ctx, 300, 300, 25.0f, 0.0f, 95, 270, true,
                    SYS_STATE_ERROR, "Water empty -> Preempts ASH_FULL to ERROR");
}
