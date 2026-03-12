#pragma once

#include <cstdint>
#include <cstddef>

namespace modem {

/// Initialize UART0 for communication with the SARA-R500 modem.
/// TX=GP0, RX=GP1, 115200 8N1.
bool init();

/// Send an AT command and wait for a response.
/// @param cmd      Command string (e.g. "ATI")
/// @param resp     Buffer to store the response
/// @param resp_len Size of the response buffer
/// @param timeout_ms Maximum time to wait for a response
/// @return true if a response was received before timeout
bool send_at(const char* cmd, char* resp, size_t resp_len, uint32_t timeout_ms = 2000);

/// Send "ATI" and print modem identification to stdio.
/// @return true if the modem responded
bool check();

}  // namespace modem
