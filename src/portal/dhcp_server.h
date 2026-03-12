#pragma once

// Adapted from pico-examples (MIT License, Damien P. George)

#include "lwip/ip_addr.h"

#define DHCPS_BASE_IP (100)
#define DHCPS_MAX_IP  (11)

typedef struct {
    uint8_t mac[6];
    uint16_t expiry;
} dhcp_server_lease_t;

typedef struct {
    ip_addr_t ip;
    ip_addr_t nm;
    dhcp_server_lease_t lease[DHCPS_MAX_IP];
    struct udp_pcb *udp;
} dhcp_server_t;

#ifdef __cplusplus
extern "C" {
#endif

void dhcp_server_init(dhcp_server_t *d, ip_addr_t *ip, ip_addr_t *nm);
void dhcp_server_deinit(dhcp_server_t *d);

#ifdef __cplusplus
}
#endif
