#include "device_mode.h"
#include <cstdio>

static const uint32_t MOBILE_INTERVAL_MS     = 10000;  // 10 seconds
static const uint32_t STATIONARY_INTERVAL_MS = 60000;  // 60 seconds

static DeviceMode s_mode = DeviceMode::STATIONARY;

namespace device_mode {

DeviceMode get() { return s_mode; }

void set(DeviceMode mode) {
    if (mode != s_mode) {
        s_mode = mode;
        printf("[mode] Switched to %s (%lus interval)\n",
               label(), (unsigned long)(interval_ms() / 1000));
    }
}

uint32_t interval_ms() {
    return s_mode == DeviceMode::MOBILE ? MOBILE_INTERVAL_MS : STATIONARY_INTERVAL_MS;
}

const char* label() {
    return s_mode == DeviceMode::MOBILE ? "Mobile" : "Stationnaire";
}

} // namespace device_mode
