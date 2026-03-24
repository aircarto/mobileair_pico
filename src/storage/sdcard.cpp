#include "storage/sdcard.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>

// SPI0 pin assignments (from pinout.md)
#define SD_SPI        spi0
#define SD_MISO       16
#define SD_CS         17
#define SD_SCK        18
#define SD_MOSI       19
#define SD_SPI_INIT_FREQ   400000    // 400 kHz for init
#define SD_SPI_FAST_FREQ   10000000  // 10 MHz for normal operation

// SD card commands
#define CMD0    0   // GO_IDLE_STATE
#define CMD8    8   // SEND_IF_COND
#define CMD9    9   // SEND_CSD
#define CMD17  17   // READ_SINGLE_BLOCK
#define CMD24  24   // WRITE_BLOCK
#define CMD55  55   // APP_CMD
#define CMD58  58   // READ_OCR
#define ACMD41 41   // SD_SEND_OP_COND

static sdcard::Info s_info = {};
static bool s_initialized = false;

// --- Low-level SPI helpers ---

static void cs_select() {
    gpio_put(SD_CS, 0);
}

static void cs_deselect() {
    gpio_put(SD_CS, 1);
    // Send one extra byte to release DO
    uint8_t dummy = 0xFF;
    spi_write_blocking(SD_SPI, &dummy, 1);
}

static uint8_t spi_transfer(uint8_t tx) {
    uint8_t rx;
    spi_write_read_blocking(SD_SPI, &tx, &rx, 1);
    return rx;
}

/// Wait for card to not be busy (DO goes high).
static bool wait_ready(uint32_t timeout_ms) {
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (true) {
        if (spi_transfer(0xFF) == 0xFF) return true;
        if (to_ms_since_boot(get_absolute_time()) - start > timeout_ms) return false;
    }
}

/// Send a 6-byte SD command and return the R1 response byte.
static uint8_t sd_command(uint8_t cmd, uint32_t arg) {
    // Wait for card ready
    wait_ready(500);

    uint8_t frame[6];
    frame[0] = 0x40 | cmd;
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)(arg);

    // CRC (only needed for CMD0 and CMD8 during SPI init)
    if (cmd == CMD0) frame[5] = 0x95;
    else if (cmd == CMD8) frame[5] = 0x87;
    else frame[5] = 0x01;  // dummy CRC + stop bit

    spi_write_blocking(SD_SPI, frame, 6);

    // Wait for response (up to 8 bytes)
    uint8_t r1;
    for (int i = 0; i < 8; i++) {
        r1 = spi_transfer(0xFF);
        if (!(r1 & 0x80)) return r1;
    }
    return 0xFF;  // no response
}

/// Send ACMD (CMD55 + command).
static uint8_t sd_acmd(uint8_t cmd, uint32_t arg) {
    sd_command(CMD55, 0);
    return sd_command(cmd, arg);
}

/// Read the CSD register to determine card capacity.
static bool read_csd(uint32_t* out_sectors) {
    cs_select();
    uint8_t r1 = sd_command(CMD9, 0);
    if (r1 != 0x00) {
        cs_deselect();
        return false;
    }

    // Wait for data token 0xFE
    uint32_t start = to_ms_since_boot(get_absolute_time());
    uint8_t token;
    do {
        token = spi_transfer(0xFF);
        if (to_ms_since_boot(get_absolute_time()) - start > 500) {
            cs_deselect();
            return false;
        }
    } while (token == 0xFF);

    if (token != 0xFE) {
        cs_deselect();
        return false;
    }

    // Read 16 bytes CSD
    uint8_t csd[16];
    for (int i = 0; i < 16; i++) {
        csd[i] = spi_transfer(0xFF);
    }
    // Skip 2 CRC bytes
    spi_transfer(0xFF);
    spi_transfer(0xFF);

    cs_deselect();

    // Parse CSD
    uint8_t csd_ver = (csd[0] >> 6) & 0x03;
    if (csd_ver == 1) {
        // CSD v2 (SDHC/SDXC): C_SIZE is 22 bits at [69:48]
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16)
                        | ((uint32_t)csd[8] << 8)
                        | csd[9];
        *out_sectors = (c_size + 1) * 1024;  // each unit = 512 KB
    } else if (csd_ver == 0) {
        // CSD v1 (SDSC)
        uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10)
                        | ((uint32_t)csd[7] << 2)
                        | ((csd[8] >> 6) & 0x03);
        uint8_t c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01);
        uint8_t read_bl_len = csd[5] & 0x0F;
        uint32_t block_len = 1u << read_bl_len;
        uint32_t mult = 1u << (c_size_mult + 2);
        uint64_t capacity = (uint64_t)(c_size + 1) * mult * block_len;
        *out_sectors = (uint32_t)(capacity / 512);
    } else {
        return false;
    }

    return true;
}

