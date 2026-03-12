#pragma once

namespace captive_portal {
    // Start the captive portal (HTTP server + DNS + DHCP) — AP/pairing mode
    bool start();

    // Start the status web server (HTTP only) — connected/STA mode
    bool start_status(const char* ssid);

    // Stop the captive portal (DNS + DHCP, httpd stays alive)
    void stop();

    // Set scan results HTML (called by wifi_manager after scan)
    void set_networks_html(const char* html, int len);

    // Check if credentials were submitted
    bool has_new_credentials();

    // Get submitted SSID/password
    void get_credentials(char* ssid, int ssid_len, char* password, int pass_len);

    // Show connection result page
    void set_connect_result(bool success, const char* message);

    // Check if a reboot was requested via the web UI
    bool should_reboot();
}
