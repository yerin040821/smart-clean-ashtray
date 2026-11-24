/**
 * @file hal_sensor.cpp
 * @brief SCA_S sensor drivers
 */

#include "hal_sensor.h"
#include "hal_pins.h"
#include <stdio.h>

#if defined(ARDUINO)
  #include <Arduino.h>
#endif

extern "C" void HAL_Sensor_Init(void)
{
#if defined(ARDUINO)
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_MQ135_ANALOG, ADC_11db);   /* 0~3.1V range (use divider if AO is 5V) */
    pinMode(PIN_FLAME_DO, INPUT_PULLUP);
    pinMode(PIN_PIR, INPUT_PULLDOWN);
    pinMode(PIN_ULTRA_TRIG, OUTPUT);
    digitalWrite(PIN_ULTRA_TRIG, LOW);
    pinMode(PIN_ULTRA_ECHO, INPUT);
#endif
    printf("[HAL] Sensors: MQ135=GPIO%d Flame=GPIO%d PIR=GPIO%d US=GPIO%d/%d\n",
           PIN_MQ135_ANALOG, PIN_FLAME_DO, PIN_PIR, PIN_ULTRA_TRIG, PIN_ULTRA_ECHO);
}

extern "C" uint16_t HAL_MQ135_Read(void)
{
#if defined(ARDUINO)
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) sum += (uint32_t)analogRead(PIN_MQ135_ANALOG);
    return (uint16_t)(sum / 8U);
#else
    return 0;
#endif
}

extern "C" bool HAL_Flame_Read(void)
{
#if defined(ARDUINO)
    const int v = digitalRead(PIN_FLAME_DO);
    return PIN_FLAME_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
#else
    return false;
#endif
}

extern "C" bool HAL_PIR_Read(void)
{
#if defined(ARDUINO)
    return digitalRead(PIN_PIR) == HIGH;
#else
    return false;
#endif
}

extern "C" uint16_t HAL_Ultrasonic_ReadMm(void)
{
#if defined(ARDUINO)
    digitalWrite(PIN_ULTRA_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_ULTRA_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRA_TRIG, LOW);
    const unsigned long us = pulseIn(PIN_ULTRA_ECHO, HIGH, 25000UL);   /* ~4m max */
    if (us == 0) return 0;
    return (uint16_t)((us * 343UL) / 2000UL);   /* speed of sound 343 m/s → mm */
#else
    return 0;
#endif
}

extern "C" uint8_t HAL_AshPctFromMm(uint16_t mm)
{
    if (mm == 0 || mm >= ASH_EMPTY_MM) return 0;
    if (mm <= ASH_FULL_MM) return 100;
    return (uint8_t)(((uint32_t)(ASH_EMPTY_MM - mm) * 100U) / (ASH_EMPTY_MM - ASH_FULL_MM));
}
