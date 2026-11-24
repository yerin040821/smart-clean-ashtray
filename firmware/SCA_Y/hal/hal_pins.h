/**
 * @file hal_pins.h
 * @brief Smart Clean Ashtray (SCA) Hardware Pin Mapping Definition
 * @details Supports pin overrides directly configured inside platformio.ini via build_flags (-D PIN_XX=YY).
 */

#ifndef HAL_PINS_H
#define HAL_PINS_H

#ifdef __cplusplus
extern "C" {
#endif

/* GPIO Pin Assignments matching schematic diagram (Overridable via platformio.ini) */

#ifndef PIN_LED_DATA
  #define PIN_LED_DATA        4   /**< GPIO4  : WS2812B NeoPixel RGB LED Data Line */
#endif

#ifndef PIN_MQ135_ANALOG
  #define PIN_MQ135_ANALOG    5   /**< GPIO5  : MQ-135 Gas Sensor Analog Input (ADC1_CH4) */
#endif

#ifndef PIN_ULTRA_TRIG
  #define PIN_ULTRA_TRIG      6   /**< GPIO6  : HC-SR04 Ultrasonic Trigger Output */
#endif

#ifndef PIN_ULTRA_ECHO
  #define PIN_ULTRA_ECHO      7   /**< GPIO7  : HC-SR04 Ultrasonic Echo Input */
#endif

#ifndef PIN_WATER_LEVEL
  #define PIN_WATER_LEVEL     8   /**< GPIO8  : Floater Water Level Switch (Pull-up, Active LOW) */
#endif

#ifndef PIN_OLED_SDA
  #define PIN_OLED_SDA        21  /**< GPIO21 : OLED Display I2C SDA */
#endif

#ifndef PIN_OLED_SCL
  #define PIN_OLED_SCL        47  /**< GPIO47 : OLED Display I2C SCL */
#endif

#ifndef PIN_DHT_DATA
  #define PIN_DHT_DATA        14  /**< GPIO14 : DHT22/AM2302 Temperature & Humidity Sensor Data */
#endif

#ifndef PIN_BUZZER
  #define PIN_BUZZER          17  /**< GPIO17 : Warning Alarm Buzzer Output */
#endif

#ifndef PIN_PUMP_PWM
  #define PIN_PUMP_PWM        18  /**< GPIO18 : F5305S MOSFET Module Water Pump Control (PWM) */
#endif

#ifdef __cplusplus
}
#endif

#endif /* HAL_PINS_H */
