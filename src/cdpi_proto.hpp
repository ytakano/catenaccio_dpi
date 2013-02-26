#ifndef CDPI_HTTP_PROTO
#define CDPI_HTTP_PROTO

#include <boost/shared_ptr.hpp>

#include <list>

enum cdpi_proto_type {
    // TCP stream
    PROTO_HTTP_CLIENT,
    PROTO_HTTP_SERVER,
    PROTO_HTTP_PROXY,
    PROTO_SSL_CLIENT,
    PROTO_SSL_SERVER,

    // UDP datagram
    PROTO_BENCODE,

    // others
    PROTO_NONE,
};

class cdpi_proto {
public:
    cdpi_proto();
    virtual ~cdpi_proto() = 0;
};

typedef boost::shared_ptr<cdpi_proto> ptr_cdpi_proto;

#endif // CDPI_HTTP_PROTO
