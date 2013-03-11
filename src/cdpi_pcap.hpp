#ifndef CDPI_PCAP_HPP
#define CDPI_PCAP_HPP

#include "cdpi_callback.hpp"
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
    void set_event_listener(ptr_cdpi_event_listener listener) {
        m_callback.set_event_listener(listener);
    }

    void callback(const struct pcap_pkthdr *h, const uint8_t *bytes);

    void run();
    void stop() { m_is_break = true; }

private:
    std::string m_dev;
    pcap_t *m_handle;
    int     m_dl_type;
    bool    m_is_break;

    const uint8_t *get_ip_hdr(const uint8_t *bytes, uint32_t len,
                              uint8_t &proto);

    cdpi_callback m_callback;
};

extern boost::shared_ptr<cdpi_pcap> pcap_inst;
extern bool pcap_is_running;

void stop_pcap();

template <class LISTENER>
void
run_pcap(std::string dev)
{
    for (;;) {
        if (pcap_is_running) {
            stop_pcap();
            sleep(1);
        } else {
            break;
        }
    }

    pcap_is_running = true;

    pcap_inst = boost::shared_ptr<cdpi_pcap>(new cdpi_pcap);

    pcap_inst->set_event_listener(ptr_cdpi_event_listener(new LISTENER));
    pcap_inst->set_dev(dev);
    pcap_inst->run();

    pcap_is_running = false;
}

#endif // CDPI_PCAP_HPP
