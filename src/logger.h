#pragma once

namespace logger {

/// Register a custom stdio driver that captures all printf output
/// into a 100-line ring buffer. Call after stdio_init_all().
void init();

/// Write all buffered lines into `buf` (newline-separated).
/// Returns the number of bytes written (excluding null terminator).
int get_lines(char* buf, int buf_size);

}  // namespace logger
