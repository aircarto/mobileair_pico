#pragma once

#include <cstdint>

namespace nextpm {

struct Data {
    float pm1;
    float pm25;
    float pm10;
    float temperature;  // internal sensor, degC
    float humidity;     // internal sensor, %
    uint16_t status;
    bool ok;
};

/// Initialize UART0 for NextPM (GP0 TX, GP1 RX, 115200 8E1, Modbus RTU).
bool init();

/// Read all sensor values (PM1, PM2.5, PM10, temp, humidity, status).
/// Blocks for ~100ms total (multiple Modbus transactions).
/// Also stores the result for get_last().
Data read();

/// Return the last reading (from the most recent read() call).
const Data& get_last();

}  // namespace nextpm
