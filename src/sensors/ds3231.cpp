#include "sensors/ds3231.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>

// I2C0 on GP20 (SDA) / GP21 (SCL)
#define RTC_I2C       i2c0
#define RTC_SDA       20
#define RTC_SCL       21
#define RTC_FREQ      100000   // 100 kHz
#define DS3231_ADDR   0x68
#define I2C_TIMEOUT_US 50000  // 50 ms

// DS3231 register addresses
#define REG_SECONDS   0x00
#define REG_TEMP_MSB  0x11
#define REG_STATUS    0x0F

static ds3231::Data s_last = {};
static bool s_initialized = false;

static uint8_t bcd_to_dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static uint8_t dec_to_bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

/// Write `len+1` bytes: register address followed by data.
static bool i2c_write_regs(uint8_t reg, const uint8_t* data, size_t len) {
    uint8_t buf[8];
    if (len + 1 > sizeof(buf)) return false;
    buf[0] = reg;
    memcpy(buf + 1, data, len);
    int ret = i2c_write_timeout_us(RTC_I2C, DS3231_ADDR, buf, len + 1, false, I2C_TIMEOUT_US);
    return ret == (int)(len + 1);
}

/// Write a register address then read `len` bytes (with timeout).
static bool i2c_read_regs(uint8_t reg, uint8_t* buf, size_t len) {
    int ret = i2c_write_timeout_us(RTC_I2C, DS3231_ADDR, &reg, 1, true, I2C_TIMEOUT_US);
    if (ret < 0) return false;
    ret = i2c_read_timeout_us(RTC_I2C, DS3231_ADDR, buf, len, false, I2C_TIMEOUT_US);
    return ret == (int)len;
}

namespace ds3231 {

bool init() {
    printf("[ds3231] Initializing I2C0 (SDA=GP%d, SCL=GP%d, %d kHz)...\n",
           RTC_SDA, RTC_SCL, RTC_FREQ / 1000);

    i2c_init(RTC_I2C, RTC_FREQ);
    gpio_set_function(RTC_SDA, GPIO_FUNC_I2C);
    gpio_set_function(RTC_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(RTC_SDA);
    gpio_pull_up(RTC_SCL);

    s_initialized = true;

    // Probe the device
    if (test()) {
        printf("[ds3231] DS3231 detected at 0x%02X\n", DS3231_ADDR);
        return true;
    } else {
        printf("[ds3231] DS3231 NOT detected at 0x%02X\n", DS3231_ADDR);
        return false;
    }
}

Data read() {
    Data d = {};

    if (!s_initialized) {
        s_last = d;
        return d;
    }

    // Read 7 time registers starting at 0x00
    uint8_t buf[7];
    if (!i2c_read_regs(REG_SECONDS, buf, 7)) {
        s_last = d;
        return d;
    }

    d.dt.seconds = bcd_to_dec(buf[0] & 0x7F);
    d.dt.minutes = bcd_to_dec(buf[1] & 0x7F);
    d.dt.hours   = bcd_to_dec(buf[2] & 0x3F);  // 24h mode
    d.dt.day     = bcd_to_dec(buf[3] & 0x07);
    d.dt.date    = bcd_to_dec(buf[4] & 0x3F);
    d.dt.month   = bcd_to_dec(buf[5] & 0x1F);
    d.dt.year    = bcd_to_dec(buf[6]);

    // Read temperature (2 bytes at 0x11)
    uint8_t temp[2];
    if (i2c_read_regs(REG_TEMP_MSB, temp, 2)) {
        int16_t raw = ((int16_t)(int8_t)temp[0] << 2) | (temp[1] >> 6);
        d.temperature = raw * 0.25f;
    }

    d.ok = true;
    s_last = d;
    return d;
}

const Data& get_last() {
    return s_last;
}

bool set_time(const DateTime& dt) {
    if (!s_initialized) return false;

    uint8_t buf[7];
    buf[0] = dec_to_bcd(dt.seconds);
    buf[1] = dec_to_bcd(dt.minutes);
    buf[2] = dec_to_bcd(dt.hours);    // 24h mode (bit 6 = 0)
    buf[3] = dec_to_bcd(dt.day);
    buf[4] = dec_to_bcd(dt.date);
    buf[5] = dec_to_bcd(dt.month);
    buf[6] = dec_to_bcd(dt.year);

    if (!i2c_write_regs(REG_SECONDS, buf, 7)) return false;

    printf("[ds3231] Time set to %02u/%02u/20%02u %02u:%02u:%02u\n",
           dt.date, dt.month, dt.year, dt.hours, dt.minutes, dt.seconds);
    return true;
}

bool test() {
    if (!s_initialized) return false;

    // Try reading the status register — if the device ACKs, it's there
    uint8_t status;
    return i2c_read_regs(REG_STATUS, &status, 1);
}

}  // namespace ds3231
