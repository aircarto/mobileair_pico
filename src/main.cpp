#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "pico/cyw43_arch.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "version.h"
#include "wifi/wifi_manager.h"
#include "wifi/wifi_credentials.h"
#include "portal/captive_portal.h"
#include "modem/modem.h"
#include "sensors/nextpm.h"
#include "device_mode.h"
#include "logger.h"
#include "lwip/apps/mdns.h"
#include <cstdio>
#include <cstring>
#include <malloc.h>

// ANSI color codes (compatible PuTTY)
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_BCYAN   "\033[1;36m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BYELLOW "\033[1;33m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_BGREEN  "\033[1;32m"
#define ANSI_WHITE   "\033[37m"
#define ANSI_BWHITE  "\033[1;37m"
#define ANSI_DIM     "\033[2m"
#define ANSI_MAGENTA "\033[35m"

// --- State machine ---
enum class AppState {
    INIT,
    CHECK_CREDS,
    TRY_CONNECT,
    START_PORTAL,
    PORTAL_RUNNING,
    PORTAL_CONNECTING,
    CONNECTED
};

static AppState s_state = AppState::INIT;
static WifiCredentials s_creds = {};
static uint32_t s_scan_timer = 0;
static bool s_mdns_initialized = false;

static void start_mdns() {
    if (!s_mdns_initialized) {
        mdns_resp_init();
        s_mdns_initialized = true;
    }
    mdns_resp_add_netif(netif_default, "mobileair");
    printf("[main] mDNS: mobileair.local\n");
}

static void print_banner() {
    // Clear screen + cursor home
    printf("\033[2J\033[H");

    printf(ANSI_BCYAN
        " __  __       _     _ _        _    _\n"
        "|  \\/  | ___ | |__ (_) | ___  / \\  (_)_ __\n"
        "| |\\/| |/ _ \\| '_ \\| | |/ _ \\/  _\\ | | '__|\n"
        "| |  | | (_) | |_) | | |  __/ ___ \\| | |\n"
        "|_|  |_|\\___/|_.__/|_|_|\\___/_/   \\_\\_|_|\n"
        ANSI_RESET);

    printf(ANSI_BYELLOW
        "              _             _    _       ___\n"
        "             | |__  _   _  / \\  (_)_ __ / __| __ _ _ __| |_ ___\n"
        "             | '_ \\| | | |/ _ \\ | | '__| |   / _` | '__| __/ _ \\\n"
        "             | |_) | |_| / ___ \\| | |  | |__| (_| | |  | || (_) |\n"
        "             |_.__/ \\__, /_/  \\_\\_|_|   \\____\\__,_|_|   \\__\\___/\n"
        "                    |___/\n"
        ANSI_RESET);

    printf(ANSI_DIM ANSI_CYAN
        "    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
        ANSI_RESET);
}

static void print_board_info() {
    // Board unique ID
    char board_id[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(board_id, sizeof(board_id));

    // Memory info
    extern char __StackLimit, __bss_end__;
    uint32_t total_heap = &__StackLimit - &__bss_end__;
    struct mallinfo mi = mallinfo();
    uint32_t heap_used = mi.uordblks;
    uint32_t heap_free = total_heap - heap_used;

    // Uptime
    uint64_t uptime_ms = to_ms_since_boot(get_absolute_time());
    uint32_t up_sec = (uint32_t)(uptime_ms / 1000);

    printf(ANSI_BWHITE "    %-18s" ANSI_RESET " %s\n",
           "Board:", PICO_BOARD);
    printf(ANSI_BWHITE "    %-18s" ANSI_RESET " %s\n",
           "Board ID:", board_id);
    printf(ANSI_BWHITE "    %-18s" ANSI_RESET " v%s\n",
           "Firmware:", FW_VERSION);
    printf(ANSI_BWHITE "    %-18s" ANSI_RESET " %u KB\n",
           "Flash:", PICO_FLASH_SIZE_BYTES / 1024);
    printf(ANSI_BWHITE "    %-18s" ANSI_RESET " %lu KB total, "
           ANSI_BGREEN "%lu KB free" ANSI_RESET ", "
           ANSI_BYELLOW "%lu KB used" ANSI_RESET "\n",
           "Heap:", total_heap / 1024, heap_free / 1024, heap_used / 1024);
    printf(ANSI_BWHITE "    %-18s" ANSI_RESET " %lu s\n",
           "Uptime:", up_sec);
#ifdef PICO_RP2350
    printf(ANSI_BWHITE "    %-18s" ANSI_RESET " RP2350 (Arm Cortex-M33)\n",
           "CPU:");
#else
    printf(ANSI_BWHITE "    %-18s" ANSI_RESET " RP2040 (Arm Cortex-M0+)\n",
           "CPU:");
#endif
    printf(ANSI_BWHITE "    %-18s" ANSI_RESET " %u MHz\n",
           "Clock:", clock_get_hz(clk_sys) / 1000000);

    printf(ANSI_DIM ANSI_CYAN
        "    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
        ANSI_RESET);
    printf("\n");
}

// Build HTML for scan results
static void update_networks_html() {
    const WifiScanResults& scan = wifi_manager::get_scan_results();
    if (scan.scanning) return;

    static char html[4096];
    int pos = 0;
    int remain = sizeof(html) - 1;

    for (int i = 0; i < scan.count && remain > 0; i++) {
        const WifiNetwork& net = scan.networks[i];

        // Signal strength indicator
        const char* signal;
        if (net.rssi > -50) signal = "&#9679;&#9679;&#9679;&#9679;"; // excellent
        else if (net.rssi > -60) signal = "&#9679;&#9679;&#9679;&#9675;"; // good
        else if (net.rssi > -70) signal = "&#9679;&#9679;&#9675;&#9675;"; // fair
        else signal = "&#9679;&#9675;&#9675;&#9675;"; // weak

        const char* lock = net.auth ? "&#128274;" : "";

        int n = snprintf(html + pos, remain,
            "<div class=\"network\" data-ssid=\"%s\" data-auth=\"%d\">"
            "<span class=\"lock\">%s</span>"
            "<span class=\"ssid\">%s</span>"
            "<span class=\"signal\">%s %ddBm</span>"
            "</div>",
            net.ssid, net.auth, lock, net.ssid, signal, net.rssi);

        if (n > 0 && n < remain) {
            pos += n;
            remain -= n;
        }
    }

    if (scan.count == 0) {
        int n = snprintf(html + pos, remain,
            "<div class=\"scanning\">Aucun r&eacute;seau trouv&eacute;</div>");
        if (n > 0) pos += n;
    }

    captive_portal::set_networks_html(html, pos);
}

int main() {
    stdio_init_all();
    logger::init();

    // Wait until USB serial is connected (terminal open)
    // Timeout after 5s to allow headless operation
    uint32_t wait_start = to_ms_since_boot(get_absolute_time());
    while (!stdio_usb_connected()) {
        if (to_ms_since_boot(get_absolute_time()) - wait_start > 5000) break;
        sleep_ms(100);
    }
    sleep_ms(200);

    print_banner();
    print_board_info();

    // --- Modem check ---
    modem::init();
    modem::check();
    printf("\n");

    // --- NextPM sensor ---
    nextpm::init();
    printf("\n");

    // Press 'r' within 3s to erase WiFi credentials and force portal
    printf(ANSI_BYELLOW "    Press 'r' within 3s to reset WiFi credentials...\n" ANSI_RESET);
    {
        uint32_t deadline = to_ms_since_boot(get_absolute_time()) + 3000;
        bool reset_requested = false;
        while (to_ms_since_boot(get_absolute_time()) < deadline) {
            int c = getchar_timeout_us(50000); // 50ms
            if (c == 'r' || c == 'R') {
                reset_requested = true;
                break;
            }
        }
        if (reset_requested) {
            printf(ANSI_BYELLOW "[main] Erasing WiFi credentials...\n" ANSI_RESET);
            wifi_credentials::erase();
            printf("[main] Done — starting portal\n");
        }
    }

    while (true) {
        switch (s_state) {

        case AppState::INIT:
            printf("[main] Initializing WiFi...\n");
            if (!wifi_manager::init()) {
                printf("[main] WiFi init failed, retrying in 2s...\n");
                sleep_ms(2000);
                break;
            }
            s_state = AppState::CHECK_CREDS;
            break;

        case AppState::CHECK_CREDS:
            printf("[main] Checking stored credentials...\n");
            if (wifi_credentials::load(s_creds)) {
                printf("[main] Found credentials for '%s'\n", s_creds.ssid);
                s_state = AppState::TRY_CONNECT;
            } else {
                printf("[main] No stored credentials\n");
                s_state = AppState::START_PORTAL;
            }
            break;

        case AppState::TRY_CONNECT:
            printf("[main] Attempting connection to '%s'...\n", s_creds.ssid);
            if (wifi_manager::connect_sta(s_creds.ssid, s_creds.password)) {
                s_state = AppState::CONNECTED;
            } else {
                printf("[main] Connection failed, starting portal\n");
                s_state = AppState::START_PORTAL;
            }
            break;

        case AppState::START_PORTAL:
            printf("[main] Starting captive portal...\n");
            wifi_manager::start_ap();

            if (!captive_portal::start()) {
                printf("[main] Portal start failed, retrying in 2s...\n");
                sleep_ms(2000);
                break;
            }

            // Start initial WiFi scan
            wifi_manager::start_scan();
            s_scan_timer = to_ms_since_boot(get_absolute_time());
            s_state = AppState::PORTAL_RUNNING;
            break;

        case AppState::PORTAL_RUNNING: {
            wifi_manager::poll();

            // Update scan results and refresh periodically
            uint32_t now = to_ms_since_boot(get_absolute_time());
            const WifiScanResults& scan = wifi_manager::get_scan_results();

            if (!scan.scanning) {
                update_networks_html();

                // Rescan every 15 seconds
                if (now - s_scan_timer > 15000) {
                    wifi_manager::start_scan();
                    s_scan_timer = now;
                }
            }

            // Check if user submitted credentials
            if (captive_portal::has_new_credentials()) {
                char ssid[33], pass[65];
                captive_portal::get_credentials(ssid, sizeof(ssid), pass, sizeof(pass));
                strncpy(s_creds.ssid, ssid, sizeof(s_creds.ssid) - 1);
                strncpy(s_creds.password, pass, sizeof(s_creds.password) - 1);

                printf("[main] User submitted: SSID='%s'\n", ssid);
                s_state = AppState::PORTAL_CONNECTING;
            }

            wifi_manager::poll();
            break;
        }

        case AppState::PORTAL_CONNECTING:
            printf("[main] Attempting connection from portal...\n");
            captive_portal::stop();
            wifi_manager::stop_ap();

            if (wifi_manager::connect_sta(s_creds.ssid, s_creds.password)) {
                printf("[main] Connected! Saving credentials.\n");
                wifi_credentials::save(s_creds);
                captive_portal::set_connect_result(true,
                    "Le Pico va red&eacute;marrer en mode connect&eacute;.");
                s_state = AppState::CONNECTED;
            } else {
                printf("[main] Connection failed, restarting portal\n");
                captive_portal::set_connect_result(false,
                    "V&eacute;rifiez le mot de passe et r&eacute;essayez.");
                s_state = AppState::START_PORTAL;
            }
            break;

        case AppState::CONNECTED:
            printf(ANSI_BGREEN "[main] WiFi connected - entering main loop\n" ANSI_RESET);
            printf("[main] SSID: %s\n", s_creds.ssid);

            // Start status web server + mDNS
            captive_portal::start_status(s_creds.ssid);
            start_mdns();

            // Main application loop
            {
                uint32_t next_sensor_read = 0;
                while (true) {
                    wifi_manager::poll();

                    if (!wifi_manager::is_connected()) {
                        printf(ANSI_BYELLOW "[main] WiFi disconnected, retrying...\n" ANSI_RESET);
                        s_state = AppState::TRY_CONNECT;
                        break;
                    }

                    if (captive_portal::should_reboot()) {
                        printf("[main] Rebooting via web UI...\n");
                        for (int i = 0; i < 10; i++) {
                            wifi_manager::poll();
                        }
                        watchdog_reboot(0, 0, 100);
                    }

                    // Read sensors at mode-dependent interval
                    uint32_t now = to_ms_since_boot(get_absolute_time());
                    if (now >= next_sensor_read) {
                        nextpm::Data pm = nextpm::read();
                        if (pm.ok) {
                            printf("[nextpm] PM1=%.1f PM2.5=%.1f PM10=%.1f ug/m3"
                                   "  T=%.1fC H=%.1f%%\n",
                                   pm.pm1, pm.pm25, pm.pm10,
                                   pm.temperature, pm.humidity);
                        } else {
                            printf("[nextpm] Read failed\n");
                        }
                        next_sensor_read = now + device_mode::interval_ms();
                    }

                    wifi_manager::poll();
                }
            }
            break;
        }
    }

    return 0;
}
