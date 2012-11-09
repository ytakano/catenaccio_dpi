#ifndef CDPI_HTTP_HPP
#define CDPI_HTTP_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_proto.hpp"

#include <list>

class cdpi_http : public cdpi_proto {
public:
    cdpi_http();
    virtual ~cdpi_http();

    static bool is_http_client(std::list<cdpi_bytes> &bytes);
    static bool is_http_server(std::list<cdpi_bytes> &bytes);

};

#endif // CDPI_HTTP_HPP
