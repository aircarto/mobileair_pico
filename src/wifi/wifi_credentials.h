#pragma once

#include <cstdint>

static constexpr uint32_t CRED_MAGIC = 0x4D414952; // "MAIR"
static constexpr int CRED_SSID_MAX   = 33;
static constexpr int CRED_PASS_MAX   = 65;

struct WifiCredentials {
    char ssid[CRED_SSID_MAX];
    char password[CRED_PASS_MAX];
};

namespace wifi_credentials {
    // Load credentials from flash. Returns true if valid credentials found.
    bool load(WifiCredentials& creds);

    // Save credentials to flash. Returns true on success.
    bool save(const WifiCredentials& creds);

    // Erase stored credentials.
    bool erase();
}
