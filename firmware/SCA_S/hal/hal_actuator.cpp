/**
 * @file hal_actuator.cpp
 * @brief SCA_S actuator drivers (GPIO MOSFETs, LEDC servo, neopixelWrite, Adafruit SSD1306)
 */

#include "hal_actuator.h"
#include "hal_pins.h"
#include <stdio.h>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>

  static Adafruit_SSD1306 s_oled(128, 64, &Wire, -1);
  static bool s_oled_ok = false;
  static bool s_oled_off = false;
  #if ESP_ARDUINO_VERSION_MAJOR < 3
    static const uint8_t SERVO_CH = 4;
  #endif
#endif

static const int PUMP_PINS[PUMP_COUNT] = {PIN_PUMP_EXTINGUISH, PIN_PUMP_DEODORIZE, PIN_PUMP_CLEAN};
static const char *const PUMP_NAMES[PUMP_COUNT] = {"extinguish", "deodorize", "clean"};
static volatile bool s_pump[PUMP_COUNT] = {false, false, false};
static volatile bool s_lid_open = false;

/* ───────────────────────── Servo (50Hz, 14-bit LEDC) ───────────────────────── */

static void servo_write_us(uint32_t us)
{
#if defined(ARDUINO)
    const uint32_t duty = (us * 16384UL) / 20000UL;   /* 20ms period */
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_SERVO_LID, duty);
  #else
    ledcWrite(SERVO_CH, duty);
  #endif
#else
    (void)us;
#endif
}

extern "C" void HAL_Actuator_Init(void)
{
#if defined(ARDUINO)
    /* STEP 0: all pumps OFF first (safety) */
    for (int i = 0; i < PUMP_COUNT; i++)
    {
        pinMode(PUMP_PINS[i], OUTPUT);
        digitalWrite(PUMP_PINS[i], LOW);
    }
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    Serial.begin(115200);   /* no wait: device wakes from deep sleep every minute */

  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_SERVO_LID, 50, 14);
  #else
    ledcSetup(SERVO_CH, 50, 14);
    ledcAttachPin(PIN_SERVO_LID, SERVO_CH);
  #endif
    servo_write_us(SERVO_CLOSED_US);

    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    s_oled_ok = s_oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    if (s_oled_ok)
    {
        s_oled.clearDisplay();
        s_oled.setTextColor(SSD1306_WHITE);
        s_oled.setTextSize(1);
        s_oled.setCursor(0, 0);
        s_oled.println("Smart Clean Ashtray");
        s_oled.println("SCA_S booting...");
        s_oled.display();
    }
    printf("[HAL] OLED SSD1306 %s (SDA=%d SCL=%d)\n", s_oled_ok ? "OK" : "NOT FOUND", PIN_OLED_SDA, PIN_OLED_SCL);
#endif
    printf("[HAL] Actuators: pumps ext=%d deo=%d clean=%d, servo=%d, led=%d, buzzer=%d\n",
           PIN_PUMP_EXTINGUISH, PIN_PUMP_DEODORIZE, PIN_PUMP_CLEAN, PIN_SERVO_LID, PIN_LED_DATA, PIN_BUZZER);
}

/* ───────────────────────── Pumps ───────────────────────── */

extern "C" void HAL_Pump_Set(Pump_t pump, bool on)
{
    if (pump >= PUMP_COUNT || s_pump[pump] == on) return;
#if defined(ARDUINO)
    digitalWrite(PUMP_PINS[pump], on ? HIGH : LOW);
#endif
    s_pump[pump] = on;
    printf("[ACT_PUMP] %s %s\n", PUMP_NAMES[pump], on ? "ON" : "off");
}

extern "C" bool HAL_Pump_Get(Pump_t pump)
{
    return pump < PUMP_COUNT && s_pump[pump];
}

extern "C" bool HAL_Pump_AnyOn(void)
{
    return s_pump[0] || s_pump[1] || s_pump[2];
}

extern "C" void HAL_Pump_AllOff(void)
{
    for (int i = 0; i < PUMP_COUNT; i++) HAL_Pump_Set((Pump_t)i, false);
}

/* ───────────────────────── Lid ───────────────────────── */

extern "C" void HAL_Lid_Set(bool open)
{
    if (s_lid_open == open) return;
    servo_write_us(open ? SERVO_OPEN_US : SERVO_CLOSED_US);
    s_lid_open = open;
    printf("[ACT_LID] %s\n", open ? "OPEN" : "closed");
}

extern "C" bool HAL_Lid_IsOpen(void)
{
    return s_lid_open;
}

