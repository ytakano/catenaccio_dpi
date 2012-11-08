#ifndef CDPI_DIVERT_HPP
#define CDPI_DIVERT_HPP

#include <stdint.h>
#include <event.h>

#include <boost/shared_ptr.hpp>

class cdpi_callback {
public:
    cdpi_callback() { }
    virtual ~cdpi_callback() { }

    virtual void operator()(uint8_t *bytes, size_t len) = 0;
};

typedef boost::shared_ptr<cdpi_callback> cdpi_callback_ptr;


class cdpi_divert {
public:
    cdpi_divert() {}
    virtual ~cdpi_divert() {}

    void set_ev_base(event_base *ev_base) { m_ev_base = ev_base; }
    void set_callback_ipv4(cdpi_callback_ptr func) {
        m_callback_ipv4 = func;
    }
    void set_callback_ipv6(cdpi_callback_ptr func) {
        m_callback_ipv6 = func;
    }
    void run(uint16_t ipv4_port, uint16_t ipv6_port);

private:
    int  open_divert(uint16_t port);

    cdpi_callback_ptr m_callback_ipv4;
    cdpi_callback_ptr m_callback_ipv6;

    event_base *m_ev_base;
    event      *m_ev_ipv4;
    event      *m_ev_ipv6;
    int         m_fd_ipv4;
    int         m_fd_ipv6;

    friend void callback_ipv4(evutil_socket_t fd, short what, void *arg);
    friend void callback_ipv6(evutil_socket_t fd, short what, void *arg);
};

#endif // CDPI_DIVERT_HPP
