/**
 * @file fsm.c
 * @brief SCA_S 10-State FSM — pure transition & output functions (no I/O, unit-testable on PC)
 *
 * Priority (highest first):
 *   1. FIRE_DETECTED  flame continuously ≥ FIRE_CONFIRM_MS
 *   2. WARNING        any flame or gas spike
 *   3. ERROR          gas sensor fault (MQ-135 saturated)
 *
 * The extinguishing-water level is only an estimate (no sensor, manual refill) and is deliberately
 * NOT an FSM input: a stale estimate must never stop the device from spraying on a fire.
 *   4. normal usage   IDLE / OCCUPIED / SMOKING / DEODORIZING / ASH_FULL / CLEANING / SLEEP
 */

#include "state_machine.h"
#include <stddef.h>

static const char *const NAMES[SYS_STATE_COUNT] = {
    "IDLE", "OCCUPIED", "SMOKING", "DEODORIZING", "ASH_FULL",
    "CLEANING", "WARNING", "FIRE_DETECTED", "ERROR", "SLEEP",
};

const char *FSM_StateName(SystemState_t state)
{
    return (state < SYS_STATE_COUNT) ? NAMES[state] : "UNKNOWN";
}

void FSM_Init(FsmContext_t *ctx)
{
    if (!ctx) return;
    ctx->state = SYS_STATE_IDLE;
    ctx->state_ms = 0;
    ctx->flame_ms = 0;
    ctx->clear_ms = 0;
    ctx->absent_ms = 0;
}

static SystemState_t enter(FsmContext_t *ctx, SystemState_t next)
{
    if (ctx->state != next)
    {
        ctx->state = next;
        ctx->state_ms = 0;
    }
    return next;
}

SystemState_t FSM_Step(FsmContext_t *ctx, const SensorData_t *d, uint32_t requests, uint32_t dt_ms)
{
    if (!ctx || !d) return ctx ? ctx->state : SYS_STATE_IDLE;

    ctx->state_ms += dt_ms;
    ctx->flame_ms  = d->flame ? ctx->flame_ms + dt_ms : 0;
    const bool hazard = d->flame || d->gas_diff >= GAS_SPIKE_THRESHOLD;
    ctx->clear_ms  = hazard ? 0 : ctx->clear_ms + dt_ms;
    ctx->absent_ms = d->presence ? 0 : ctx->absent_ms + dt_ms;

    const bool smoke     = d->gas_diff > SMOKE_THRESHOLD;
    const bool smoke_end = d->gas_diff < (SMOKE_THRESHOLD - SMOKE_HYSTERESIS);
    const bool ash_full  = d->ash_pct >= ASH_FULL_PCT;

    /* 1. Fire — highest priority, from any state */
    if (ctx->flame_ms >= FIRE_CONFIRM_MS) return enter(ctx, SYS_STATE_FIRE_DETECTED);
    if (ctx->state == SYS_STATE_FIRE_DETECTED)
    {
        if (ctx->clear_ms < FIRE_CLEAR_MS) return SYS_STATE_FIRE_DETECTED;
        return enter(ctx, d->sensor_fault ? SYS_STATE_ERROR : SYS_STATE_DEODORIZING);   /* ventilate smell */
    }

    /* 2. Warning */
    if (hazard) return enter(ctx, SYS_STATE_WARNING);
    if (ctx->state == SYS_STATE_WARNING)
    {
        if (ctx->clear_ms < WARNING_CLEAR_MS) return SYS_STATE_WARNING;
        return enter(ctx, SYS_STATE_IDLE);
    }

    /* 3. Gas sensor fault */
    if (d->sensor_fault) return enter(ctx, SYS_STATE_ERROR);
    if (ctx->state == SYS_STATE_ERROR) return enter(ctx, SYS_STATE_IDLE);

    /* 4. Normal usage */
    switch (ctx->state)
    {
        case SYS_STATE_SLEEP:
            if (d->presence) return enter(ctx, SYS_STATE_OCCUPIED);
            if (smoke || (requests & FSM_REQ_WAKE)) return enter(ctx, SYS_STATE_IDLE);
            break;

        case SYS_STATE_IDLE:
            if (requests & FSM_REQ_CLEAN) return enter(ctx, SYS_STATE_CLEANING);
            if (smoke)                    return enter(ctx, SYS_STATE_SMOKING);
            if (d->presence)              return enter(ctx, SYS_STATE_OCCUPIED);
            if (ash_full)                 return enter(ctx, SYS_STATE_ASH_FULL);
            if (requests & FSM_REQ_SLEEP) return enter(ctx, SYS_STATE_SLEEP);
            break;

        case SYS_STATE_OCCUPIED:
            if (smoke) return enter(ctx, SYS_STATE_SMOKING);
            if (ctx->absent_ms >= OCCUPIED_ABSENT_MS) return enter(ctx, SYS_STATE_IDLE);
            break;

        case SYS_STATE_SMOKING:
            if (smoke_end) return enter(ctx, SYS_STATE_DEODORIZING);
            break;

        case SYS_STATE_DEODORIZING:
            if (smoke) return enter(ctx, SYS_STATE_SMOKING);
            if (ctx->state_ms >= DEODORIZE_MS) return enter(ctx, ash_full ? SYS_STATE_ASH_FULL : SYS_STATE_IDLE);
            break;

        case SYS_STATE_ASH_FULL:
            if (requests & FSM_REQ_CLEAN)     return enter(ctx, SYS_STATE_CLEANING);
            if (smoke)                        return enter(ctx, SYS_STATE_SMOKING);
            if (d->ash_pct < ASH_CLEAR_PCT)   return enter(ctx, SYS_STATE_IDLE);
            if (d->presence)                  return enter(ctx, SYS_STATE_OCCUPIED);
            if (requests & FSM_REQ_SLEEP)     return enter(ctx, SYS_STATE_SLEEP);
            break;

        case SYS_STATE_CLEANING:
            if (ctx->state_ms >= CLEANING_MS) return enter(ctx, SYS_STATE_IDLE);
            break;

        default:
            return enter(ctx, SYS_STATE_IDLE);
    }
    return ctx->state;
}

void FSM_Outputs(SystemState_t state, ActuatorCmd_t *out)
{
    if (!out) return;
    out->state            = state;
    out->lid_open         = (state == SYS_STATE_OCCUPIED || state == SYS_STATE_SMOKING);
    out->extinguish_pulse = (state == SYS_STATE_FIRE_DETECTED);
    out->deodorize_on     = (state == SYS_STATE_DEODORIZING);
    out->clean_on         = (state == SYS_STATE_CLEANING);
    out->buzzer_mode      = (state == SYS_STATE_FIRE_DETECTED) ? 2 : (state == SYS_STATE_WARNING ? 1 : 0);
}
