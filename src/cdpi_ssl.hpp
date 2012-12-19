#ifndef CDPI_SSL_HPP
#define CDPI_SSL_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_proto.hpp"

#include <stdint.h>

#include <list>

enum cdpi_ssl_content_type {
    SSL_CHANGE_CIPHER_SPEC = 20,
    SSL_ALERT              = 21,
    SSL_HANDSHAKE          = 22,
    SSL_APPLICATION_DATA   = 23,
    SSL_HEARTBEAT          = 24,
};

enum cdpi_ssl_handshake_type {
    SSL_HELLO_REQUEST        = 0,
    SSL_CLIENT_HELLO         = 1,
    SSL_SERVER_HELLO         = 2,
    SSL_HELLO_VERIFY_REQUEST = 3,
    SSL_NEW_SESSION_TICKET   = 4,
    SSL_CERTIFICATE          = 11,
    SSL_SERVER_KEY_EXCHANGE  = 12,
    SSL_CERTIFICATE_REQUEST  = 13,
    SSL_SERVER_HELLO_DONE    = 14,
    SSL_CERTIFICATE_VERIFY   = 15,
    SSL_CLIENT_KEY_EXCHANGE  = 16,
    SSL_FINISHED             = 20,
    SSL_CERTIFICATE_URL      = 21,
    SSL_CERTIFICATE_STATUS   = 22,
    SSL_SUPPLEMENTAL_DATA    = 23,
};

class cdpi_ssl : public cdpi_proto {
public:
    cdpi_ssl(cdpi_proto_type type);
    virtual ~cdpi_ssl();

    static bool is_ssl_client(std::list<cdpi_bytes> &bytes);
    static bool is_ssl_server(std::list<cdpi_bytes> &bytes);

    void parse(std::list<cdpi_bytes> &bytes);

private:
    cdpi_proto_type     m_type;
    uint8_t             m_ver;
    uint8_t             m_random[28];
    uint32_t            m_gmt_unix_time;
    cdpi_bytes          m_session_id;
    std::list<uint16_t> m_cipher_suites;
    std::list<uint8_t>  m_compression_methods;

    void parse_handshake(char *data, int len);
    void parse_client_hello(char *data, int len);
    void parse_server_hello(char *data, int len);

};

#endif // CDPI_SSL_HPP
