#pragma once

#include <cstdint>

enum class DeviceMode {
    MOBILE,      // 10s sensor/send interval
    STATIONARY   // 60s sensor/send interval
};

namespace device_mode {
    DeviceMode get();
    void set(DeviceMode mode);
    uint32_t interval_ms();
    const char* label();
}
