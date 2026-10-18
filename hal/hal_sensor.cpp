#include "hal_sensor.h"
#include "hal_pins.h"
#include <stdio.h>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include <DHTesp.h>
  #include <esp_task_wdt.h>   /* esp_task_wdt_reset() for watchdog feed */

  /* DHTesp 전용 객체 선언 */
  static DHTesp s_dht;

  /* Last valid DHT22 reading cache (for fallback on read error) */
  static float s_last_temp     = 25.0f;
  static float s_last_humidity = 50.0f;
#endif

/* Mock fallback values for non-Arduino builds */
static uint8_t s_mock_ash_pct     = 15;
static bool    s_mock_water_empty = false;

/* ============================================================
 * HAL_Sensor_Init
 * ============================================================ */
extern "C" void HAL_Sensor_Init(void)
{
#if defined(ARDUINO)
    /* MQ-135: ADC 12-bit resolution */
    analogReadResolution(12);

    /* HC-SR04 ultrasonic */
    pinMode(PIN_ULTRA_TRIG, OUTPUT);
    pinMode(PIN_ULTRA_ECHO, INPUT);
    digitalWrite(PIN_ULTRA_TRIG, LOW);

    /* Float switch: Active LOW, internal pull-up */
    pinMode(PIN_WATER_LEVEL, INPUT_PULLUP);

    /* AM2302 / DHT22 Data Pin: Input with pullup */
    pinMode(PIN_DHT_DATA, INPUT_PULLUP);

    /* DHTesp 초기화 (AM2302/DHT22 지원) */
    s_dht.setup(PIN_DHT_DATA, DHTesp::AM2302);
#endif
    printf("[HAL] Sensors Init: MQ135=GPIO%d, DHT22=GPIO%d, Ultra=GPIO%d/%d, Float=GPIO%d\n",
           PIN_MQ135_ANALOG, PIN_DHT_DATA, PIN_ULTRA_TRIG, PIN_ULTRA_ECHO, PIN_WATER_LEVEL);
}

/* ============================================================
 * HAL_MQ135_Read - Gas Sensor ADC (0~4095)
 * ============================================================ */
extern "C" uint16_t HAL_MQ135_Read(void)
{
#if defined(ARDUINO)
    return (uint16_t)analogRead(PIN_MQ135_ANALOG);
#else
    static uint32_t cnt = 0;
    cnt++;
    return 300 + (uint16_t)(cnt % 5);
#endif
}

/* ============================================================
 * Robust AM2302 (DHT22) Bit-Banging Driver for ESP32-S3
 * ============================================================ */
#if defined(ARDUINO)
static bool read_am2302_raw(float *out_temp, float *out_humidity)
{
    uint8_t data[5] = {0, 0, 0, 0, 0};
    uint8_t pin = PIN_DHT_DATA;

    /* 1. Host Start Signal: Pull Low for 18ms, then High for 30us */
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delay(18); // AM2302 requires at least 1ms to 20ms low
    digitalWrite(pin, HIGH);
    delayMicroseconds(30);

    /* 2. Switch to Input with Pull-up */
    pinMode(pin, INPUT_PULLUP);

    /* 3. Wait for Sensor Response: Sensor pulls LOW (80us), then HIGH (80us) */
    uint32_t timeout_us = 0;

    // Wait for pin to go LOW
    timeout_us = micros();
    while (digitalRead(pin) == HIGH)
    {
        if (micros() - timeout_us > 100) return false; // Sensor didn't respond
    }

    // Wait for pin to go HIGH
    timeout_us = micros();
    while (digitalRead(pin) == LOW)
    {
        if (micros() - timeout_us > 120) return false;
    }

    // Wait for pin to go LOW (Start of data transmission)
    timeout_us = micros();
    while (digitalRead(pin) == HIGH)
    {
        if (micros() - timeout_us > 120) return false;
    }

    /* 4. Read 40 Bits (5 Bytes) */
    for (int i = 0; i < 40; i++)
    {
        // Wait for bit start (LOW phase ~50us)
        timeout_us = micros();
        while (digitalRead(pin) == LOW)
        {
            if (micros() - timeout_us > 100) return false;
        }

        // Measure HIGH duration (26-28us = '0', 70us = '1')
        uint32_t t_start = micros();
        while (digitalRead(pin) == HIGH)
        {
            if (micros() - t_start > 120) return false;
        }
        uint32_t pulse_len = micros() - t_start;

        data[i / 8] <<= 1;
        if (pulse_len > 40) // Threshold between 28us and 70us
        {
            data[i / 8] |= 1;
        }
    }

    /* 5. Checksum verification */
    uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (data[4] != checksum)
    {
        printf("[HAL_WARN] AM2302 Checksum Error (calc=%d, got=%d)\n", checksum, data[4]);
        return false;
    }

    /* 6. Convert Raw to Humidity & Temperature */
    float hum = (float)((data[0] << 8) | data[1]) * 0.1f;
    int16_t raw_temp = (int16_t)(((data[2] & 0x7F) << 8) | data[3]);
    float temp = (float)raw_temp * 0.1f;
    if (data[2] & 0x80) temp = -temp; // Negative temperature flag

    if (out_temp)     *out_temp     = temp;
    if (out_humidity) *out_humidity = hum;

    return true;
}
#endif

