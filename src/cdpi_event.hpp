#ifndef CDPI_EVENT_HPP
#define CDPI_EVENT_HPP

#include "cdpi_id.hpp"
#include "cdpi_stream.hpp"

#include <boost/shared_ptr.hpp>

enum cdpi_event {
    CDPI_EVENT_STREAM_OPEN,
    CDPI_EVENT_STREAM_CLOSE,
    CDPI_EVENT_PROTOCOL_DETECTED,
};

class cdpi_stream;

class cdpi_event_listener {
public:
    cdpi_event_listener() { }
    virtual ~cdpi_event_listener() { }

    virtual void operator() (cdpi_event cev, const cdpi_id_dir &id_dir,
                             cdpi_stream &stream) = 0;
};

typedef boost::shared_ptr<cdpi_event_listener> ptr_cdpi_event_listener;

#endif // CDPI_EVENT_HPP
