#include "captive_portal.h"
#include "dns_server.h"
#include "dhcp_server.h"
#include "../wifi/wifi_manager.h"
#include "../modem/modem.h"

#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/clocks.h"
#include "version.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "lwip/def.h"
#include "lwip/netif.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <malloc.h>

// --- Mode ---
enum PortalMode {
    MODE_PORTAL,   // AP pairing mode
    MODE_STATUS    // STA connected mode
};

// --- State ---
static PortalMode s_mode = MODE_PORTAL;
static bool s_running = false;
static bool s_httpd_initialized = false;
static dhcp_server_t s_dhcp_server = {};
static bool s_creds_ready = false;
static char s_submitted_ssid[33] = {};
static char s_submitted_pass[65] = {};
static char s_connect_msg[128] = {};
static bool s_connect_success = false;
static bool s_has_connect_result = false;
static bool s_reboot_requested = false;
static char s_status_ssid[33] = {};

// --- Networks HTML fragment (filled by set_networks_html) ---
static char s_networks_html[4096] = {};
static int s_networks_html_len = 0;

// =====================================================================
// Portal page (AP mode) — WiFi configuration
// =====================================================================

static const char PAGE_HEADER[] = R"(<!DOCTYPE html>
<html lang="fr"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MobileAir - Configuration WiFi</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;min-height:100vh;padding:20px}
.container{max-width:420px;width:100%;margin:0 auto}
h1{color:#00d4ff;text-align:center;margin:20px 0 8px;font-size:1.5em}
.subtitle{text-align:center;color:#888;margin-bottom:24px;font-size:0.9em}
.card{background:#16213e;border-radius:12px;padding:20px;margin-bottom:16px;border:1px solid #0f3460}
.network{display:flex;align-items:center;padding:12px;margin:4px 0;border-radius:8px;cursor:pointer;transition:background 0.2s}
.network:hover{background:#0f3460}
.network.selected{background:#0f3460;border:1px solid #00d4ff}
.ssid{flex:1;font-size:1em}
.signal{font-size:0.8em;color:#888;margin-left:8px}
.lock{margin-right:8px;font-size:0.9em}
input[type=password],input[type=text]{width:100%;padding:12px;border-radius:8px;border:1px solid #0f3460;background:#1a1a2e;color:#e0e0e0;font-size:1em;margin-top:8px}
input:focus{outline:none;border-color:#00d4ff}
button{width:100%;padding:14px;border-radius:8px;border:none;background:#00d4ff;color:#1a1a2e;font-size:1em;font-weight:700;cursor:pointer;margin-top:16px;transition:background 0.2s}
button:hover{background:#00b8d9}
button:disabled{background:#555;cursor:not-allowed}
.msg-ok{color:#00ff88;text-align:center;padding:16px}
.msg-err{color:#ff6b6b;text-align:center;padding:16px}
.scanning{text-align:center;color:#888;padding:20px}
.footer{text-align:center;color:#555;font-size:0.75em;margin-top:24px}
</style></head><body><div class="container">
<h1>MobileAir</h1>
<p class="subtitle">Configuration WiFi</p>
)";

static const char PAGE_FOOTER[] = R"(
<div class="footer">MobileAir by AirCarto</div>
</div></body></html>)";

static const char PAGE_FORM_PRE[] = R"(
<form method="POST" action="/connect" id="wf">
<div class="card">
<input type="hidden" name="ssid" id="ssid_field" value="">
)";

static const char PAGE_FORM_POST[] = R"(
</div>
<div class="card">
<label>Mot de passe WiFi</label>
<input type="password" name="password" id="pass" placeholder="Entrez le mot de passe">
</div>
<button type="submit" id="btn" disabled>Connexion</button>
</form>
<script>
document.querySelectorAll('.network').forEach(n=>{
 n.onclick=()=>{
  document.querySelectorAll('.network').forEach(x=>x.classList.remove('selected'));
  n.classList.add('selected');
  document.getElementById('ssid_field').value=n.dataset.ssid;
  document.getElementById('btn').disabled=false;
  if(!parseInt(n.dataset.auth))document.getElementById('pass').value='';
 }
});
document.getElementById('wf').onsubmit=()=>{
 document.getElementById('btn').disabled=true;
 document.getElementById('btn').textContent='Connexion en cours...';
};
</script>
)";

static const char PAGE_RESULT_OK[] = R"(
<div class="card">
<div class="msg-ok">
<p style="font-size:2em;margin-bottom:12px">&#10004;</p>
<p><strong>Connect&eacute; avec succ&egrave;s !</strong></p>
<p style="margin-top:8px;color:#888">)";

static const char PAGE_RESULT_FAIL[] = R"(
<div class="card">
<div class="msg-err">
<p style="font-size:2em;margin-bottom:12px">&#10008;</p>
<p><strong>&Eacute;chec de connexion</strong></p>
<p style="margin-top:8px;color:#888">)";

static const char PAGE_RESULT_CLOSE[] = R"(</p>
</div></div>
<a href="/"><button>R&eacute;essayer</button></a>
)";

// =====================================================================
// Status page (STA mode) — Dashboard
// =====================================================================

static const char STATUS_PAGE_FMT[] =
    "<!DOCTYPE html>"
    "<html lang=\"fr\"><head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<meta http-equiv=\"refresh\" content=\"30\">"
    "<title>MobileAir</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;min-height:100vh;padding:20px}"
    ".c{max-width:900px;width:100%%;margin:0 auto}"
    ".hdr{text-align:center;margin-bottom:24px}"
    "h1{color:#00d4ff;margin:20px 0 8px;font-size:1.5em}"
    ".sub{color:#888;font-size:0.9em}"
    ".grid{display:grid;grid-template-columns:1fr;gap:16px}"
    "@media(min-width:600px){.grid{grid-template-columns:repeat(2,1fr)}}"
    "@media(min-width:900px){.grid{grid-template-columns:repeat(3,1fr)}}"
    ".cd{background:#16213e;border-radius:12px;padding:20px;border:1px solid #0f3460}"
    ".cd h2{color:#00d4ff;font-size:1.1em;margin-bottom:12px}"
    ".r{display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid #0f3460}"
    ".r:last-child{border-bottom:none}"
    ".l{color:#888}.v{color:#e0e0e0;font-weight:500}"
    ".ok{color:#00ff88}"
    "button{width:100%%;padding:14px;border-radius:8px;border:none;"
    "background:#ff6b6b;color:#fff;font-size:1em;font-weight:700;"
    "cursor:pointer;margin-top:16px;transition:background 0.2s}"
    "button:hover{background:#ff5252}"
    "button:disabled{background:#555;cursor:not-allowed}"
    ".ft{text-align:center;color:#555;font-size:0.75em;margin-top:24px;grid-column:1/-1}"
    ".act{grid-column:1/-1}"
    "</style></head><body><div class=\"c\">"
    "<div class=\"hdr\"><h1>MobileAir</h1>"
    "<p class=\"sub\">Tableau de bord</p></div>"
    "<div class=\"grid\">"

    "<div class=\"cd\"><h2>&#128225; WiFi</h2>"
    "<div class=\"r\"><span class=\"l\">SSID</span><span class=\"v\">%s</span></div>"
    "<div class=\"r\"><span class=\"l\">Signal</span><span class=\"v\">%s %d dBm</span></div>"
    "<div class=\"r\"><span class=\"l\">IP</span><span class=\"v ok\">%s</span></div>"
    "</div>"

    "<div class=\"cd\"><h2>&#9881; Syst&egrave;me</h2>"
    "<div class=\"r\"><span class=\"l\">Board</span><span class=\"v\">%s</span></div>"
    "<div class=\"r\"><span class=\"l\">ID</span><span class=\"v\" style=\"font-size:0.75em\">%s</span></div>"
    "<div class=\"r\"><span class=\"l\">Firmware</span><span class=\"v\">v%s</span></div>"
    "<div class=\"r\"><span class=\"l\">CPU</span><span class=\"v\">%s</span></div>"
    "<div class=\"r\"><span class=\"l\">Horloge</span><span class=\"v\">%u MHz</span></div>"
    "<div class=\"r\"><span class=\"l\">Flash</span><span class=\"v\">%u KB</span></div>"
    "<div class=\"r\"><span class=\"l\">Heap libre</span><span class=\"v ok\">%lu / %lu KB</span></div>"
    "<div class=\"r\"><span class=\"l\">Uptime</span><span class=\"v\">%s</span></div>"
    "</div>"

    "<div class=\"cd\"><h2>&#128225; Modem</h2>"
    "<div class=\"r\"><span class=\"l\">Modem</span>"
    "<span class=\"v\" id=\"ms\">&mdash;</span></div>"
    "<div class=\"r\"><span class=\"l\">Carte SIM</span>"
    "<span class=\"v\" id=\"ss\">&mdash;</span></div>"
    "<button type=\"button\" onclick=\"testModem()\" id=\"mb\" "
    "style=\"background:#00d4ff;color:#1a1a2e;margin-top:8px\">"
    "Tester le modem</button>"
    "<button type=\"button\" onclick=\"testSim()\" id=\"sb\" "
    "style=\"background:#00d4ff;color:#1a1a2e;margin-top:8px\">"
    "Tester carte SIM</button>"
    "<div class=\"r\" style=\"margin-top:8px\"><span class=\"l\">LED status</span>"
    "<span class=\"v\" id=\"ls\">&mdash;</span></div>"
    "<button type=\"button\" onclick=\"toggleLed()\" id=\"lb\" "
    "style=\"background:#00d4ff;color:#1a1a2e;margin-top:8px\">"
    "Activer LED status</button>"
    "</div>"

    "<form method=\"POST\" action=\"/reboot\" class=\"act\">"
    "<button type=\"submit\" onclick=\"return confirm('Red\\u00e9marrer le Pico ?')\">"
    "&#x1F504; Red&eacute;marrer</button>"
    "</form>"
    "<div class=\"ft\">MobileAir by AirCarto &bull; mobileair.local</div>"
    "</div>"

    "<script>"
    "function testModem(){"
    "var b=document.getElementById('mb'),s=document.getElementById('ms');"
    "b.disabled=true;b.textContent='Test en cours...';"
    "fetch('/modem-test').then(function(x){return x.json()}).then(function(d){"
    "s.textContent=d.response;s.className='v '+(d.ok?'ok':'');"
    "b.disabled=false;b.textContent='Tester le modem';"
    "}).catch(function(){"
    "s.textContent='Erreur r\\u00e9seau';s.className='v';"
    "b.disabled=false;b.textContent='Tester le modem';"
    "});}"
    "function testSim(){"
    "var b=document.getElementById('sb'),s=document.getElementById('ss');"
    "b.disabled=true;b.textContent='Test en cours...';"
    "fetch('/sim-test').then(function(x){return x.json()}).then(function(d){"
    "s.textContent=d.response;s.className='v '+(d.ok?'ok':'');"
    "b.disabled=false;b.textContent='Tester carte SIM';"
    "}).catch(function(){"
    "s.textContent='Erreur r\\u00e9seau';s.className='v';"
    "b.disabled=false;b.textContent='Tester carte SIM';"
    "});}"
    "function toggleLed(){"
    "var b=document.getElementById('lb'),s=document.getElementById('ls');"
    "b.disabled=true;b.textContent='Envoi...';"
    "fetch('/led-test').then(function(x){return x.json()}).then(function(d){"
    "s.textContent=d.response;s.className='v '+(d.ok?'ok':'');"
    "b.disabled=false;b.textContent='Activer LED status';"
    "}).catch(function(){"
    "s.textContent='Erreur r\\u00e9seau';s.className='v';"
    "b.disabled=false;b.textContent='Activer LED status';"
    "});}"
    "</script>"

    "</div></body></html>";

static const char REBOOT_PAGE_BODY[] =
    "<!DOCTYPE html><html lang=\"fr\"><head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>MobileAir</title>"
    "<style>"
    "body{font-family:-apple-system,system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;"
    "display:flex;align-items:center;justify-content:center;min-height:100vh}"
    ".m{text-align:center;color:#00d4ff;font-size:1.2em}"
    "</style></head><body>"
    "<div class=\"m\"><p style=\"font-size:2em\">&#x1F504;</p>"
    "<p>Red&eacute;marrage en cours...</p>"
    "</div></body></html>";

// =====================================================================
// HTTP response helpers
// =====================================================================

// Large buffer for composing pages
static char s_page_buf[8192];
static int s_page_buf_len = 0;

// Build HTTP response with headers included
static int compose_http_response(char* buf, int buf_size, const char* body, int body_len) {
    int hdr_len = snprintf(buf, buf_size,
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n", body_len);
    if (hdr_len + body_len < buf_size) {
        memcpy(buf + hdr_len, body, body_len);
        return hdr_len + body_len;
    }
    return hdr_len;
}

// Temporary buffer for HTML body before adding headers
static char s_body_tmp[7000];

static void compose_index_page() {
    char* p = s_body_tmp;
    int remain = sizeof(s_body_tmp) - 1;

    auto append = [&](const char* s, int len) {
        if (len > remain) len = remain;
        memcpy(p, s, len);
        p += len;
        remain -= len;
    };

    append(PAGE_HEADER, sizeof(PAGE_HEADER) - 1);
    append(PAGE_FORM_PRE, sizeof(PAGE_FORM_PRE) - 1);

    if (s_networks_html_len > 0) {
        append(s_networks_html, s_networks_html_len);
    } else {
        const char* scanning = "<div class=\"scanning\">Recherche des r&eacute;seaux...</div>";
        append(scanning, strlen(scanning));
    }

    append(PAGE_FORM_POST, sizeof(PAGE_FORM_POST) - 1);
    append(PAGE_FOOTER, sizeof(PAGE_FOOTER) - 1);

    *p = '\0';
    int body_len = (int)(p - s_body_tmp);

    s_page_buf_len = compose_http_response(s_page_buf, sizeof(s_page_buf),
                                            s_body_tmp, body_len);
}

static char s_result_buf[2048];
static int s_result_buf_len = 0;

static void compose_result_page() {
    char* p = s_body_tmp;
    int remain = sizeof(s_body_tmp) - 1;

    auto append = [&](const char* s, int len) {
        if (len > remain) len = remain;
        memcpy(p, s, len);
        p += len;
        remain -= len;
    };

    append(PAGE_HEADER, sizeof(PAGE_HEADER) - 1);
    if (s_connect_success) {
        append(PAGE_RESULT_OK, sizeof(PAGE_RESULT_OK) - 1);
    } else {
        append(PAGE_RESULT_FAIL, sizeof(PAGE_RESULT_FAIL) - 1);
    }
    append(s_connect_msg, strlen(s_connect_msg));
    append(PAGE_RESULT_CLOSE, sizeof(PAGE_RESULT_CLOSE) - 1);
    append(PAGE_FOOTER, sizeof(PAGE_FOOTER) - 1);

    *p = '\0';
    int body_len = (int)(p - s_body_tmp);

    s_result_buf_len = compose_http_response(s_result_buf, sizeof(s_result_buf),
                                              s_body_tmp, body_len);
}

static void compose_status_page() {
    // Board ID
    char board_id[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(board_id, sizeof(board_id));

    // Heap
    extern char __StackLimit, __bss_end__;
    uint32_t total_heap = &__StackLimit - &__bss_end__;
    struct mallinfo mi = mallinfo();
    uint32_t heap_free = total_heap - (uint32_t)mi.uordblks;

    // Uptime
    uint64_t uptime_ms = to_ms_since_boot(get_absolute_time());
    uint32_t up_sec = (uint32_t)(uptime_ms / 1000);
    char uptime_str[32];
    if (up_sec >= 3600) {
        snprintf(uptime_str, sizeof(uptime_str), "%luh %lum %lus",
                 (unsigned long)(up_sec / 3600),
                 (unsigned long)((up_sec % 3600) / 60),
                 (unsigned long)(up_sec % 60));
    } else if (up_sec >= 60) {
        snprintf(uptime_str, sizeof(uptime_str), "%lum %lus",
                 (unsigned long)(up_sec / 60),
                 (unsigned long)(up_sec % 60));
    } else {
        snprintf(uptime_str, sizeof(uptime_str), "%lus", (unsigned long)up_sec);
    }

    // WiFi RSSI
    int rssi = wifi_manager::get_sta_rssi();
    const char* signal;
    if (rssi > -50) signal = "&#9679;&#9679;&#9679;&#9679;";
    else if (rssi > -60) signal = "&#9679;&#9679;&#9679;&#9675;";
    else if (rssi > -70) signal = "&#9679;&#9679;&#9675;&#9675;";
    else signal = "&#9679;&#9675;&#9675;&#9675;";

    // IP
    const ip4_addr_t* ip = netif_ip4_addr(netif_default);
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), "%s", ip4addr_ntoa(ip));

    // CPU
#ifdef PICO_RP2350
    const char* cpu = "RP2350 (Cortex-M33)";
#else
    const char* cpu = "RP2040 (Cortex-M0+)";
#endif

    int body_len = snprintf(s_body_tmp, sizeof(s_body_tmp), STATUS_PAGE_FMT,
        s_status_ssid,
        signal, rssi,
        ip_str,
        PICO_BOARD,
        board_id,
        FW_VERSION,
        cpu,
        clock_get_hz(clk_sys) / 1000000,
        PICO_FLASH_SIZE_BYTES / 1024,
        (unsigned long)(heap_free / 1024),
        (unsigned long)(total_heap / 1024),
        uptime_str);

    if (body_len < 0) body_len = 0;
    if (body_len >= (int)sizeof(s_body_tmp)) body_len = sizeof(s_body_tmp) - 1;

    s_page_buf_len = compose_http_response(s_page_buf, sizeof(s_page_buf),
                                            s_body_tmp, body_len);
}

static void compose_reboot_page() {
    int body_len = sizeof(REBOOT_PAGE_BODY) - 1;
    s_result_buf_len = compose_http_response(s_result_buf, sizeof(s_result_buf),
                                              REBOOT_PAGE_BODY, body_len);
}

// --- Modem test JSON response ---
static char s_modem_json_buf[512];
static int s_modem_json_len = 0;

static void compose_modem_test_response() {
    char at_resp[256] = {};
    bool ok = modem::send_at("ATI", at_resp, sizeof(at_resp), 3000);

    // Extract model name from ATI response (skip echo line)
    char model[128] = {};
    if (ok && strstr(at_resp, "OK")) {
        const char* p = at_resp;
        const char* nl = strchr(p, '\n');
        if (nl) p = nl + 1;
        int i = 0;
        while (p[i] && p[i] != '\r' && p[i] != '\n' && i < (int)sizeof(model) - 1) {
            model[i] = p[i];
            i++;
        }
        model[i] = '\0';
    }

    char json[256];
    int json_len;
    if (model[0]) {
        json_len = snprintf(json, sizeof(json),
            "{\"ok\":true,\"response\":\"%s\"}", model);
    } else {
        json_len = snprintf(json, sizeof(json),
            "{\"ok\":false,\"response\":\"Pas de r\\u00e9ponse du modem\"}");
    }

    s_modem_json_len = snprintf(s_modem_json_buf, sizeof(s_modem_json_buf),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n%s", json_len, json);
}

static void compose_sim_test_response() {
    char at_resp[256] = {};
    bool ok = modem::send_at("AT+CIMI", at_resp, sizeof(at_resp), 3000);

    // Extract IMSI from response (skip echo line)
    char imsi[64] = {};
    if (ok && strstr(at_resp, "OK")) {
        const char* p = at_resp;
        const char* nl = strchr(p, '\n');
        if (nl) p = nl + 1;
        int i = 0;
        while (p[i] && p[i] != '\r' && p[i] != '\n' && i < (int)sizeof(imsi) - 1) {
            imsi[i] = p[i];
            i++;
        }
        imsi[i] = '\0';
    }

    char json[256];
    int json_len;
    if (imsi[0]) {
        json_len = snprintf(json, sizeof(json),
            "{\"ok\":true,\"response\":\"%s\"}", imsi);
    } else if (ok && strstr(at_resp, "ERROR")) {
        json_len = snprintf(json, sizeof(json),
            "{\"ok\":false,\"response\":\"Carte SIM absente ou verrouill\\u00e9e\"}");
    } else {
        json_len = snprintf(json, sizeof(json),
            "{\"ok\":false,\"response\":\"Pas de r\\u00e9ponse du modem\"}");
    }

    s_modem_json_len = snprintf(s_modem_json_buf, sizeof(s_modem_json_buf),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n%s", json_len, json);
}

static void compose_led_test_response() {
    char at_resp[256] = {};
    bool ok = modem::send_at("AT+UGPIOC=16,2", at_resp, sizeof(at_resp), 3000);

    char json[256];
    int json_len;
    if (ok && strstr(at_resp, "OK")) {
        json_len = snprintf(json, sizeof(json),
            "{\"ok\":true,\"response\":\"LED activ\\u00e9e\"}");
    } else if (ok && strstr(at_resp, "ERROR")) {
        json_len = snprintf(json, sizeof(json),
            "{\"ok\":false,\"response\":\"Erreur commande\"}");
    } else {
        json_len = snprintf(json, sizeof(json),
            "{\"ok\":false,\"response\":\"Pas de r\\u00e9ponse du modem\"}");
    }

    s_modem_json_len = snprintf(s_modem_json_buf, sizeof(s_modem_json_buf),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n%s", json_len, json);
}

// Redirect page for captive portal detection
static const char REDIRECT_BODY[] =
    "HTTP/1.1 302 Found\r\n"
    "Location: http://192.168.4.1/\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n\r\n";

// --- lwIP httpd custom file callbacks ---

static bool is_captive_detect_url(const char* uri) {
    if (strcmp(uri, "/generate_204") == 0) return true;
    if (strcmp(uri, "/gen_204") == 0) return true;
    if (strcmp(uri, "/hotspot-detect.html") == 0) return true;
    if (strcmp(uri, "/library/test/success.html") == 0) return true;
    if (strcmp(uri, "/connecttest.txt") == 0) return true;
    if (strcmp(uri, "/ncsi.txt") == 0) return true;
    if (strcmp(uri, "/redirect") == 0) return true;
    if (strcmp(uri, "/success.txt") == 0) return true;
    if (strcmp(uri, "/canonical.html") == 0) return true;
    return false;
}

extern "C" {

// Track current serve buffer for fs_read_custom (httpd is single-threaded)
static const char* s_serve_ptr = nullptr;

// Helper: set up file for dynamic reading via fs_read_custom
// file->data = NULL prevents httpd from sending data directly (avoids doubling)
static void file_serve(struct fs_file* file, const char* buf, int len) {
    s_serve_ptr = buf;
    file->data = NULL;
    file->len = len;
    file->index = 0;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
}

int fs_open_custom(struct fs_file* file, const char* name) {
    memset(file, 0, sizeof(struct fs_file));

    // --- Status mode (STA connected) ---
    if (s_mode == MODE_STATUS) {
        if (strcmp(name, "/rebooting") == 0) {
            compose_reboot_page();
            file_serve(file, s_result_buf, s_result_buf_len);
            return 1;
        }

        if (strcmp(name, "/modem-test") == 0) {
            compose_modem_test_response();
            file_serve(file, s_modem_json_buf, s_modem_json_len);
            return 1;
        }

        if (strcmp(name, "/sim-test") == 0) {
            compose_sim_test_response();
            file_serve(file, s_modem_json_buf, s_modem_json_len);
            return 1;
        }

        if (strcmp(name, "/led-test") == 0) {
            compose_led_test_response();
            file_serve(file, s_modem_json_buf, s_modem_json_len);
            return 1;
        }

        // Serve status/dashboard page for any URL
        compose_status_page();
        file_serve(file, s_page_buf, s_page_buf_len);
        return 1;
    }

    // --- Portal mode (AP pairing) ---

    if (strcmp(name, "/result") == 0 && s_has_connect_result) {
        compose_result_page();
        file_serve(file, s_result_buf, s_result_buf_len);

        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        return 1;
    }

    if (is_captive_detect_url(name)) {
        file_serve(file, REDIRECT_BODY, sizeof(REDIRECT_BODY) - 1);
        return 1;
    }

    // Default: serve index page
    compose_index_page();
    file_serve(file, s_page_buf, s_page_buf_len);
    return 1;
}

void fs_close_custom(struct fs_file* file) {
    (void)file;
}

int fs_read_custom(struct fs_file* file, char* buffer, int count) {
    int remaining = file->len - file->index;
    if (remaining <= 0) return FS_READ_EOF;

    const char* data = s_serve_ptr;
    int to_read = (count < remaining) ? count : remaining;
    memcpy(buffer, data + file->index, to_read);
    file->index += to_read;
    return to_read;
}

// --- POST handling ---

static char s_post_buf[256];
static int s_post_buf_len = 0;
static int s_post_content_len = 0;
static char s_post_uri[16] = {};

err_t httpd_post_begin(void* connection, const char* uri,
                       const char* http_request, u16_t http_request_len,
                       int content_len, char* response_uri,
                       u16_t response_uri_len, u8_t* post_auto_wnd) {
    if (strcmp(uri, "/connect") == 0 || strcmp(uri, "/reboot") == 0) {
        strncpy(s_post_uri, uri, sizeof(s_post_uri) - 1);
        s_post_buf_len = 0;
        s_post_content_len = content_len;
        *post_auto_wnd = 1;
        return ERR_OK;
    }
    return ERR_VAL;
}

err_t httpd_post_receive_data(void* connection, struct pbuf* p) {
    if (!p) return ERR_OK;

    int copy_len = p->tot_len;
    if (s_post_buf_len + copy_len > (int)sizeof(s_post_buf) - 1) {
        copy_len = sizeof(s_post_buf) - 1 - s_post_buf_len;
    }

    pbuf_copy_partial(p, s_post_buf + s_post_buf_len, copy_len, 0);
    s_post_buf_len += copy_len;
    s_post_buf[s_post_buf_len] = '\0';

    pbuf_free(p);
    return ERR_OK;
}

// URL-decode in place
static void url_decode(char* str) {
    char* dst = str;
    char* src = str;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            *dst++ = (char)strtol(hex, nullptr, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// Extract field value from URL-encoded form data
static bool extract_field(const char* data, const char* field, char* out, int out_len) {
    int field_len = strlen(field);
    const char* p = data;
    while ((p = strstr(p, field)) != nullptr) {
        if (p != data && *(p - 1) != '&') {
            p += field_len;
            continue;
        }
        if (p[field_len] != '=') {
            p += field_len;
            continue;
        }
        p += field_len + 1;
        const char* end = strchr(p, '&');
        int len = end ? (int)(end - p) : (int)strlen(p);
        if (len >= out_len) len = out_len - 1;
        memcpy(out, p, len);
        out[len] = '\0';
        url_decode(out);
        return true;
    }
    return false;
}

void httpd_post_finished(void* connection, char* response_uri, u16_t response_uri_len) {
    if (strcmp(s_post_uri, "/reboot") == 0) {
        printf("[portal] Reboot requested via web UI\n");
        s_reboot_requested = true;
        snprintf(response_uri, response_uri_len, "/rebooting");
        return;
    }

    // /connect — parse form data: ssid=...&password=...
    memset(s_submitted_ssid, 0, sizeof(s_submitted_ssid));
    memset(s_submitted_pass, 0, sizeof(s_submitted_pass));

    extract_field(s_post_buf, "ssid", s_submitted_ssid, sizeof(s_submitted_ssid));
    extract_field(s_post_buf, "password", s_submitted_pass, sizeof(s_submitted_pass));

    printf("[portal] Credentials received: SSID='%s'\n", s_submitted_ssid);

    s_creds_ready = true;
    snprintf(response_uri, response_uri_len, "/result");
}

} // extern "C"

// =====================================================================
// Public API
// =====================================================================

static void ensure_httpd() {
    if (!s_httpd_initialized) {
        httpd_init();
        s_httpd_initialized = true;
        printf("[portal] HTTP server started on port 80\n");
    }
}

namespace captive_portal {

bool start() {
    if (s_running) return true;

    // Start DHCP server
    ip_addr_t ip, nm;
    IP4_ADDR(ip_2_ip4(&ip), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&nm), 255, 255, 255, 0);
    dhcp_server_init(&s_dhcp_server, &ip, &nm);

    // Start DNS server
    if (!dns_server::start(AP_IP)) {
        printf("[portal] DNS start failed\n");
        dhcp_server_deinit(&s_dhcp_server);
        return false;
    }

    ensure_httpd();

    s_mode = MODE_PORTAL;
    s_running = true;
    s_creds_ready = false;
    s_has_connect_result = false;

    printf("[portal] Captive portal running\n");
    return true;
}

bool start_status(const char* ssid) {
    strncpy(s_status_ssid, ssid, sizeof(s_status_ssid) - 1);
    s_status_ssid[sizeof(s_status_ssid) - 1] = '\0';

    ensure_httpd();

    s_mode = MODE_STATUS;
    s_reboot_requested = false;

    printf("[portal] Status server running (SSID=%s)\n", s_status_ssid);
    return true;
}

void stop() {
    if (!s_running) return;
    dns_server::stop();
    dhcp_server_deinit(&s_dhcp_server);
    // httpd has no deinit — stays alive
    s_running = false;
    printf("[portal] Stopped\n");
}

void set_networks_html(const char* html, int len) {
    if (len > (int)sizeof(s_networks_html) - 1) {
        len = sizeof(s_networks_html) - 1;
    }
    memcpy(s_networks_html, html, len);
    s_networks_html[len] = '\0';
    s_networks_html_len = len;
}

bool has_new_credentials() {
    return s_creds_ready;
}

void get_credentials(char* ssid, int ssid_len, char* password, int pass_len) {
    strncpy(ssid, s_submitted_ssid, ssid_len - 1);
    ssid[ssid_len - 1] = '\0';
    strncpy(password, s_submitted_pass, pass_len - 1);
    password[pass_len - 1] = '\0';
    s_creds_ready = false;
}

void set_connect_result(bool success, const char* message) {
    s_connect_success = success;
    strncpy(s_connect_msg, message, sizeof(s_connect_msg) - 1);
    s_connect_msg[sizeof(s_connect_msg) - 1] = '\0';
    s_has_connect_result = true;
}

bool should_reboot() {
    return s_reboot_requested;
}

} // namespace captive_portal
