#include "dns_server.h"

#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include <cstring>
#include <cstdio>

static struct udp_pcb* s_dns_pcb = nullptr;
static uint32_t s_captive_ip = 0;

// Minimal DNS response: answer all A queries with captive IP
static void dns_recv_cb(void* arg, struct udp_pcb* pcb, struct pbuf* p,
                        const ip_addr_t* addr, u16_t port) {
    if (!p || p->tot_len < 12) {
        if (p) pbuf_free(p);
        return;
    }

    uint8_t* req = (uint8_t*)p->payload;
    printf("[dns] Query from %s:%d (%d bytes)\n", ipaddr_ntoa(addr), port, p->tot_len);

    // Extract transaction ID
    uint16_t txid = (req[0] << 8) | req[1];

    // Build response
    // Fixed size: 12 header + question (copy from request) + 16 answer
    // We need to find end of question section
    int qend = 12;
    // Skip QNAME
    while (qend < (int)p->tot_len && req[qend] != 0) {
        qend += req[qend] + 1; // label length + label
    }
    qend++; // skip null terminator
    qend += 4; // skip QTYPE + QCLASS

    int resp_len = qend + 16; // answer: name ptr(2) + type(2) + class(2) + ttl(4) + rdlen(2) + ip(4)

    struct pbuf* resp = pbuf_alloc(PBUF_TRANSPORT, resp_len, PBUF_RAM);
    if (!resp) {
        pbuf_free(p);
        return;
    }

    uint8_t* r = (uint8_t*)resp->payload;
    memset(r, 0, resp_len);

    // Copy question section header + question
    memcpy(r, req, qend);

    // Set response flags
    r[2] = 0x81; // QR=1, RD=1
    r[3] = 0x80; // RA=1
    r[6] = 0x00; r[7] = 0x01; // ANCOUNT = 1

    // Answer section
    int a = qend;
    r[a + 0] = 0xC0; r[a + 1] = 0x0C; // Name pointer to question
    r[a + 2] = 0x00; r[a + 3] = 0x01; // Type A
    r[a + 4] = 0x00; r[a + 5] = 0x01; // Class IN
    r[a + 6] = 0x00; r[a + 7] = 0x00; // TTL = 60
    r[a + 8] = 0x00; r[a + 9] = 60;
    r[a + 10] = 0x00; r[a + 11] = 0x04; // RDLENGTH = 4
    // IP in network byte order
    r[a + 12] = (s_captive_ip >> 24) & 0xFF;
    r[a + 13] = (s_captive_ip >> 16) & 0xFF;
    r[a + 14] = (s_captive_ip >> 8) & 0xFF;
    r[a + 15] = s_captive_ip & 0xFF;

    udp_sendto(pcb, resp, addr, port);

    pbuf_free(resp);
    pbuf_free(p);
}

namespace dns_server {

bool start(uint32_t captive_ip) {
    if (s_dns_pcb) return true;

    s_captive_ip = captive_ip;

    s_dns_pcb = udp_new();
    if (!s_dns_pcb) {
        printf("[dns] Failed to create PCB\n");
        return false;
    }

    err_t err = udp_bind(s_dns_pcb, IP_ADDR_ANY, 53);
    if (err != ERR_OK) {
        printf("[dns] Bind failed (err=%d)\n", err);
        udp_remove(s_dns_pcb);
        s_dns_pcb = nullptr;
        return false;
    }

    udp_recv(s_dns_pcb, dns_recv_cb, nullptr);
    printf("[dns] Captive DNS started\n");
    return true;
}

void stop() {
    if (s_dns_pcb) {
        udp_remove(s_dns_pcb);
        s_dns_pcb = nullptr;
        printf("[dns] Stopped\n");
    }
}

} // namespace dns_server
