#include "logger.h"
#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include <cstring>

#define LOG_LINES    100
#define LOG_LINE_LEN 160

// Ring buffer of log lines
static char s_lines[LOG_LINES][LOG_LINE_LEN];
static int  s_head = 0;     // next write slot
static int  s_count = 0;    // total lines stored (max LOG_LINES)

// Current line accumulator
static char s_cur_line[LOG_LINE_LEN];
static int  s_cur_pos = 0;

static void flush_line() {
    if (s_cur_pos == 0) return;

    // Strip trailing \r\n
    while (s_cur_pos > 0 &&
           (s_cur_line[s_cur_pos - 1] == '\n' ||
            s_cur_line[s_cur_pos - 1] == '\r')) {
        s_cur_pos--;
    }
    if (s_cur_pos >= 0 && s_cur_pos < LOG_LINE_LEN)
        s_cur_line[s_cur_pos] = '\0';

    // Skip ANSI escape sequences for web display
    char* dst = s_lines[s_head];
    int dst_pos = 0;
    for (int i = 0; i < s_cur_pos && dst_pos < LOG_LINE_LEN - 1; i++) {
        if (s_cur_line[i] == '\033') {
            // Skip until end of escape sequence (letter or end of string)
            while (i < s_cur_pos && !((s_cur_line[i] >= 'A' && s_cur_line[i] <= 'Z') ||
                                       (s_cur_line[i] >= 'a' && s_cur_line[i] <= 'z')))
                i++;
            continue;
        }
        dst[dst_pos++] = s_cur_line[i];
    }
    dst[dst_pos] = '\0';

    s_head = (s_head + 1) % LOG_LINES;
    if (s_count < LOG_LINES) s_count++;
    s_cur_pos = 0;
}

static void log_out_chars(const char* buf, int len) {
    for (int i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '\n') {
            flush_line();
        } else if (c != '\r') {
            if (s_cur_pos < LOG_LINE_LEN - 1) {
                s_cur_line[s_cur_pos++] = c;
            }
        }
    }
}

static stdio_driver_t s_log_driver = {
    .out_chars = log_out_chars,
    .out_flush = nullptr,
    .in_chars = nullptr,
    .set_chars_available_callback = nullptr,
    .next = nullptr,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .last_ended_with_cr = false,
    .crlf_enabled = false,
#endif
};

namespace logger {

void init() {
    memset(s_lines, 0, sizeof(s_lines));
    s_head = 0;
    s_count = 0;
    s_cur_pos = 0;
    stdio_set_driver_enabled(&s_log_driver, true);
}

int get_lines(char* buf, int buf_size) {
    int pos = 0;
    int remain = buf_size - 1;

    // Start from the oldest line
    int start = (s_count < LOG_LINES) ? 0 : s_head;

    for (int i = 0; i < s_count && remain > 0; i++) {
        int idx = (start + i) % LOG_LINES;
        int len = strlen(s_lines[idx]);
        if (len + 1 > remain) break;  // +1 for newline
        memcpy(buf + pos, s_lines[idx], len);
        pos += len;
        buf[pos++] = '\n';
        remain -= (len + 1);
    }

    buf[pos] = '\0';
    return pos;
}

}  // namespace logger
