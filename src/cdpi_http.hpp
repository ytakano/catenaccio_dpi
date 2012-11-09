#ifndef CDPI_HTTP_HPP
#define CDPI_HTTP_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_proto.hpp"

#include <list>
#include <map>
#include <queue>

class cdpi_http;

typedef boost::shared_ptr<cdpi_http> ptr_cdpi_http;

class cdpi_http : public cdpi_proto {
public:
    cdpi_http(cdpi_proto_type type);
    virtual ~cdpi_http();

    static bool is_http_client(std::list<cdpi_bytes> &bytes);
    static bool is_http_server(std::list<cdpi_bytes> &bytes);

    void parse(std::list<cdpi_bytes> &bytes);
    void set_peer(ptr_cdpi_http peer) { m_peer = peer; }

private:
    enum http_state {
        HTTP_METHOD,
        HTTP_RESPONSE,
        HTTP_HEAD,
        HTTP_BODY,
        HTTP_CHUNK_LEN,
        HTTP_CHUNK_BODY,
        HTTP_CHUNK_EL,
        HTTP_CHUNK_TRAILER,
    };

    cdpi_proto_type m_type;
    http_state m_state;
    std::queue<std::string> m_method;
    std::string m_uri;
    std::string m_ver;
    std::string m_code;
    std::string m_res_msg;
    std::map<std::string, std::string> m_header;
    int m_body_read;
    int m_chunk_len;
    ptr_cdpi_http m_peer;

    bool parse_method(std::list<cdpi_bytes> &bytes);
    bool parse_response(std::list<cdpi_bytes> &bytes);
    bool parse_head(std::list<cdpi_bytes> &bytes);
    bool parse_body(std::list<cdpi_bytes> &bytes);
    bool parse_chunk_len(std::list<cdpi_bytes> &bytes);
    bool parse_chunk_body(std::list<cdpi_bytes> &bytes);
    bool parse_chunk_el(std::list<cdpi_bytes> &bytes);

    void set_header(std::string key, std::string val);
    std::string get_header(std::string key);
};


#endif // CDPI_HTTP_HPP
