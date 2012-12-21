#ifndef CDPI_PCAP_HPP
#define CDPI_PCAP_HPP

#include "cdpi_divert.hpp"
#include "cdpi_event.hpp"

#include <pcap/pcap.h>

#include <stdint.h>

#include <string>

class cdpi_pcap {
public:
    cdpi_pcap() : m_handle(NULL), m_is_break(false) { }
    virtual ~cdpi_pcap() {
        if (m_handle != NULL)
            pcap_close(m_handle);
    }

    void set_dev(std::string dev);
    void run();

    void callback(const struct pcap_pkthdr *h, const uint8_t *bytes);

    void set_callback_ipv4(cdpi_callback_ptr func) {
        m_callback_ipv4 = func;
    }
    void set_callback_ipv6(cdpi_callback_ptr func) {
        m_callback_ipv6 = func;
    }

    void stop() { m_is_break = true; }

private:
    std::string m_dev;
    pcap_t *m_handle;
    int     m_dl_type;
    bool    m_is_break;

    const uint8_t *get_ip_hdr(const uint8_t *bytes, uint32_t len,
                              uint8_t &proto);

    cdpi_callback_ptr m_callback_ipv4;
    cdpi_callback_ptr m_callback_ipv6;
};

void run_pcap(std::string dev, ptr_cdpi_event_listener listener);
void stop_pcap();

#endif // CDPI_PCAP_HPP
