#ifndef CDPI_MONGO_HPP
#define CDPI_MONGO_HPP

#include "cdpi.hpp"

#include <mongo/client/dbclient.h>

#include <map>

#include <boost/date_time/posix_time/posix_time.hpp>

class my_event_listener : public cdpi_event_listener {
public:
    my_event_listener();
    virtual ~my_event_listener();

    virtual void in_stream(cdpi_event cev, const cdpi_id_dir &id_dir,
                           cdpi_stream &stream);
    virtual void in_datagram(cdpi_event cev, const cdpi_id_dir &id_dir,
                             ptr_cdpi_proto data);

private:
    typedef boost::posix_time::ptime ptime;

    class tcp_info {
    public:
        bool  m_is_opened;
        int   m_num_open;
        ptime m_time_open;

        tcp_info() : m_num_open(0) { }
    };

    mongo::DBClientConnection   m_mongo;
    std::map<cdpi_id, tcp_info> m_tcp;

    void open_tcp(const cdpi_id_dir &id_dir);
    void close_tcp(const cdpi_id_dir &id_dir, cdpi_stream &stream);
    void in_bencode(const cdpi_id_dir &id_dir, ptr_cdpi_bencode bc);
    void in_dht_nodes(const cdpi_id_dir &id_dir,
                      cdpi_bencode::ptr_ben_str bstr);
};

#endif // CDPI_MONGO_HPP
