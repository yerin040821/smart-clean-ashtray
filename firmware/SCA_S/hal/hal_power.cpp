/**
 * @file hal_power.cpp
 * @brief ESP32-S3 deep sleep with ext0 (PIR) + timer wake and pump-pin hold
 */

#include "hal_power.h"
#include "hal_pins.h"
#include <stdio.h>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include "esp_sleep.h"
  #include "driver/gpio.h"
  #include "driver/rtc_io.h"

  static const gpio_num_t HOLD_PINS[] = {
      (gpio_num_t)PIN_PUMP_EXTINGUISH, (gpio_num_t)PIN_PUMP_DEODORIZE, (gpio_num_t)PIN_PUMP_CLEAN,
      (gpio_num_t)PIN_BUZZER,
  };
#endif

extern "C" WakeReason_t HAL_Power_BootInit(void)
{
#if defined(ARDUINO)
    for (gpio_num_t p : HOLD_PINS) gpio_hold_dis(p);
    gpio_deep_sleep_hold_dis();
    rtc_gpio_deinit((gpio_num_t)PIN_PIR);   /* back to digital GPIO after ext0 wake */

    switch (esp_sleep_get_wakeup_cause())
    {
        case ESP_SLEEP_WAKEUP_EXT0:      return WAKE_PIR;
        case ESP_SLEEP_WAKEUP_TIMER:     return WAKE_TIMER;
        case ESP_SLEEP_WAKEUP_UNDEFINED: return WAKE_POWER_ON;
        default:                         return WAKE_OTHER;
    }
#else
    return WAKE_POWER_ON;
#endif
}

extern "C" void HAL_Power_DeepSleep(uint32_t timer_sec)
{
#if defined(ARDUINO)
    printf("[HAL_PWR] Deep sleep (wake: PIR GPIO%d HIGH or %lus timer)\n", PIN_PIR, (unsigned long)timer_sec);
    fflush(stdout);
    for (gpio_num_t p : HOLD_PINS)
    {
        gpio_set_level(p, 0);
        gpio_hold_en(p);
    }
    gpio_deep_sleep_hold_en();
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PIR, 1);
    rtc_gpio_pulldown_en((gpio_num_t)PIN_PIR);
    rtc_gpio_pullup_dis((gpio_num_t)PIN_PIR);
    esp_sleep_enable_timer_wakeup((uint64_t)timer_sec * 1000000ULL);
    delay(50);
    esp_deep_sleep_start();
#else
    (void)timer_sec;
#endif
}

extern "C" const char *HAL_Power_WakeName(WakeReason_t r)
{
    switch (r)
    {
        case WAKE_POWER_ON: return "power-on";
        case WAKE_PIR:      return "PIR";
        case WAKE_TIMER:    return "timer";
        default:            return "other";
    }
}
