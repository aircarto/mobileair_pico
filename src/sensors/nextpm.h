#pragma once

#include <cstdint>

namespace nextpm {

/// Status flags from register 0x13 (2 bytes).
struct Status {
    uint16_t raw;
    bool sleep;          // bit 0: sensor in sleep mode
    bool degraded;       // bit 1: minor error, reduced accuracy
    bool not_ready;      // bit 2: starting up (15s after power-on or wake)
    bool heat_error;     // bit 3: humidity > 60% for > 10 min
    bool trh_error;      // bit 4: T/RH readings out of spec
    bool fan_error;      // bit 5: fan speed out of range (still working)
    bool memory_error;   // bit 6: memory access failure
    bool laser_error;    // bit 7: no particle detected for 240s
    bool default_state;  // bit 8: fan stopped, sensor in default/sleep

    bool has_error() const { return raw & 0x01FE; }  // any error bit set (1-8)
    bool is_ready()  const { return !sleep && !not_ready && !default_state; }
};

struct Data {
    float pm1;
    float pm25;
    float pm10;
    float temperature;  // internal sensor, degC
    float humidity;     // internal sensor, %
    Status status;
    bool ok;
};

/// Initialize UART0 for NextPM (GP0 TX, GP1 RX, 115200 8E1, Modbus RTU).
bool init();

/// Read all sensor values (PM1, PM2.5, PM10, temp, humidity, status).
/// Uses 10s average registers in MOBILE mode, 1min average in STATIONARY.
/// Blocks for ~100ms total (multiple Modbus transactions).
/// Also stores the result for get_last().
Data read();

/// Return the last reading (from the most recent read() call).
const Data& get_last();

}  // namespace nextpm
