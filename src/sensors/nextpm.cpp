#include "sensors/nextpm.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>

// UART0 on GP0 (TX) / GP1 (RX)
#define NEXTPM_UART     uart0
#define NEXTPM_TX       0
#define NEXTPM_RX       1
#define NEXTPM_BAUD     115200
#define NEXTPM_ADDR     1   // Modbus slave address

// NextPM Modbus holding registers
#define REG_STATUS      0x13
#define REG_PM1_1MIN    0x44
#define REG_PM25_1MIN   0x46
#define REG_PM10_1MIN   0x48
#define REG_HUM_INT     0x6A
#define REG_TEMP_INT    0x6B

// Modbus function codes
#define FUNC_READ_HOLDING 0x03

// --- Modbus CRC16 ---

static uint16_t modbus_crc16(const uint8_t* buf, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

// --- Low-level Modbus RTU ---

static void uart_flush_rx() {
    while (uart_is_readable(NEXTPM_UART))
        uart_getc(NEXTPM_UART);
}

/// Send a Modbus RTU request and read the response.
/// Returns number of bytes received, or 0 on timeout/error.
static int modbus_transaction(const uint8_t* req, size_t req_len,
                              uint8_t* resp, size_t resp_max,
                              uint32_t timeout_ms = 500) {
    uart_flush_rx();

    // Send request
    uart_write_blocking(NEXTPM_UART, req, req_len);

    // Modbus RTU: wait 3.5 char times (~0.3ms at 115200) before response
    sleep_ms(5);

    // Read response
    size_t pos = 0;
    uint32_t start = to_ms_since_boot(get_absolute_time());
    uint32_t last_rx = start;

    while ((to_ms_since_boot(get_absolute_time()) - start) < timeout_ms) {
        if (uart_is_readable(NEXTPM_UART)) {
            if (pos < resp_max)
                resp[pos++] = uart_getc(NEXTPM_UART);
            last_rx = to_ms_since_boot(get_absolute_time());
        } else {
            // Modbus RTU: frame ends after 3.5 char silence (~0.3ms at 115200)
            // Use 5ms as a safe inter-frame gap
            if (pos > 0 && (to_ms_since_boot(get_absolute_time()) - last_rx) > 5)
                break;
            sleep_us(100);
        }
    }

    return (int)pos;
}

/// Read holding registers (function 0x03).
/// Returns true if response is valid, fills `regs` with register values.
static bool read_holding_registers(uint16_t start_reg, uint16_t count,
                                   uint16_t* regs) {
    // Build request: [Addr][0x03][RegHi][RegLo][CountHi][CountLo][CRC16]
    uint8_t req[8];
    req[0] = NEXTPM_ADDR;
    req[1] = FUNC_READ_HOLDING;
    req[2] = (start_reg >> 8) & 0xFF;
    req[3] = start_reg & 0xFF;
    req[4] = (count >> 8) & 0xFF;
    req[5] = count & 0xFF;
    uint16_t crc = modbus_crc16(req, 6);
    req[6] = crc & 0xFF;        // CRC low
    req[7] = (crc >> 8) & 0xFF; // CRC high

    // Expected response: [Addr][0x03][ByteCount][Data...][CRC16]
    uint8_t resp[64];
    int expected = 3 + count * 2 + 2;  // header + data + CRC
    int n = modbus_transaction(req, 8, resp, sizeof(resp));

    if (n < expected) {
        printf("[nextpm] Short response: got %d, expected %d (reg 0x%02X)\n",
               n, expected, start_reg);
        return false;
    }

    // Validate address and function
    if (resp[0] != NEXTPM_ADDR || resp[1] != FUNC_READ_HOLDING) {
        // Check for Modbus exception (function | 0x80)
        if (resp[1] == (FUNC_READ_HOLDING | 0x80)) {
            printf("[nextpm] Modbus exception 0x%02X (reg 0x%02X)\n",
                   resp[2], start_reg);
        }
        return false;
    }

    // Validate byte count
    if (resp[2] != count * 2)
        return false;

    // Validate CRC
    uint16_t rx_crc = resp[n - 2] | (resp[n - 1] << 8);
    if (rx_crc != modbus_crc16(resp, n - 2)) {
        printf("[nextpm] CRC error (reg 0x%02X)\n", start_reg);
        return false;
    }

    // Extract register values (big-endian in Modbus)
    for (uint16_t i = 0; i < count; i++) {
        regs[i] = (resp[3 + i * 2] << 8) | resp[3 + i * 2 + 1];
    }

    return true;
}

// --- Public API ---

static nextpm::Data s_last_data = {};

namespace nextpm {

bool init() {
    uart_init(NEXTPM_UART, NEXTPM_BAUD);
    gpio_set_function(NEXTPM_TX, GPIO_FUNC_UART);
    gpio_set_function(NEXTPM_RX, GPIO_FUNC_UART);

    // 8E1: 8 data bits, even parity, 1 stop bit (NextPM Modbus RTU)
    uart_set_format(NEXTPM_UART, 8, 1, UART_PARITY_EVEN);
    uart_set_hw_flow(NEXTPM_UART, false, false);

    printf("[nextpm] UART0 initialized (GP0 TX, GP1 RX, %d baud, 8E1)\n",
           NEXTPM_BAUD);
    return true;
}

Data read() {
    Data d = {};

    // Status register (1 x 16-bit)
    uint16_t status_reg;
    if (read_holding_registers(REG_STATUS, 1, &status_reg)) {
        d.status = status_reg;
    }

    // PM values (2 x 16-bit each = 32-bit)
    uint16_t pm_regs[2];

    if (read_holding_registers(REG_PM1_1MIN, 2, pm_regs)) {
        uint32_t raw = ((uint32_t)pm_regs[1] << 16) | pm_regs[0];
        d.pm1 = raw / 1000.0f;
        d.ok = true;
    }

    if (read_holding_registers(REG_PM25_1MIN, 2, pm_regs)) {
        uint32_t raw = ((uint32_t)pm_regs[1] << 16) | pm_regs[0];
        d.pm25 = raw / 1000.0f;
        d.ok = true;
    }

    if (read_holding_registers(REG_PM10_1MIN, 2, pm_regs)) {
        uint32_t raw = ((uint32_t)pm_regs[1] << 16) | pm_regs[0];
        d.pm10 = raw / 1000.0f;
        d.ok = true;
    }

    // Internal temperature & humidity (1 x 16-bit each)
    uint16_t temp_reg;
    if (read_holding_registers(REG_TEMP_INT, 1, &temp_reg)) {
        d.temperature = (int16_t)temp_reg / 100.0f;
    }

    uint16_t hum_reg;
    if (read_holding_registers(REG_HUM_INT, 1, &hum_reg)) {
        d.humidity = hum_reg / 100.0f;
    }

    s_last_data = d;
    return d;
}

const Data& get_last() {
    return s_last_data;
}

}  // namespace nextpm
