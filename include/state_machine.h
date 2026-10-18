/**
 * @file state_machine.h
 * @brief State Machine Types, Structures, and IPC Queue Declarations for SCA Firmware
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>

#if defined(NATIVE_TEST)
  /* Native PC Test mode: FreeRTOS Queues are bypassed */
  typedef void* QueueHandle_t;
#elif defined(ESP_PLATFORM) || defined(ARDUINO)
  #include "freertos/FreeRTOS.h"
  #include "freertos/queue.h"
#else
  typedef void* QueueHandle_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SCA Embedded System State Enum
 */
typedef enum {
    SYS_STATE_IDLE = 0,        /**< Standby mode: AQI good, green LED, IIR baseline update */
    SYS_STATE_SMOKING,         /**< Smoking mode: Air quality degradation, yellow LED, graph animation */
    SYS_STATE_ASH_FULL,        /**< Ashtray full alert: Ash level high (echo us <= 300us), prompt user to clean */
    SYS_STATE_WARNING,         /**< Warning mode: Fire risk detected (dT/dt spike), red blink LED, buzzer alarm */
    SYS_STATE_FIRE_DETECTED,   /**< Fire Discharge mode: 0.5s Pulse Spray, red fast blink, emergency siren */
    SYS_STATE_ERROR            /**< Error mode: Water empty (float switch LOW), purple LED, pump force lock */
} SystemState_t;

/**
 * @brief Sensor Payload Structure sent from Sensor Task to State Task
 */
typedef struct {
    uint16_t mq135_raw;       /**< Raw ADC reading from MQ-135 */
    uint16_t mq135_baseline;  /**< Adaptive baseline reading */
    int16_t  mq135_diff;      /**< Difference value (raw - baseline) */
    float    temp_current;    /**< Current temperature from DHT22 in deg C */
    float    temp_rate;       /**< Temperature rate of change (dT/dt in deg C/sec) */
    uint8_t  ash_level_pct;   /**< Ashtray ash accumulation level (0 - 100%) */
    uint32_t ash_echo_us;     /**< Ultrasonic raw echo pulse width in microseconds */
    bool     water_empty;     /**< Floater switch status (true: water empty, false: sufficient) */
} SensorData_t;

/**
 * @brief Actuator Command Payload sent from State Task to Actuator Task
 */
typedef struct {
    SystemState_t target_state;  /**< Current active system state */
    bool          pump_pulse_en; /**< Enable 0.5s pulse mist spray loop */
    bool          pump_lock;     /**< Force lock pump to prevent dry-run */
    uint8_t       ash_level_pct; /**< Latest ash level percentage for OLED display */
    uint32_t      ash_echo_us;   /**< Latest ultrasonic echo duration in us */
    int16_t       gas_diff;      /**< Latest gas difference value for OLED graph */
    float         temp_current;  /**< Latest temperature for OLED display */
} ActuatorCmd_t;

/* Threshold and timing definitions matching state transition diagram */
#define SMOKE_THRESHOLD         50     /**< Gas_Diff threshold to trigger SMOKING state */
#define HYSTERESIS_LOW          20     /**< Gas_Diff threshold to return to IDLE state */
#define WARNING_RATE_THRESHOLD 2.0f    /**< Temp_Rate (dT/dt) threshold to trigger WARNING state */
#define FIRE_TEMP_THRESHOLD    50.0f   /**< Temp_Raw threshold to trigger FIRE_DETECTED state */
#define SAFE_TEMP_THRESHOLD    40.0f   /**< Temp_Raw threshold to confirm fire extinguished */
#define FIRE_TIMEOUT_MS        3000    /**< 3-second continuous condition timeout for FIRE_DETECTED */

/* Ashtray Ash Full Notification Thresholds (Parameter-driven) */
#define ASH_FULL_ECHO_US_THRESHOLD   300UL   /**< Ultrasonic echo duration threshold in us (<= 300us => Ash Full) */
#define ASH_FULL_RECOVERY_US_HYST     50UL   /**< Hysteresis in us (> 350us to clear alert) */
#define ASH_FULL_PCT_THRESHOLD        80U    /**< Ash percentage fallback threshold (>= 80%) */

#define SENSOR_SAMPLING_PERIOD_MS 1000  /**< 1-second (1000ms) precise sensor sampling period */

/* Inter-Task Communication Queue Handles */
extern QueueHandle_t g_sensor_queue;
extern QueueHandle_t g_actuator_queue;

#ifdef __cplusplus
}
#endif

#endif /* STATE_MACHINE_H */
