#ifndef CDPI_UDP_HPP
#define CDPI_UDP_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_event.hpp"
#include "cdpi_id.hpp"

#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>

#include <queue>

#define PROTO_TO_BENCODE(PROTO) boost::dynamic_pointer_cast<cdpi_bencode>(PROTO)
#define PROTO_TO_DNS(PROTO) boost::dynamic_pointer_cast<cdpi_dns>(PROTO)

struct cdpi_udp_packet {
    cdpi_id_dir m_id_dir;
    cdpi_bytes  m_bytes;
};

class cdpi_udp {
public:
    cdpi_udp();
    virtual ~cdpi_udp();

    void input_udp(cdpi_id &id, cdpi_direction dir, char *buf, int len,
                   char *l4hdr);
    void run();
    void set_event_listener(ptr_cdpi_event_listener listener) {
        m_listener = listener;
    }

private:
    std::queue<cdpi_udp_packet> m_queue;
    ptr_cdpi_event_listener     m_listener;

    bool             m_is_del;
    boost::mutex     m_mutex;
    boost::condition m_condition;
    boost::thread    m_thread;


    void input_udp4(cdpi_id &id, cdpi_direction dir, char *buf, int len);

};

#endif // CDPI_UDP_HPP
