#ifndef CDPI_EVENT_HPP
#define CDPI_EVENT_HPP

#include "cdpi_id.hpp"
#include "cdpi_stream.hpp"
#include "cdpi_proto.hpp"

#include <boost/shared_ptr.hpp>

enum cdpi_event {
    // TCP stream
    CDPI_EVENT_STREAM_OPEN,
    CDPI_EVENT_STREAM_CLOSE,
    CDPI_EVENT_PROTOCOL_DETECTED,

    // HTTP
    CDPI_EVENT_HTTP_READ_METHOD,
    CDPI_EVENT_HTTP_READ_RESPONSE,
    CDPI_EVENT_HTTP_READ_HEAD,
    CDPI_EVENT_HTTP_READ_BODY,
    CDPI_EVENT_HTTP_READ_TRAILER,
    CDPI_EVENT_HTTP_READ,
    CDPI_EVENT_HTTP_PROXY,

    // TLS/SSL
    CDPI_EVENT_SSL_CLIENT_HELLO,
    CDPI_EVENT_SSL_SERVER_HELLO,
    CDPI_EVENT_SSL_CERTIFICATE,
    CDPI_EVENT_SSL_CHANGE_CIPHER_SPEC,

    // DNS
    CDPI_EVENT_DNS,

    // UDP datagram
    CDPI_EVENT_BENCODE,
};

class cdpi_stream;

class cdpi_event_listener {
public:
    cdpi_event_listener() { }
    virtual ~cdpi_event_listener() { }

    virtual void in_stream(cdpi_event cev, const cdpi_id_dir &id_dir,
                           cdpi_stream &stream) = 0;
    virtual void in_datagram(cdpi_event cev, const cdpi_id_dir &id_dir,
                             ptr_cdpi_proto data) = 0;
};

typedef boost::shared_ptr<cdpi_event_listener> ptr_cdpi_event_listener;

#endif // CDPI_EVENT_HPP
