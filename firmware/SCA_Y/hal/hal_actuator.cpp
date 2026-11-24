/**
 * @file hal_actuator.cpp
 * @brief Hardware Abstraction Layer Actuator Driver (1.3" SSH1106 OLED Dedicated)
 *
 * SPEC:
 *  - OLED: 1.3 inch 128x64 SSH1106 / I2C (7-bit: 0x3C / 8-bit: 0x78)
 *  - VCC: 5V
 *  - SDA: GPIO9, SCL: GPIO10
 */

#include "hal_actuator.h"
#include "hal_pins.h"
#include <stdio.h>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SH110X.h>

  #define SCREEN_WIDTH  128
  #define SCREEN_HEIGHT  64
  #define OLED_RESET     -1

  static Adafruit_SH1106G s_display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
  static bool s_oled_ready = false;
#endif

extern "C" void HAL_Actuator_Init(void)
{
#if defined(ARDUINO)
    /* STEP 0: PUMP / BUZZER / LED → FORCE OFF IMMEDIATELY */
    pinMode(PIN_PUMP_PWM, OUTPUT);
    digitalWrite(PIN_PUMP_PWM, LOW);   /* PUMP OFF - highest priority */

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);     /* BUZZER OFF */

    pinMode(PIN_LED_DATA, OUTPUT);
    digitalWrite(PIN_LED_DATA, LOW);   /* LED OFF */

    /* STEP 1: USB-CDC Serial Init */
    Serial.begin(115200);
    {
        uint32_t t0 = millis();
        while (!Serial && (millis() - t0 < 2000)) { delay(10); }
    }
    delay(200);

    /* STEP 2: I2C for 1.3" SSH1106 OLED */
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.setClock(100000); /* 100kHz standard mode */
    delay(200);

    /* STEP 3: Probe & Init SH1106 (0x3C standard, 0x3D fallback) */
    if (s_display.begin(0x3C, true))
    {
        s_oled_ready = true;
        s_display.setContrast(255);
        s_display.clearDisplay();
        s_display.setTextSize(1);
        s_display.setTextColor(SH110X_WHITE);
        s_display.setCursor(0, 0);
        s_display.println("Smart Clean Ashtray");
        s_display.println("OLED: 1.3\" SSH1106 OK");
        s_display.println("System Running...");
        s_display.display();
        printf("[HAL] OLED Init OK (SSH1106 1.3\" @ 0x3C)\n");
    }
    else if (s_display.begin(0x3D, true))
    {
        s_oled_ready = true;
        s_display.setContrast(255);
        s_display.clearDisplay();
        s_display.setTextSize(1);
        s_display.setTextColor(SH110X_WHITE);
        s_display.setCursor(0, 0);
        s_display.println("Smart Clean Ashtray");
        s_display.println("OLED: 1.3\" SSH1106 OK");
        s_display.println("System Running...");
        s_display.display();
        printf("[HAL] OLED Init OK (SSH1106 1.3\" @ 0x3D)\n");
    }
    else
    {
        s_oled_ready = false;
        printf("[HAL_WARN] SSH1106 OLED not found at 0x3C/0x3D. Will retry in background.\n");
    }
#endif
    printf("[HAL] Actuators Init: Pump=GPIO%d, Buzzer=GPIO%d, LED=GPIO%d\n",
           PIN_PUMP_PWM, PIN_BUZZER, PIN_LED_DATA);
}

/* ====================================================================
 * WS2812B LED (single pixel) — driven by ESP32 core RMT helper neopixelWrite()
 * ==================================================================== */
static volatile uint8_t s_idle_r = 0, s_idle_g = 255, s_idle_b = 0; /* default: green */
static volatile uint8_t s_brightness = 128;
static volatile uint8_t s_cur_r = 0, s_cur_g = 0, s_cur_b = 0;
static bool s_blink_phase = false;

static void led_write_b(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    s_cur_r = r; s_cur_g = g; s_cur_b = b;
#if defined(ARDUINO)
    const uint16_t k = (uint16_t)brightness + 1U;
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
    rgbLedWrite(PIN_LED_DATA, (uint8_t)((r * k) >> 8), (uint8_t)((g * k) >> 8), (uint8_t)((b * k) >> 8));
    #else
    neopixelWrite(PIN_LED_DATA, (uint8_t)((r * k) >> 8), (uint8_t)((g * k) >> 8), (uint8_t)((b * k) >> 8));
    #endif
#endif
}

static void led_write(uint8_t r, uint8_t g, uint8_t b) { led_write_b(r, g, b, s_brightness); }

extern "C" void HAL_LED_SetStateColor(SystemState_t state)
{
    /* Called once per FSM cycle (1s) → blink toggles at 0.5Hz; fast blink handled the same way
       but with full brightness so it is clearly distinguishable. */
    s_blink_phase = !s_blink_phase;
    switch (state)
    {
        case SYS_STATE_IDLE:          led_write(s_idle_r, s_idle_g, s_idle_b); break;     /* Green / user color */
        case SYS_STATE_SMOKING:       led_write(255, 180, 0); break;                     /* Yellow */
        case SYS_STATE_ASH_FULL:      led_write(255, 100, 0); break;                     /* Orange */
        case SYS_STATE_WARNING:       s_blink_phase ? led_write(255, 0, 0) : led_write(0, 0, 0); break; /* Red blink */
        case SYS_STATE_FIRE_DETECTED: s_blink_phase ? led_write_b(255, 0, 0, 255) : led_write_b(0, 0, 0, 255); break; /* Red blink, max */
        case SYS_STATE_ERROR:         led_write(160, 0, 255); break;                     /* Purple */
        default:                      led_write(0, 0, 0); break;
    }
}

