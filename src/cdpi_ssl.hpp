#ifndef CDPI_SSL_HPP
#define CDPI_SSL_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_event.hpp"
#include "cdpi_proto.hpp"

#include <stdint.h>

#include <deque>
#include <list>
#include <map>

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

class cdpi_ssl;

typedef boost::shared_ptr<cdpi_ssl> ptr_cdpi_ssl;

class cdpi_ssl : public cdpi_proto {
public:
    cdpi_ssl(cdpi_proto_type type, const cdpi_id_dir &id_dir,
             cdpi_stream &stream, ptr_cdpi_event_listener listener);
    virtual ~cdpi_ssl();

    static bool is_ssl_client(std::deque<cdpi_bytes> &bytes);
    static bool is_ssl_server(std::deque<cdpi_bytes> &bytes);

    void parse(std::deque<cdpi_bytes> &bytes);

    uint16_t get_ver() { return m_ver; }
    const uint8_t* get_random() { return m_random; }
    cdpi_bytes get_session_id() { return m_session_id; }
    uint32_t get_gmt_unix_time() { return m_gmt_unix_time; }
    const std::list<uint16_t>& get_cipher_suites() { return m_cipher_suites; }
    const std::list<uint8_t>& get_compression_methods() { return m_compression_methods; }
    const std::deque<cdpi_bytes>& get_certificats() { return m_certificates; }
    const cdpi_id_dir &get_id_dir() { return m_id_dir; }
    bool is_change_cihper_spec() { return m_is_change_cipher_spec; }

    std::string num_to_cipher(uint16_t num);
    std::string num_to_compression(uint8_t num);

private:
    uint16_t                m_ver;
    uint8_t                 m_random[28];
    uint32_t                m_gmt_unix_time;
    cdpi_bytes              m_session_id;
    std::deque<cdpi_bytes>   m_certificates;
    std::list<uint16_t>     m_cipher_suites;
    std::list<uint8_t>      m_compression_methods;
    cdpi_id_dir             m_id_dir;
    cdpi_stream            &m_stream;
    ptr_cdpi_event_listener m_listener;
    bool                    m_is_change_cipher_spec;

    void parse_handshake(char *data, int len);
    void parse_client_hello(char *data, int len);
    void parse_server_hello(char *data, int len);
    void parse_certificate(char *data, int len);
    void parse_change_cipher_spec(char *data, int len);

};

#endif // CDPI_SSL_HPP
