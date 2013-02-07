#ifndef CDPI_CALLBACK_HPP
#define CDPI_CALLBACK_HPP

#include "cdpi_tcp.hpp"

class cdpi_callback {
public:
    cdpi_callback() { }
    virtual ~cdpi_callback() { }

    void set_event_listener(ptr_cdpi_event_listener listener) {
        m_tcp.set_event_listener(listener);
    }

    void operator() (char *bytes, size_t len, uint8_t proto);
private:
    cdpi_tcp m_tcp;

    // TODO
    // boost::shared_ptr<cdpi_tcp> m_udp;

};

typedef boost::shared_ptr<cdpi_callback> cdpi_callback_ptr;

#endif
