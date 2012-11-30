#ifndef CDPI_PCAP_HPP
#define CDPI_PCAP_HPP

#include "cdpi_divert.hpp"
#include "cdpi_event.hpp"

#include <stdint.h>

#include <string>

class cdpi_pcap {
public:
    cdpi_pcap() { }
    virtual ~cdpi_pcap() { }

    void set_dev(std::string dev);
    void run();

    void callback(const struct pcap_pkthdr *h, const uint8_t *bytes);

    void set_callback_ipv4(cdpi_callback_ptr func) {
        m_callback_ipv4 = func;
    }
    void set_callback_ipv6(cdpi_callback_ptr func) {
        m_callback_ipv6 = func;
    }

private:
    std::string m_dev;
    int m_dl_type;

    const uint8_t *get_ip_hdr(const uint8_t *bytes, uint32_t len,
                              uint8_t &proto);

    cdpi_callback_ptr m_callback_ipv4;
    cdpi_callback_ptr m_callback_ipv6;
};

void run_pcap(std::string dev, ptr_cdpi_event_listener listener);

#endif // CDPI_PCAP_HPP