/* ───────────────────────── Buzzer ───────────────────────── */

extern "C" void HAL_Buzzer_SetMode(uint8_t mode)
{
#if defined(ARDUINO)
    digitalWrite(PIN_BUZZER, mode ? HIGH : LOW);
#else
    (void)mode;
#endif
}

/* ───────────────────────── WS2812B ───────────────────────── */

static volatile uint8_t s_idle_r = 0, s_idle_g = 255, s_idle_b = 0, s_bright = 96;
static volatile uint8_t s_cur_r = 0, s_cur_g = 0, s_cur_b = 0;
static bool s_blink = false;

static void led_write(uint8_t r, uint8_t g, uint8_t b, uint8_t bright)
{
    s_cur_r = r; s_cur_g = g; s_cur_b = b;
#if defined(ARDUINO)
    const uint16_t k = (uint16_t)bright + 1U;
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
    rgbLedWrite(PIN_LED_DATA, (uint8_t)((r * k) >> 8), (uint8_t)((g * k) >> 8), (uint8_t)((b * k) >> 8));
    #else
    neopixelWrite(PIN_LED_DATA, (uint8_t)((r * k) >> 8), (uint8_t)((g * k) >> 8), (uint8_t)((b * k) >> 8));
    #endif
#else
    (void)bright;
#endif
}

extern "C" void HAL_LED_SetStateColor(SystemState_t state)
{
    s_blink = !s_blink;
    switch (state)
    {
        case SYS_STATE_IDLE:          led_write(s_idle_r, s_idle_g, s_idle_b, s_bright); break;
        case SYS_STATE_OCCUPIED:      led_write(0, 200, 255, s_bright); break;   /* cyan */
        case SYS_STATE_SMOKING:       led_write(255, 180, 0, s_bright); break;   /* yellow */
        case SYS_STATE_DEODORIZING:   led_write(0, 60, 255, s_bright); break;    /* blue */
        case SYS_STATE_ASH_FULL:      led_write(255, 100, 0, s_bright); break;   /* orange */
        case SYS_STATE_CLEANING:      led_write(255, 255, 255, s_bright); break; /* white */
        case SYS_STATE_WARNING:       s_blink ? led_write(255, 0, 0, s_bright) : led_write(0, 0, 0, 0); break;
        case SYS_STATE_FIRE_DETECTED: s_blink ? led_write(255, 0, 0, 255) : led_write(0, 0, 0, 0); break;
        case SYS_STATE_ERROR:         led_write(160, 0, 255, s_bright); break;   /* purple */
        case SYS_STATE_SLEEP:
        default:                      led_write(0, 0, 0, 0); break;
    }
}

extern "C" void HAL_LED_SetIdleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    s_idle_r = r; s_idle_g = g; s_idle_b = b; s_bright = brightness;
}

extern "C" void HAL_LED_Off(void)
{
    led_write(0, 0, 0, 0);
}

extern "C" void HAL_LED_GetColorHex(char out[8])
{
    snprintf(out, 8, "#%02x%02x%02x", s_cur_r, s_cur_g, s_cur_b);
}

/* ───────────────────────── OLED ───────────────────────── */

extern "C" void HAL_OLED_Update(SystemState_t state, const SensorData_t *d)
{
#if defined(ARDUINO)
    if (!s_oled_ok || !d) return;
    if (s_oled_off) { s_oled.ssd1306_command(SSD1306_DISPLAYON); s_oled_off = false; }
    s_oled.clearDisplay();
    s_oled.setTextSize(1);
    s_oled.setCursor(0, 0);
    s_oled.print("SCA_S ");
    s_oled.println(FSM_StateName(state));
    s_oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    s_oled.setCursor(0, 14);
    s_oled.printf("Gas  %4u (%+d)\n", d->gas_raw, d->gas_diff);
    s_oled.printf("Ash  %3u%%\n", d->ash_pct);
    s_oled.printf("Water%3u%%\n", d->water_pct);
    s_oled.printf("PIR %s  Flame %s\n", d->presence ? "Y" : "-", d->flame ? "Y!" : "-");
    if (state == SYS_STATE_FIRE_DETECTED)
    {
        s_oled.setTextSize(2);
        s_oled.setCursor(16, 48);
        s_oled.print("! FIRE !");
    }
    s_oled.display();
#else
    (void)state; (void)d;
#endif
}

extern "C" void HAL_OLED_Off(void)
{
#if defined(ARDUINO)
    if (s_oled_ok && !s_oled_off) { s_oled.ssd1306_command(SSD1306_DISPLAYOFF); s_oled_off = true; }
#endif
}
