/**
 * @file hal_pins.h
 * @brief SCA_S Hardware Pin Mapping (ESP32-S3 DevKitC-1) — overridable via platformio.ini build_flags
 *
 * Fixed by design spec (ashtray-app-spec.md §1-2):
 *   PIR GPIO14 (deep-sleep wake), Deodorizer pump GPIO16, Lid servo GPIO17, OLED SDA GPIO21
 *
 * Chosen for ESP32-S3 constraints (verify against your wiring!):
 *   - ESP32-S3 has NO GPIO22~25 → OLED SCL moved from "GPIO22" (classic ESP32 pinout) to GPIO47
 *   - Analog inputs must be on ADC1 (GPIO1~10): ADC2 is unusable while Wi-Fi is on
 *   - Deep-sleep wake (ext0) requires an RTC GPIO (GPIO0~21) → PIR on GPIO14 ✔
 *   - Avoid strapping pins 0/3/45/46, USB 19/20, flash/PSRAM 26~37
 *   - Pump MOSFET gates need 10kΩ pull-down resistors (pins float during boot)
 *   - HC-SR04 ECHO is 5V → use a voltage divider (e.g. 1kΩ/2kΩ) to 3.3V
 */

#ifndef HAL_PINS_H
#define HAL_PINS_H

/* ── Sensors ─────────────────────────────────────────────────────────── */
#ifndef PIN_MQ135_ANALOG
  #define PIN_MQ135_ANALOG     4   /**< ADC1_CH3 : MQ-135 gas sensor AO */
#endif
#ifndef PIN_FLAME_DO
  #define PIN_FLAME_DO         5   /**< Flame sensor DO (active LOW on KY-026/IR flame modules) */
#endif
#ifndef PIN_FLAME_ACTIVE_LOW
  #define PIN_FLAME_ACTIVE_LOW 1
#endif
#ifndef PIN_ULTRA_TRIG
  #define PIN_ULTRA_TRIG       8   /**< HC-SR04 TRIG */
#endif
#ifndef PIN_ULTRA_ECHO
  #define PIN_ULTRA_ECHO       9   /**< HC-SR04 ECHO (via 5V→3.3V divider) */
#endif
#ifndef PIN_PIR
  #define PIN_PIR             14   /**< HC-SR501 PIR OUT (HIGH = presence) — RTC GPIO, ext0 wake source */
#endif

/* ── Actuators ───────────────────────────────────────────────────────── */
#ifndef PIN_PUMP_EXTINGUISH
  #define PIN_PUMP_EXTINGUISH 15   /**< 소화수 펌프 MOSFET gate */
#endif
#ifndef PIN_PUMP_DEODORIZE
  #define PIN_PUMP_DEODORIZE  16   /**< 탈취제 펌프 MOSFET gate (spec: GPIO21 → 16) */
#endif
#ifndef PIN_SERVO_LID
  #define PIN_SERVO_LID       17   /**< 덮개 서보 PWM 50Hz (spec: GPIO22 → 17) */
#endif
#ifndef PIN_PUMP_CLEAN
  #define PIN_PUMP_CLEAN      18   /**< 청소액 펌프 MOSFET gate */
#endif
#ifndef PIN_BUZZER
  #define PIN_BUZZER          10   /**< Active buzzer (optional) */
#endif
#ifndef PIN_LED_DATA
  #define PIN_LED_DATA        38   /**< WS2812B data (also the on-board RGB LED on DevKitC-1 v1.1) */
#endif
#ifndef PIN_OLED_SDA
  #define PIN_OLED_SDA        21   /**< SSD1306 I2C SDA */
#endif
#ifndef PIN_OLED_SCL
  #define PIN_OLED_SCL        47   /**< SSD1306 I2C SCL (S3 has no GPIO22) */
#endif

#endif /* HAL_PINS_H */
