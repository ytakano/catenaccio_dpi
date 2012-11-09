#ifndef CDPI_HTTP_PROTO
#define CDPI_HTTP_PROTO

#include <boost/shared_ptr.hpp>

enum cdpi_proto_type {
    PROTO_HTTP_CLIENT,
    PROTO_HTTP_SERVER,
    PROTO_HTTP_PROXY,
    PROTO_NONE,
};

class cdpi_proto {
public:
    cdpi_proto();
    virtual ~cdpi_proto() = 0;

};

typedef boost::shared_ptr<cdpi_proto> ptr_cdpi_proto;

#endif // CDPI_HTTP_PROTO
