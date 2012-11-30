#ifndef CDPI_STREAM_HPP
#define CDPI_STREAM_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_event.hpp"
#include "cdpi_id.hpp"
#include "cdpi_proto.hpp"

#include <list>
#include <map>
#include <sstream>

#include <boost/shared_ptr.hpp>

enum cdpi_stream_event {
    STREAM_CREATED,
    STREAM_DATA_IN,
    STREAM_DESTROYED,
    STREAM_ERROR,
};

struct cdpi_stream_info {
    std::list<cdpi_bytes>     m_bytes;
    std::list<ptr_cdpi_proto> m_proxy;
    cdpi_proto_type           m_type;
    ptr_cdpi_proto            m_proto;
    bool                      m_is_gaveup;

    cdpi_stream_info() : m_type(PROTO_NONE), m_is_gaveup(false) { }
};

class cdpi_parse_error {
public:
    cdpi_parse_error(std::string file, int line) {
        std::stringstream ss;

        ss << file << ":" << line;

        m_msg = ss.str();
    }

    std::string m_msg;
};

class cdpi_proxy { };

typedef boost::shared_ptr<cdpi_stream_info> ptr_cdpi_stream_info;

class cdpi_stream {
public:
    cdpi_stream();
    virtual ~cdpi_stream();

    void in_stream_event(cdpi_stream_event st_event, const cdpi_id_dir &id_dir,
                         cdpi_bytes bytes);

    void set_event_listener(ptr_cdpi_event_listener &listener) {
        m_listener = listener;
    }

private:
    std::map<cdpi_id_dir, ptr_cdpi_stream_info> m_info;

    void create_stream(const cdpi_id_dir &id_dir);
    void destroy_stream(const cdpi_id_dir &id_dir);
    void in_data(const cdpi_id_dir &id_dir, cdpi_bytes bytes);

    ptr_cdpi_event_listener m_listener;

};

#endif // CDPI_STREAM_HPP
