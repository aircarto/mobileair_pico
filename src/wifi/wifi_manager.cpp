#include "wifi_manager.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <cstdio>
#include <algorithm>

static bool s_initialized = false;
static bool s_ap_active = false;
static bool s_sta_connected = false;
static WifiScanResults s_scan_results = {};

// --- Scan callback ---
static int scan_callback(void* env, const cyw43_ev_scan_result_t* result) {
    if (!result) return 0;

    WifiScanResults* sr = &s_scan_results;
    if (sr->count >= WIFI_SCAN_MAX) return 0;

    // Skip empty SSIDs
    if (result->ssid_len == 0) return 0;

    // Skip duplicates (keep strongest signal)
    for (int i = 0; i < sr->count; i++) {
        if (strncmp(sr->networks[i].ssid, (const char*)result->ssid, result->ssid_len) == 0 &&
            sr->networks[i].ssid[result->ssid_len] == '\0') {
            if (result->rssi > sr->networks[i].rssi) {
                sr->networks[i].rssi = result->rssi;
            }
            return 0;
        }
    }

    WifiNetwork& net = sr->networks[sr->count];
    memset(net.ssid, 0, sizeof(net.ssid));
    memcpy(net.ssid, result->ssid, std::min((size_t)result->ssid_len, sizeof(net.ssid) - 1));
    net.rssi = result->rssi;
    net.auth = result->auth_mode;
    sr->count++;

    return 0;
}

namespace wifi_manager {

bool init() {
    if (s_initialized) return true;

    if (cyw43_arch_init()) {
        printf("[wifi] CYW43 init failed\n");
        return false;
    }

    s_initialized = true;
    printf("[wifi] CYW43 initialized\n");
    return true;
}

bool start_ap() {
    if (!s_initialized) return false;
    if (s_ap_active) return true;

    cyw43_arch_enable_ap_mode(AP_SSID, AP_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);

    // CYW43 driver configures the AP netif internally with CYW43_DEFAULT_IP_AP_ADDRESS
    // (192.168.4.1) — do NOT call netif_set_addr/netif_set_up/netif_set_default here

    s_ap_active = true;
    printf("[wifi] AP started: SSID=%s PASS=%s IP=192.168.4.1\n", AP_SSID, AP_PASSWORD);
    return true;
}

void stop_ap() {
    if (!s_ap_active) return;
    cyw43_arch_disable_ap_mode();
    s_ap_active = false;
    printf("[wifi] AP stopped\n");
}

bool connect_sta(const char* ssid, const char* password) {
    if (!s_initialized) return false;

    printf("[wifi] Connecting to '%s'...\n", ssid);

    cyw43_arch_enable_sta_mode();

    int err = cyw43_arch_wifi_connect_timeout_ms(
        ssid, password, CYW43_AUTH_WPA2_AES_PSK, WIFI_CONNECT_TIMEOUT_MS);

    if (err == 0) {
        s_sta_connected = true;
        printf("[wifi] Connected to '%s'\n", ssid);

        // Print IP with highlight
        const ip4_addr_t* ip = netif_ip4_addr(netif_default);
        printf("\033[1;42;30m >>> IP: %s <<< \033[0m\n", ip4addr_ntoa(ip));
        return true;
    }

    printf("[wifi] Connection failed (err=%d)\n", err);
    s_sta_connected = false;
    return false;
}

void disconnect_sta() {
    if (!s_sta_connected) return;
    cyw43_arch_disable_sta_mode();
    s_sta_connected = false;
    printf("[wifi] STA disconnected\n");
}

bool is_connected() {
    if (!s_sta_connected) return false;
    int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    return status == CYW43_LINK_UP;
}

void start_scan() {
    if (!s_initialized) return;

    s_scan_results.count = 0;
    s_scan_results.scanning = true;

    cyw43_wifi_scan_options_t opts = {};
    int err = cyw43_wifi_scan(&cyw43_state, &opts, nullptr, scan_callback);
    if (err != 0) {
        printf("[wifi] Scan start failed (err=%d)\n", err);
        s_scan_results.scanning = false;
    } else {
        printf("[wifi] Scan started\n");
    }
}

const WifiScanResults& get_scan_results() {
    // Update scanning state
    if (s_scan_results.scanning) {
        if (!cyw43_wifi_scan_active(&cyw43_state)) {
            s_scan_results.scanning = false;
            printf("[wifi] Scan complete: %d networks\n", s_scan_results.count);

            // Sort by signal strength (strongest first)
            for (int i = 0; i < s_scan_results.count - 1; i++) {
                for (int j = i + 1; j < s_scan_results.count; j++) {
                    if (s_scan_results.networks[j].rssi > s_scan_results.networks[i].rssi) {
                        WifiNetwork tmp = s_scan_results.networks[i];
                        s_scan_results.networks[i] = s_scan_results.networks[j];
                        s_scan_results.networks[j] = tmp;
                    }
                }
            }
        }
    }
    return s_scan_results;
}

int get_sta_rssi() {
    int32_t rssi;
    if (cyw43_wifi_get_rssi(&cyw43_state, &rssi) == 0) {
        return (int)rssi;
    }
    return -100;
}

void poll() {
    cyw43_arch_poll();
    cyw43_arch_wait_for_work_until(make_timeout_time_ms(10));
}

} // namespace wifi_manager
