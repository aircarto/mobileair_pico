#include "modem/modem.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <cstring>

// UART1 on GP4 (TX) / GP5 (RX)
#define MODEM_UART      uart1
#define MODEM_UART_TX   4
#define MODEM_UART_RX   5
#define MODEM_BAUD      115200

namespace modem {

bool init() {
    uart_init(MODEM_UART, MODEM_BAUD);
    gpio_set_function(MODEM_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(MODEM_UART_RX, GPIO_FUNC_UART);

    // 8N1 (default after uart_init, but be explicit)
    uart_set_format(MODEM_UART, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(MODEM_UART, false, false);

    printf("[modem] UART1 initialized (GP4 TX, GP5 RX, %d baud)\n", MODEM_BAUD);
    return true;
}

bool send_at(const char* cmd, char* resp, size_t resp_len, uint32_t timeout_ms) {
    // Flush any pending RX data
    while (uart_is_readable(MODEM_UART)) {
        uart_getc(MODEM_UART);
    }

    // Send command + CR
    uart_puts(MODEM_UART, cmd);
    uart_putc(MODEM_UART, '\r');

    // Read response until "OK" or "ERROR" or timeout
    size_t pos = 0;
    uint32_t start = to_ms_since_boot(get_absolute_time());

    while ((to_ms_since_boot(get_absolute_time()) - start) < timeout_ms) {
        if (uart_is_readable(MODEM_UART)) {
            char c = uart_getc(MODEM_UART);
            if (pos < resp_len - 1) {
                resp[pos++] = c;
                resp[pos] = '\0';
            }
            // Check for termination
            if (strstr(resp, "OK") || strstr(resp, "ERROR")) {
                return true;
            }
        } else {
            sleep_ms(1);
        }
    }

    return pos > 0;  // partial response counts as something
}

bool check() {
    char resp[256] = {};
    if (send_at("ATI", resp, sizeof(resp), 3000)) {
        if (strstr(resp, "OK")) {
            printf("[modem] %s\n", resp);
            return true;
        }
        if (strstr(resp, "ERROR")) {
            printf("[modem] ERROR\n");
            return false;
        }
        printf("[modem] Partial: %s\n", resp);
        return false;
    }

    printf("[modem] No response - check wiring\n");
    return false;
}

}  // namespace modem
