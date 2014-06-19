#ifndef CDPI_DIVERT_HPP
#define CDPI_DIVERT_HPP

#ifdef USE_DIVERT

#include "cdpi_id.hpp"
#include "cdpi_callback.hpp"
#include "cdpi_tcp.hpp"

#include <stdint.h>
#include <event.h>

#include <iostream>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/regex.hpp>


class cdpi_divert {
public:
    cdpi_divert(std::string conf) : m_callback(conf) {}
    virtual ~cdpi_divert() {}

    void set_ev_base(event_base *ev_base) { m_ev_base = ev_base; }
    void set_event_listener(ptr_cdpi_event_listener listener) {
        m_callback.set_event_listener(listener);
    }

    void run(uint16_t ipv4_port, uint16_t ipv6_port);

private:
    int  open_divert(uint16_t port);

    cdpi_callback m_callback;

    event_base *m_ev_base;
    event      *m_ev_ipv4;
    event      *m_ev_ipv6;
    int         m_fd_ipv4;
    int         m_fd_ipv6;

    friend void callback_ipv4(evutil_socket_t fd, short what, void *arg);
    friend void callback_ipv6(evutil_socket_t fd, short what, void *arg);
};

void run_divert(int port, std::string conf);

#endif // USE_DIVERT

#endif // CDPI_DIVERT_HPP
