#include "wifi_credentials.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <cstring>
#include <cstdio>

// Use the last sector of flash for credential storage
// Pico 2W has 4MB flash = 0x400000
static constexpr uint32_t FLASH_CRED_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

// On-flash layout (256 bytes, fits in one page)
struct FlashCredBlock {
    uint32_t magic;           // CRED_MAGIC
    char ssid[CRED_SSID_MAX]; // 33 bytes, null-terminated
    char password[CRED_PASS_MAX]; // 65 bytes, null-terminated
    uint8_t reserved[149];    // padding to 252
    uint32_t crc32;           // CRC32 of bytes 0..251
};

static_assert(sizeof(FlashCredBlock) == 256, "FlashCredBlock must be 256 bytes");

// Simple CRC32
static uint32_t calc_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

namespace wifi_credentials {

bool load(WifiCredentials& creds) {
    const FlashCredBlock* block =
        (const FlashCredBlock*)(XIP_BASE + FLASH_CRED_OFFSET);

    // Check magic
    if (block->magic != CRED_MAGIC) {
        printf("[cred] No credentials in flash (magic=0x%08lX)\n", block->magic);
        return false;
    }

    // Check CRC
    uint32_t crc = calc_crc32((const uint8_t*)block, offsetof(FlashCredBlock, crc32));
    if (crc != block->crc32) {
        printf("[cred] CRC mismatch (expected=0x%08lX got=0x%08lX)\n", block->crc32, crc);
        return false;
    }

    // Validate SSID is not empty
    if (block->ssid[0] == '\0') {
        printf("[cred] Empty SSID in flash\n");
        return false;
    }

    strncpy(creds.ssid, block->ssid, CRED_SSID_MAX - 1);
    creds.ssid[CRED_SSID_MAX - 1] = '\0';
    strncpy(creds.password, block->password, CRED_PASS_MAX - 1);
    creds.password[CRED_PASS_MAX - 1] = '\0';

    printf("[cred] Loaded: SSID='%s'\n", creds.ssid);
    return true;
}

bool save(const WifiCredentials& creds) {
    FlashCredBlock block = {};
    block.magic = CRED_MAGIC;
    strncpy(block.ssid, creds.ssid, CRED_SSID_MAX - 1);
    strncpy(block.password, creds.password, CRED_PASS_MAX - 1);
    block.crc32 = calc_crc32((const uint8_t*)&block, offsetof(FlashCredBlock, crc32));

    printf("[cred] Saving credentials for '%s'...\n", creds.ssid);

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_CRED_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_CRED_OFFSET, (const uint8_t*)&block, sizeof(block));
    restore_interrupts(ints);

    printf("[cred] Saved\n");
    return true;
}

bool erase() {
    printf("[cred] Erasing credentials...\n");

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_CRED_OFFSET, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    printf("[cred] Erased\n");
    return true;
}

} // namespace wifi_credentials