extern "C" void HAL_DHT22_Read(float *temp, float *humidity)
{
#if defined(ARDUINO)
    static uint32_t s_last_dht_ms = 0;
    uint32_t now_ms = (uint32_t)millis();

    if (now_ms - s_last_dht_ms >= 2000U)
    {
        s_last_dht_ms = now_ms;
        float t = 0.0f, h = 0.0f;

        if (read_am2302_raw(&t, &h))
        {
            s_last_temp     = t;
            s_last_humidity = h;
        }
        else
        {
            int pin_level = digitalRead(PIN_DHT_DATA);
            printf("[HAL_WARN] AM2302/DHT22 Error on GPIO%d: TIMEOUT (Pin Level: %s)\n",
                   PIN_DHT_DATA, (pin_level == HIGH) ? "HIGH" : "LOW");
        }
    }

    if (temp)     *temp     = s_last_temp;
    if (humidity) *humidity = s_last_humidity;
#else
    if (temp)     *temp     = 25.0f;
    if (humidity) *humidity = 50.0f;
#endif
}


/* ============================================================
 * HAL_Ultrasonic_ReadLevelPct - HC-SR04 Ash Level %
 * ============================================================ */
static uint32_t s_last_echo_duration_us = 3500;

extern "C" uint32_t HAL_Ultrasonic_ReadRawDurationUs(void)
{
#if defined(ARDUINO)
    return s_last_echo_duration_us;
#else
    return 3500;
#endif
}

extern "C" uint8_t HAL_Ultrasonic_ReadLevelPct(void)
{
#if defined(ARDUINO)
    /* Feed watchdog before blocking pulseIn() call */
    esp_task_wdt_reset();  /* Prevent watchdog timeout during ~30ms blocking */

    /* Trigger 10us pulse */
    digitalWrite(PIN_ULTRA_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_ULTRA_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRA_TRIG, LOW);

    /* Measure echo pulse width, 30ms timeout */
    long duration = pulseIn(PIN_ULTRA_ECHO, HIGH, 30000UL);

    if (duration == 0)
    {
        /* Timeout: sensor not connected or out of range */
        printf("[HAL_WARN] HC-SR04 echo timeout (check TRIG=GPIO%d, ECHO=GPIO%d)\n",
               PIN_ULTRA_TRIG, PIN_ULTRA_ECHO);
        s_last_echo_duration_us = 3500;
        return s_mock_ash_pct;
    }

    s_last_echo_duration_us = (uint32_t)duration;

    float distance_cm = (duration * 0.0343f) / 2.0f;
    printf("[HAL_ULTRA] Echo duration: %ld us | Distance: %.1f cm\n", duration, distance_cm);

    /*
     * Ash level mapping:
     *   distance >= 15cm → empty (0%)
     *   distance <= 3cm  → full (100%)
     */
    if (distance_cm >= 15.0f) return 0;
    if (distance_cm <= 3.0f)  return 100;

    float pct = ((15.0f - distance_cm) / (15.0f - 3.0f)) * 100.0f;
    return (uint8_t)pct;
#else
    s_last_echo_duration_us = 3500;
    return s_mock_ash_pct;
#endif
}

/* ============================================================
 * HAL_FloatSwitch_Read - Water Level (Active LOW)
 * ============================================================ */
extern "C" bool HAL_FloatSwitch_Read(void)
{
#if defined(ARDUINO)
    /* Pull-up: HIGH = water OK, LOW = water empty */
    bool empty = (digitalRead(PIN_WATER_LEVEL) == LOW);
    return empty;
#else
    return s_mock_water_empty;
#endif
}