/// Initialize the SD card via SPI protocol.
static bool sd_init_card() {
    s_info = {};

    // Start at slow clock for initialization
    spi_set_baudrate(SD_SPI, SD_SPI_INIT_FREQ);

    // Send 80+ clock cycles with CS high (card enters SPI mode)
    cs_deselect();
    for (int i = 0; i < 10; i++) {
        spi_transfer(0xFF);
    }

    // CMD0: GO_IDLE_STATE (software reset)
    cs_select();
    uint8_t r1 = sd_command(CMD0, 0);
    cs_deselect();
    if (r1 != 0x01) {
        printf("[sdcard] CMD0 failed (r1=0x%02X)\n", r1);
        return false;
    }

    // CMD8: SEND_IF_COND (check voltage range, 0x1AA pattern)
    cs_select();
    r1 = sd_command(CMD8, 0x000001AA);
    bool v2_card = false;
    if (r1 == 0x01) {
        // SDv2+: read 4 response bytes
        uint8_t resp[4];
        for (int i = 0; i < 4; i++) resp[i] = spi_transfer(0xFF);
        cs_deselect();

        if ((resp[2] & 0x0F) != 0x01 || resp[3] != 0xAA) {
            printf("[sdcard] CMD8 voltage mismatch\n");
            return false;
        }
        v2_card = true;
    } else {
        cs_deselect();
    }

    // ACMD41: SD_SEND_OP_COND (initialize, with HCS bit for v2 cards)
    uint32_t arg = v2_card ? 0x40000000 : 0;
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (true) {
        cs_select();
        r1 = sd_acmd(ACMD41, arg);
        cs_deselect();
        if (r1 == 0x00) break;  // ready
        if (to_ms_since_boot(get_absolute_time()) - start > 2000) {
            printf("[sdcard] ACMD41 timeout (r1=0x%02X)\n", r1);
            return false;
        }
        sleep_ms(10);
    }

    // Determine card type
    if (v2_card) {
        // CMD58: READ_OCR to check CCS bit
        cs_select();
        r1 = sd_command(CMD58, 0);
        if (r1 == 0x00) {
            uint8_t ocr[4];
            for (int i = 0; i < 4; i++) ocr[i] = spi_transfer(0xFF);
            s_info.type = (ocr[0] & 0x40)
                        ? sdcard::CardType::SDHC
                        : sdcard::CardType::SD_V2;
        } else {
            s_info.type = sdcard::CardType::SD_V2;
        }
        cs_deselect();
    } else {
        s_info.type = sdcard::CardType::SD_V1;
    }

    // Read capacity from CSD register
    uint32_t sectors = 0;
    if (read_csd(&sectors)) {
        s_info.sectors = sectors;
        s_info.capacity_mb = (uint32_t)(sectors / 2048);  // 512 * 2048 = 1 MB
    }

    // Switch to fast SPI clock
    spi_set_baudrate(SD_SPI, SD_SPI_FAST_FREQ);

    s_info.detected = true;
    return true;
}

namespace sdcard {

bool init() {
    printf("[sdcard] Initializing SPI0 (MISO=GP%d, CS=GP%d, SCK=GP%d, MOSI=GP%d)...\n",
           SD_MISO, SD_CS, SD_SCK, SD_MOSI);

    // Initialize SPI peripheral
    spi_init(SD_SPI, SD_SPI_INIT_FREQ);
    spi_set_format(SD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Configure GPIO pins
    gpio_set_function(SD_MISO, GPIO_FUNC_SPI);
    gpio_set_function(SD_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);

    // CS is manual GPIO (active low)
    gpio_init(SD_CS);
    gpio_set_dir(SD_CS, GPIO_OUT);
    gpio_put(SD_CS, 1);  // deselected

    s_initialized = true;

    if (sd_init_card()) {
        const char* type_str = "?";
        switch (s_info.type) {
            case CardType::SD_V1: type_str = "SDv1"; break;
            case CardType::SD_V2: type_str = "SDv2"; break;
            case CardType::SDHC:  type_str = "SDHC"; break;
            default: break;
        }
        printf("[sdcard] Card detected: %s, %lu MB (%lu sectors)\n",
               type_str,
               (unsigned long)s_info.capacity_mb,
               (unsigned long)s_info.sectors);
        return true;
    } else {
        printf("[sdcard] No card detected\n");
        return false;
    }
}

const Info& get_info() {
    return s_info;
}

bool test() {
    if (!s_initialized) return false;
    return sd_init_card();
}

bool read_block(uint32_t block, uint8_t* buf) {
    if (!s_info.detected) return false;

    // SDSC uses byte address, SDHC uses block address
    uint32_t addr = (s_info.type == CardType::SDHC) ? block : block * 512;

    cs_select();
    uint8_t r1 = sd_command(CMD17, addr);
    if (r1 != 0x00) {
        cs_deselect();
        return false;
    }

    // Wait for data token
    uint32_t start = to_ms_since_boot(get_absolute_time());
    uint8_t token;
    do {
        token = spi_transfer(0xFF);
        if (to_ms_since_boot(get_absolute_time()) - start > 500) {
            cs_deselect();
            return false;
        }
    } while (token == 0xFF);

    if (token != 0xFE) {
        cs_deselect();
        return false;
    }

    // Read 512 bytes
    for (int i = 0; i < 512; i++) {
        buf[i] = spi_transfer(0xFF);
    }

    // Skip CRC (2 bytes)
    spi_transfer(0xFF);
    spi_transfer(0xFF);

    cs_deselect();
    return true;
}

bool write_block(uint32_t block, const uint8_t* buf) {
    if (!s_info.detected) return false;

    uint32_t addr = (s_info.type == CardType::SDHC) ? block : block * 512;

    cs_select();
    if (!wait_ready(500)) {
        cs_deselect();
        return false;
    }

    uint8_t r1 = sd_command(CMD24, addr);
    if (r1 != 0x00) {
        cs_deselect();
        return false;
    }

    // Send data token
    spi_transfer(0xFE);

    // Send 512 bytes
    spi_write_blocking(SD_SPI, buf, 512);

    // Send dummy CRC
    spi_transfer(0xFF);
    spi_transfer(0xFF);

    // Check data response
    uint8_t resp = spi_transfer(0xFF);
    if ((resp & 0x1F) != 0x05) {
        cs_deselect();
        return false;
    }

    // Wait for write to complete
    if (!wait_ready(1000)) {
        cs_deselect();
        return false;
    }

    cs_deselect();
    return true;
}

}  // namespace sdcard
