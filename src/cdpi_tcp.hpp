#ifndef CDPI_TCP_HPP
#define CDPI_TCP_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_id.hpp"
#include "cdpi_stream.hpp"

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
    bool     m_is_syn;
    bool     m_is_fin;
    bool     m_is_rm;

    cdpi_tcp_uniflow() : m_min_seq(0), m_is_gaveup(false), m_is_syn(false),
                         m_is_fin(false), m_is_rm(false) { }
};

struct cdpi_tcp_flow {
    cdpi_tcp_uniflow m_flow1, m_flow2;
};

typedef boost::shared_ptr<cdpi_tcp_flow> ptr_cdpi_tcp_flow;

typedef boost::multi_index::multi_index_container<
    cdpi_id_dir,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::identity<cdpi_id_dir> >,
        boost::multi_index::sequenced<>
    >
> cdpi_id_dir_cont;

class cdpi_tcp {
public:
    cdpi_tcp();
    virtual ~cdpi_tcp();

    void input_tcp(cdpi_id &id, cdpi_direction dir, char *buf, int len,
                   char *l4hdr);
    void run();
    void garbage_collector();
    void set_event_listener(ptr_cdpi_event_listener listener) {
        m_stream.set_event_listener(listener);
    }

private:
    std::map<cdpi_id, ptr_cdpi_tcp_flow> m_flow;
    cdpi_id_dir_cont                     m_events;
    cdpi_stream                          m_stream;

    bool get_packet(const cdpi_id &id, cdpi_direction dir,
                    cdpi_tcp_packet &packet);
    bool recv_fin(const cdpi_id &id, cdpi_direction dir);
    void rm_flow(const cdpi_id &id, cdpi_direction dir);
    int  num_packets(const cdpi_id &id, cdpi_direction dir);

    boost::mutex     m_mutex;
    boost::mutex     m_mutex_gc;
    boost::condition m_condition;
    boost::condition m_condition_gc;
    bool             m_is_del;

    boost::thread    m_thread_run;
    boost::thread    m_thread_gc;
};

#endif // CDPI_TCP_HPP
