#pragma once

#include <cstdint>

namespace sdcard {

enum class CardType : uint8_t {
    NONE = 0,
    SD_V1,      // SD version 1.x (SDSC, byte addressing)
    SD_V2,      // SD version 2+ (SDSC, byte addressing)
    SDHC        // SDHC/SDXC (block addressing)
};

struct Info {
    CardType type;
    uint32_t sectors;       // total 512-byte sectors
    uint32_t capacity_mb;   // capacity in MB
    bool detected;
};

/// Initialize SPI0 on GP16 (MISO), GP17 (CS), GP18 (SCK), GP19 (MOSI)
/// and attempt to detect + initialize the SD card.
bool init();

/// Return cached card information from the last init()/test().
const Info& get_info();

/// Re-detect the card (useful after hot-plug).
bool test();

/// Read a single 512-byte block.
bool read_block(uint32_t block, uint8_t* buf);

/// Write a single 512-byte block.
bool write_block(uint32_t block, const uint8_t* buf);

}  // namespace sdcard
