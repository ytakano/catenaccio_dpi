#ifndef CDPI_CALLBACK_HPP
#define CDPI_CALLBACK_HPP

#include "cdpi_tcp.hpp"
#include "cdpi_udp.hpp"

class cdpi_callback {
public:
    cdpi_callback(std::string conf);
    virtual ~cdpi_callback() { }

    void operator() (char *bytes, size_t len, uint8_t proto);

private:
    ptr_cdpi_appif m_appif;
    cdpi_tcp m_tcp;
    cdpi_udp m_udp;

};

#endif
