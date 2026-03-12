#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// Based on pico-examples lwipopts_examples_common.h

// --- System ---
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// --- Memory ---
// CRITICAL: use libc malloc in poll mode (avoids lwIP heap lock issues)
#define MEM_LIBC_MALLOC             1
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_UDP_PCB            8
#define PBUF_POOL_SIZE              24

// --- Core protocols ---
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_DHCP                   1
#define LWIP_DNS                    1
#define LWIP_UDP                    1
#define LWIP_TCP                    1
#define LWIP_IPV4                   1
#define LWIP_IGMP                   1

// --- TCP tuning ---
#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_TCP_KEEPALIVE          1
#define LWIP_NETIF_TX_SINGLE_PBUF   1

// --- DHCP ---
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

// --- Netif callbacks ---
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1

// --- HTTPD ---
#define LWIP_HTTPD_CGI              0
#define LWIP_HTTPD_SSI              0
#define LWIP_HTTPD_CUSTOM_FILES     1
#define LWIP_HTTPD_DYNAMIC_FILE_READ 1
#define LWIP_HTTPD_DYNAMIC_HEADERS  0
#define LWIP_HTTPD_FILE_STATE       0
#define LWIP_HTTPD_SUPPORT_POST     1
#define HTTPD_SERVER_PORT           80
#define LWIP_HTTPD_SUPPORT_11_KEEPALIVE 0

// --- mDNS ---
#define LWIP_MDNS_RESPONDER         1
#define LWIP_NUM_NETIF_CLIENT_DATA  1
#define MDNS_MAX_SERVICES           1

// --- Timers ---
#define LWIP_TIMERS                 1
#define MEMP_NUM_SYS_TIMEOUT        16

// --- Checksum ---
#define LWIP_CHKSUM_ALGORITHM       3

// --- Stats ---
#define LWIP_STATS                  0
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0

#endif // _LWIPOPTS_H
