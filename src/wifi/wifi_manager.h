#pragma once

#include <cstdint>
#include <cstring>

static constexpr uint32_t AP_IP        = 0xC0A80401; // 192.168.4.1
static constexpr uint32_t AP_NETMASK   = 0xFFFFFF00; // 255.255.255.0
static constexpr uint32_t AP_GATEWAY   = 0xC0A80401; // 192.168.4.1
static constexpr const char* AP_SSID     = "MobileAir";
static constexpr const char* AP_PASSWORD = "mobileair";

static constexpr int WIFI_SCAN_MAX     = 20;
static constexpr int WIFI_CONNECT_TIMEOUT_MS = 15000;

struct WifiNetwork {
    char ssid[33];
    int8_t rssi;
    uint8_t auth; // 0=open, else secured
};

struct WifiScanResults {
    WifiNetwork networks[WIFI_SCAN_MAX];
    int count;
    bool scanning;
};

namespace wifi_manager {
    // Initialize CYW43 driver
    bool init();

    // Access Point mode
    bool start_ap();
    void stop_ap();

    // Station mode
    bool connect_sta(const char* ssid, const char* password);
    void disconnect_sta();
    bool is_connected();

    // Scanning
    void start_scan();
    const WifiScanResults& get_scan_results();

    // Signal strength of current STA connection (dBm, or -100 on error)
    int get_sta_rssi();

    // Poll (call from main loop)
    void poll();
}
