#ifndef CDPI_TCP_HPP
#define CDPI_TCP_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_id.hpp"

#include <stdint.h>
#include <time.h>

#include <list>
#include <map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>

#define MAX_PACKETS 32

struct cdpi_tcp_packet {
    cdpi_bytes m_bytes;
    uint32_t   m_seq;
    uint32_t   m_nxt_seq;
    uint8_t    m_flags;
    int        m_data_pos;
    int        m_data_len;
    int        m_read_pos;
};

struct cdpi_tcp_uniflow {
    std::map<uint32_t, cdpi_tcp_packet> m_packets;
    time_t   m_time;
    uint32_t m_min_seq;
    bool     m_is_gaveup;
    bool     m_is_fin;

    cdpi_tcp_uniflow() : m_min_seq(0), m_is_gaveup(false), m_is_fin(false) { }
};

struct cdpi_tcp_flow {
    cdpi_tcp_uniflow m_flow1, m_flow2;
};

struct cdpi_tcp_event {
    cdpi_id        m_id;
    cdpi_direction m_dir;

    bool operator< (const cdpi_tcp_event &rhs) const {
        if (m_dir == rhs.m_dir)
            return m_id < rhs.m_id;

        return m_dir < rhs.m_dir;
    }

    bool operator> (const cdpi_tcp_event &rhs) const {
        return rhs < *this;
    }

    bool operator== (const cdpi_tcp_event &rhs) const {
        return m_dir == rhs.m_dir && m_id == rhs.m_id;
    }
};

typedef boost::shared_ptr<cdpi_tcp_flow> ptr_cdpi_tcp_flow;

typedef boost::multi_index::multi_index_container<
    cdpi_tcp_event,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::identity<cdpi_tcp_event> >,
        boost::multi_index::sequenced<>
    >
> cdpi_tcp_event_cont;

class cdpi_tcp {
public:
    cdpi_tcp();
    virtual ~cdpi_tcp();

    void input_tcp(cdpi_id &id, cdpi_direction dir, char *buf, int len);
    void run();

private:
    std::map<cdpi_id, ptr_cdpi_tcp_flow> m_flow;
    cdpi_tcp_event_cont                  m_events;

    void input_tcp4(cdpi_id &id, cdpi_direction dir, char *buf, int len);
    bool get_packet(const cdpi_id &id, cdpi_direction dir,
                    cdpi_tcp_packet &packet);
    bool recv_fin(const cdpi_id &id, cdpi_direction dir);
    void recv_rst(const cdpi_id &id, cdpi_direction dir);
    int  num_packets(const cdpi_id &id, cdpi_direction dir);

    boost::thread    m_thread;
    boost::mutex     m_mutex;
    boost::condition m_condition;
};

#endif // CDPI_TCP_HPP
