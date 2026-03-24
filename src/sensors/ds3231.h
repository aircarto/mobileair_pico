#pragma once

#include <cstdint>

namespace ds3231 {

struct DateTime {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;       // day of week (1-7)
    uint8_t date;      // day of month (1-31)
    uint8_t month;     // 1-12
    uint8_t year;      // 0-99 (offset from 2000)
};

struct Data {
    DateTime dt;
    float temperature;  // internal temp sensor, degC (0.25 resolution)
    bool ok;
};

/// Initialize I2C0 on GP20 (SDA) / GP21 (SCL) and probe DS3231 at 0x68.
bool init();

/// Read date/time and temperature from the DS3231.
/// Also stores the result for get_last().
Data read();

/// Return the last reading (from the most recent read() call).
const Data& get_last();

/// Set the DS3231 date/time registers.
bool set_time(const DateTime& dt);

/// Quick connectivity test: probe the I2C address, read status register.
/// Returns true if DS3231 responds correctly.
bool test();

}  // namespace ds3231
