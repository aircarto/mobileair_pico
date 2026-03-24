#include "storage/datalog.h"
#include "storage/sdcard.h"
#include <cstdio>
#include <cstring>

// Layout:
//   Sector 0: header (magic, data_bytes, line_count, buf_offset)
//   Sectors 1+: CSV data written sequentially

#define HEADER_SECTOR   0
#define DATA_START      1      // first data sector
#define MAGIC           0x4D415344  // "MASD"

struct __attribute__((packed)) Header {
    uint32_t magic;
    uint32_t data_bytes;    // total CSV bytes written (committed to SD)
    uint32_t line_count;    // total lines written
    uint16_t buf_used;      // bytes pending in current sector buffer
};

static Header s_hdr = {};
static bool s_ready = false;

// Write buffer — one sector (512 bytes)
static uint8_t s_buf[512];

/// Current write sector = DATA_START + (data_bytes / 512)
static uint32_t current_sector() {
    return DATA_START + (s_hdr.data_bytes / 512);
}

static bool write_header() {
    uint8_t block[512];
    memset(block, 0, sizeof(block));
    memcpy(block, &s_hdr, sizeof(s_hdr));
    return sdcard::write_block(HEADER_SECTOR, block);
}

static bool read_header() {
    uint8_t block[512];
    if (!sdcard::read_block(HEADER_SECTOR, block)) return false;
    memcpy(&s_hdr, block, sizeof(s_hdr));
    return s_hdr.magic == MAGIC;
}

namespace datalog {

bool init() {
    const sdcard::Info& sd = sdcard::get_info();
    if (!sd.detected) {
        printf("[datalog] SD card not available\n");
        return false;
    }

    if (read_header()) {
        // Resume from existing log
        printf("[datalog] Existing log: %lu lines, %lu bytes\n",
               (unsigned long)s_hdr.line_count,
               (unsigned long)s_hdr.data_bytes);

        // If there's a partial sector, read it back into the buffer
        if (s_hdr.buf_used > 0) {
            if (sdcard::read_block(current_sector(), s_buf)) {
                printf("[datalog] Resumed partial sector (%u bytes)\n", s_hdr.buf_used);
            } else {
                s_hdr.buf_used = 0;
            }
        }
    } else {
        // Fresh card — write CSV header line
        printf("[datalog] Initializing new log\n");
        s_hdr.magic = MAGIC;
        s_hdr.data_bytes = 0;
        s_hdr.line_count = 0;
        s_hdr.buf_used = 0;
        memset(s_buf, 0, sizeof(s_buf));

        // Write the CSV column header as first line
        const char* csv_header = "datetime,pm1,pm25,pm10,temp_pm,hum_pm,status\n";
        int len = strlen(csv_header);
        memcpy(s_buf, csv_header, len);
        s_hdr.buf_used = len;

        write_header();
    }

    s_ready = true;
    return true;
}

bool append(const char* line) {
    if (!s_ready) return false;

    // Build line with newline
    int line_len = strlen(line);
    if (line_len == 0) return false;

    // Temporary buffer: line + '\n'
    char tmp[256];
    if (line_len + 1 > (int)sizeof(tmp)) return false;
    memcpy(tmp, line, line_len);
    tmp[line_len] = '\n';
    int total = line_len + 1;

    int written = 0;
    while (written < total) {
        int space = 512 - s_hdr.buf_used;
        int chunk = (total - written < space) ? (total - written) : space;

        memcpy(s_buf + s_hdr.buf_used, tmp + written, chunk);
        s_hdr.buf_used += chunk;
        written += chunk;

        // Sector full — write to SD
        if (s_hdr.buf_used >= 512) {
            if (!sdcard::write_block(current_sector(), s_buf)) {
                printf("[datalog] Write error at sector %lu\n",
                       (unsigned long)current_sector());
                return false;
            }
            s_hdr.data_bytes += 512;
            s_hdr.buf_used = 0;
            memset(s_buf, 0, sizeof(s_buf));
        }
    }

    s_hdr.line_count++;

    // Save header every 10 lines to limit flash wear
    if (s_hdr.line_count % 10 == 0) {
        flush();
    }

    return true;
}

void flush() {
    if (!s_ready) return;

    // Write partial sector if any data pending
    if (s_hdr.buf_used > 0) {
        sdcard::write_block(current_sector(), s_buf);
    }

    write_header();
}

uint32_t get_line_count() {
    return s_hdr.line_count;
}

uint32_t get_data_bytes() {
    return s_hdr.data_bytes + s_hdr.buf_used;
}

int read_data(char* buf, int buf_size, uint32_t from) {
    if (!s_ready) return 0;

    uint32_t total = get_data_bytes();
    if (from >= total) return 0;

    // Flush so all data is on SD
    flush();

    uint32_t remaining = total - from;
    int to_read = (remaining < (uint32_t)buf_size) ? (int)remaining : buf_size;

    int copied = 0;
    while (copied < to_read) {
        uint32_t abs_byte = from + copied;
        uint32_t sector = DATA_START + (abs_byte / 512);
        uint32_t offset = abs_byte % 512;

        uint8_t block[512];
        if (!sdcard::read_block(sector, block)) break;

        int avail = 512 - offset;
        int chunk = (to_read - copied < avail) ? (to_read - copied) : avail;
        memcpy(buf + copied, block + offset, chunk);
        copied += chunk;
    }

    return copied;
}

}  // namespace datalog
