#include "cdpi_ssl.hpp"

#include <boost/regex.hpp>

using namespace std;

/*
 * Content Type: Handshake(0x16)
 * Version: SSL2.0(0x0200), SSL3.0(0x0300), TLS1.0(0x0301)
 * Length: 2 bytes
 * Handshake Type: Cleint Hello(0x01), Server Hello(0x02)
 * Length: 3 bytes
 * Version: SSL2.0(0x0200), SSL3.0(0x0300), TLS1.0(0x0301)
 */

static boost::regex regex_ssl_client_hello("^\\x16(\\x02\\x00|\\x03\\x00|\\x03\\x01)..\\x01...\\1.*");
static boost::regex regex_ssl_server_hello("^\\x16(\\x02\\x00|\\x03\\x00|\\x03\\x01)..\\x02...\\1.*");

cdpi_ssl::cdpi_ssl(cdpi_proto_type type)
{
    m_type = type;
}

cdpi_ssl::~cdpi_ssl()
{

}

bool
cdpi_ssl::is_ssl_client(list<cdpi_bytes> &bytes)
{
    if (bytes.size() == 0)
        return false;

    cdpi_bytes buf = bytes.front();
    string     data(buf.m_ptr.get() + buf.m_pos,
                    buf.m_ptr.get() + buf.m_pos + buf.m_len);


    //print_binary(data.c_str(), data.size());

    //cout << endl;

    return boost::regex_match(data, regex_ssl_client_hello);
}

bool
cdpi_ssl::is_ssl_server(list<cdpi_bytes> &bytes)
{
    if (bytes.size() == 0)
        return false;

    cdpi_bytes buf = bytes.front();
    string     data(buf.m_ptr.get() + buf.m_pos,
                    buf.m_ptr.get() + buf.m_pos + buf.m_len);

    return boost::regex_match(data, regex_ssl_server_hello);
}