extern "C" void HAL_LED_SetIdleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    s_idle_r = r; s_idle_g = g; s_idle_b = b;
    s_brightness = brightness;
}

extern "C" void HAL_LED_GetColorHex(char out[8])
{
    snprintf(out, 8, "#%02x%02x%02x", s_cur_r, s_cur_g, s_cur_b);
}

static volatile bool s_pump_on = false;

extern "C" bool HAL_Pump_GetOutput(void)
{
    return s_pump_on;
}

extern "C" void HAL_Pump_SetOutput(bool enable)
{
#if defined(ARDUINO)
    digitalWrite(PIN_PUMP_PWM, enable ? HIGH : LOW);
#endif
    s_pump_on = enable;
    printf("[ACT_PUMP] %s\n", enable ? ">>> PUMP ON <<<" : "Pump OFF");
}

extern "C" void HAL_Buzzer_SetMode(uint8_t mode)
{
#if defined(ARDUINO)
    digitalWrite(PIN_BUZZER, (mode > 0) ? HIGH : LOW);
#endif
}

/* ====================================================================
 * OLED Display UI Update
 * ==================================================================== */
extern "C" void HAL_OLED_UpdateUI(SystemState_t state, int16_t gas_diff,
                                   float temp_current, uint8_t ash_pct)
{
#if defined(ARDUINO)
    if (!s_oled_ready)
    {
        /* Try auto-detect every 3s if disconnected at boot */
        static uint32_t s_last_probe = 0;
        if (millis() - s_last_probe > 3000)
        {
            s_last_probe = millis();

            /* Full I2C Address Scan (0x08 ~ 0x77) to identify real device address */
            uint8_t found_addr = 0;
            for (uint8_t addr = 8; addr < 120; addr++)
            {
                Wire.beginTransmission(addr);
                if (Wire.endTransmission() == 0)
                {
                    found_addr = addr;
                    printf("[HAL_I2C_SCAN] Device Found at I2C Address: 0x%02X!\n", addr);
                    break;
                }
            }

            if (found_addr > 0)
            {
                if (s_display.begin(found_addr, true))
                {
                    s_oled_ready = true;
                    s_display.setContrast(255);
                    printf("[HAL_OLED] SSH1106 OLED Initialized at 0x%02X!\n", found_addr);
                }
                else
                {
                    printf("[HAL_OLED] Found device at 0x%02X but SH1106 init returned false!\n", found_addr);
                }
            }
            else
            {
                printf("[HAL_OLED_DIAG] No I2C Devices Found on SDA=GPIO%d, SCL=GPIO%d! (Check VCC=5V, GND, SDA/SCL swap)\n",
                       PIN_OLED_SDA, PIN_OLED_SCL);
            }
        }
        return;
    }

    s_display.clearDisplay();
    s_display.setTextSize(1);
    s_display.setTextColor(SH110X_WHITE);

    switch (state)
    {
        case SYS_STATE_IDLE:
            s_display.setCursor(0, 0);  s_display.println("=== SYSTEM IDLE ===");
            s_display.setCursor(0, 16); s_display.print("AQI Status : "); s_display.println("GOOD");
            s_display.print("Temp       : "); s_display.print(temp_current, 1); s_display.println(" C");
            s_display.print("Ash Level  : "); s_display.print(ash_pct); s_display.println("%");
            break;
        case SYS_STATE_ASH_FULL:
            s_display.setCursor(0, 0);  s_display.println("=== ASHTRAY FULL ===");
            s_display.setCursor(0, 16); s_display.println("PLEASE EMPTY TANK!");
            s_display.print("Ash Level  : "); s_display.print(ash_pct); s_display.println("%");
            s_display.println("Cleaning Required");
            break;
        case SYS_STATE_SMOKING:
            s_display.setCursor(0, 0);  s_display.println("=== SMOKING MODE ===");
            s_display.setCursor(0, 16); s_display.print("Air Quality: "); s_display.println("POOR");
            s_display.print("Gas Diff   : +"); s_display.println(gas_diff);
            s_display.print("Ash Level  : "); s_display.print(ash_pct); s_display.println("%");
            break;
        case SYS_STATE_WARNING:
            s_display.setCursor(0, 0);  s_display.println("!! WARNING RISK !!");
            s_display.setCursor(0, 16); s_display.println("FIRE RISK DETECTED!");
            s_display.print("Temp       : "); s_display.print(temp_current, 1); s_display.println(" C");
            s_display.print("Gas Diff   : +"); s_display.println(gas_diff);
            break;
        case SYS_STATE_FIRE_DETECTED:
            s_display.setCursor(0, 0);  s_display.println("!!! FIRE DISCHARGE!!!");
            s_display.setCursor(0, 16); s_display.println("SPRAYING MIST PULSE");
            s_display.print("Temp : "); s_display.print(temp_current, 1); s_display.println(" C");
            s_display.println("Extinguishing...");
            break;
        case SYS_STATE_ERROR:
            s_display.setCursor(0, 0);  s_display.println("=== SYSTEM ERROR ===");
            s_display.setCursor(0, 16); s_display.println("REFILL WATER TANK!");
            s_display.println("PUMP FORCE LOCKED");
            break;
        default: break;
    }
    s_display.display();
#else
    (void)state; (void)gas_diff; (void)temp_current; (void)ash_pct;
#endif
}
