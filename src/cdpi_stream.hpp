#ifndef CDPI_STREAM_HPP
#define CDPI_STREAM_HPP

#include "cdpi_tcp.hpp"

enum cdpi_stream_event {
    STREAM_CREATED,
    STREAM_DATA_IN,
    STREAM_DESTROYED,
    STREAM_ERROR,
};

class cdpi_stream {
public:
    cdpi_stream();
    virtual ~cdpi_stream();

    void in_stream_event();

private:
    

};

#endif // CDPI_STREAM_HPP
