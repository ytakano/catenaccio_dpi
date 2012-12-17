#ifndef CDPI_DIVERT_HPP
#define CDPI_DIVERT_HPP

#include "cdpi_id.hpp"
#include "cdpi_tcp.hpp"

#include <stdint.h>
#include <event.h>

#include <boost/shared_ptr.hpp>

class cdpi_callback {
public:
    cdpi_callback() { }
    virtual ~cdpi_callback() { }

    virtual void operator()(char *bytes, size_t len) = 0;
};

class cb_ipv4 : public cdpi_callback {
public:
    cb_ipv4(boost::shared_ptr<cdpi_tcp> p_tcp) : m_tcp(p_tcp) { }
    virtual ~cb_ipv4() { }

    virtual void operator() (char *bytes, size_t len) {
        cdpi_direction dir;
        cdpi_id        id;

        dir = id.set_iph(bytes, IPPROTO_IP);

        switch (id.get_l4_proto()) {
        case IPPROTO_TCP:
            m_tcp->input_tcp(id, dir, bytes, len);
            break;
        default:
            ;
        }
    }

private:
    boost::shared_ptr<cdpi_tcp> m_tcp;

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

void run_divert(int port, ptr_cdpi_event_listener listener);


#endif // CDPI_DIVERT_HPP
