#pragma once

#include <cstdint>

namespace dns_server {
    // Start captive DNS server (all queries → captive_ip)
    bool start(uint32_t captive_ip);

    // Stop DNS server
    void stop();
}
