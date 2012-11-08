#ifndef CDPI_STREAM_HPP
#define CDPI_STREAM_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_id.hpp"
#include "cdpi_proto.hpp"

#include <list>
#include <map>

#include <boost/shared_ptr.hpp>

enum cdpi_stream_event {
    STREAM_CREATED,
    STREAM_DATA_IN,
    STREAM_DESTROYED,
    STREAM_ERROR,
};

struct cdpi_stream_info {
    std::list<cdpi_bytes> m_bytes;
    cdpi_proto_type       m_proto;
};

typedef boost::shared_ptr<cdpi_stream_info> ptr_cdpi_stream_info;

class cdpi_stream {
public:
    cdpi_stream();
    virtual ~cdpi_stream();

    void in_stream_event(cdpi_stream_event st_event, const cdpi_id_dir &id_dir,
                         cdpi_bytes bytes);

private:
    std::map<cdpi_id_dir, ptr_cdpi_stream_info> m_info;

    void create_stream(const cdpi_id_dir &id_dir);
    void destroy_stream(const cdpi_id_dir &id_dir);

};

#endif // CDPI_STREAM_HPP
