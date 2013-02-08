#ifndef CDPI_EVENT_HPP
#define CDPI_EVENT_HPP

#include "cdpi_id.hpp"
#include "cdpi_stream.hpp"

#include <boost/shared_ptr.hpp>

enum cdpi_event {
    CDPI_EVENT_STREAM_OPEN,
    CDPI_EVENT_STREAM_CLOSE,
    CDPI_EVENT_PROTOCOL_DETECTED,
    CDPI_EVENT_HTTP_READ_METHOD,
    CDPI_EVENT_HTTP_READ_RESPONSE,
    CDPI_EVENT_HTTP_READ_HEAD,
    CDPI_EVENT_HTTP_READ_BODY,
    CDPI_EVENT_HTTP_READ_TRAILER,
    CDPI_EVENT_HTTP_PROXY,
};

class cdpi_stream;

class cdpi_event_listener {
public:
    cdpi_event_listener() { }
    virtual ~cdpi_event_listener() { }

    virtual void in_stream(cdpi_event cev, const cdpi_id_dir &id_dir,
                              cdpi_stream &stream) = 0;
    virtual void in_datagram(cdpi_event cev, const cdpi_id_dir &id_dir) = 0;
};

typedef boost::shared_ptr<cdpi_event_listener> ptr_cdpi_event_listener;

#endif // CDPI_EVENT_HPP
