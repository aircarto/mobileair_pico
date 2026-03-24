#pragma once

#include <cstdint>

namespace datalog {

/// Initialize the datalog: read header from SD card sector 0
/// to resume writing at the correct position.
/// Returns false if SD card is not available.
bool init();

/// Append a CSV line (without trailing newline — it is added automatically).
/// Returns false if SD card is unavailable or write fails.
bool append(const char* line);

/// Flush the current write buffer to SD (partial sector).
void flush();

/// Return the total number of CSV lines written.
uint32_t get_line_count();

/// Return the total data bytes written (excluding header sector).
uint32_t get_data_bytes();

/// Read CSV data from SD card into buf, starting at byte offset `from`.
/// Returns the number of bytes actually read (0 if nothing to read).
/// Used by the web server to stream CSV data in chunks.
int read_data(char* buf, int buf_size, uint32_t from);

}  // namespace datalog
