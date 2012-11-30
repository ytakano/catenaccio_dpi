#include "cdpi_ssl.hpp"
#include "cdpi_stream.hpp"

#include <arpa/inet.h>

#include <boost/regex.hpp>

using namespace std;

#define SSL20_VER 0x0200
#define SSL30_VER 0x0300
#define TLS10_VER 0x0301

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

void
cdpi_ssl::parse(list<cdpi_bytes> &bytes)
{
    uint16_t ver;
    uint16_t len;
    uint8_t  type;
    char     buf[5];
    int      read_len;

    for (;;) {
        read_len = read_bytes(bytes, buf, sizeof(buf));

        if (read_len < (int)sizeof(buf))
            return;

        type = (uint8_t)buf[0];

        memcpy(&ver, &buf[1], sizeof(ver));
        memcpy(&len, &buf[3], sizeof(len));

        ver = ntohs(ver);
        len = ntohs(len);


        boost::shared_array<char> data(new char[len]);

        read_len = read_bytes(bytes, data.get(), len);

        if (read_len < len)
            return;

        skip_bytes(bytes, read_len + sizeof(buf));

        switch (type) {
        case SSL_HANDSHAKE:
        case SSL_CHANGE_CIPHER_SPEC:
        case SSL_ALERT:
        case SSL_APPLICATION_DATA:
        case SSL_HEARTBEAT:
            break;
        default:
            throw cdpi_parse_error(__FILE__, __LINE__);
        }
    }
}
