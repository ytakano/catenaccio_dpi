#include "cdpi_http.hpp"

#include <boost/regex.hpp>

using namespace std;

static boost::regex regex_http_req("^[A-Z]+ .+ HTTP/(1.0|1.1)\r?\n(([a-zA-Z]|-)+: .+\r?\n)+.*");
static boost::regex regex_http_res("^HTTP/(1.0|1.1) [0-9]{3} .+\r?\n(([a-zA-Z]|-)+: .+\r?\n)+.*");

cdpi_http::cdpi_http()
{

}

cdpi_http::~cdpi_http()
{

}

bool
cdpi_http::is_http_client(std::list<cdpi_bytes> &bytes)
{
    if (bytes.size() == 0)
        return false;

    cdpi_bytes buf = bytes.front();
    string     data(buf.m_ptr.get() + buf.m_pos,
                    buf.m_ptr.get() + buf.m_pos + buf.m_len);

    return boost::regex_match(data, regex_http_req);
}

bool
cdpi_http::is_http_server(std::list<cdpi_bytes> &bytes)
{
    if (bytes.size() == 0)
        return false;

    cdpi_bytes buf = bytes.front();
    string     data(buf.m_ptr.get() + buf.m_pos,
                    buf.m_ptr.get() + buf.m_pos + buf.m_len);

    return boost::regex_match(data, regex_http_res);
}
