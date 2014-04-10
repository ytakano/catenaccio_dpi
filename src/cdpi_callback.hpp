#ifndef CDPI_CALLBACK_HPP
#define CDPI_CALLBACK_HPP

#include "cdpi_tcp.hpp"
#include "cdpi_udp.hpp"

class cdpi_callback {
public:
    cdpi_callback();
    virtual ~cdpi_callback() { }

    void set_event_listener(ptr_cdpi_event_listener listener) {
        //m_tcp.set_event_listener(listener);
        m_udp.set_event_listener(listener);
    }

    void operator() (char *bytes, size_t len, uint8_t proto);
private:
    ptr_cdpi_stream m_stream;
    cdpi_tcp m_tcp;
    cdpi_udp m_udp;

};

#endif
