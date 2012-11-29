#ifndef CDPI_SSL_HPP
#define CDPI_SSL_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_proto.hpp"

#include <stdint.h>

#include <list>

struct cdpi_ssl_record {
    uint8_t  m_content_type;
    uint16_t m_version;
};

class cdpi_ssl : public cdpi_proto {
public:
    cdpi_ssl(cdpi_proto_type type);
    virtual ~cdpi_ssl();

    static bool is_ssl_client(std::list<cdpi_bytes> &bytes);
    static bool is_ssl_server(std::list<cdpi_bytes> &bytes);

private:
    cdpi_proto_type m_type;

};

#endif // CDPI_SSL_HPP
