#ifndef CDPI_HTTP_HPP
#define CDPI_HTTP_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_event.hpp"
#include "cdpi_id.hpp"
#include "cdpi_proto.hpp"
#include "cdpi_stream.hpp"

#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>

class cdpi_stream;
class cdpi_http;

typedef boost::shared_ptr<cdpi_http> ptr_cdpi_http;

class cdpi_http : public cdpi_proto {
public:
    cdpi_http(cdpi_proto_type type, const cdpi_id_dir &id_dir,
              cdpi_stream &stream, ptr_cdpi_event_listener listener);
    virtual ~cdpi_http();

    static bool is_http_client(std::list<cdpi_bytes> &bytes);
    static bool is_http_server(std::list<cdpi_bytes> &bytes);

    void parse(std::list<cdpi_bytes> &bytes);
    void set_peer(ptr_cdpi_http peer) { m_peer = peer; }
    void set_event_listener(ptr_cdpi_event_listener &listener) {
        m_listener = listener;
    }

    void add_mime_to_read(std::string mime) { m_mime_to_read.insert(mime); }
    void del_mime_to_raed(std::string mime) { m_mime_to_read.erase(mime); }

    cdpi_proto_type get_proto_type() const { return m_type; }
    std::string get_method() const {
        return m_method.size() > 0 ? m_method.back() : ""; }
    std::string get_uri() const { return m_uri; }
    std::string get_ver() const { return m_ver; }
    std::string get_res_code() const { return m_code; }
    std::string get_res_msg() const { return m_res_msg; }
    std::string get_header(std::string key) const;
    std::string get_trailer(std::string key) const;
    void get_header_keys(std::list<std::string> &keys) const;
    void get_trailer_keys(std::list<std::string> &keys) const;
    cdpi_bytes get_content() { return m_content; }

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
    std::map<std::string, std::string> m_trailer;
    std::set<std::string> m_mime_to_read;
    int m_body_read;
    int m_chunk_len;
    ptr_cdpi_http m_peer;
    cdpi_id_dir   m_id_dir;
    cdpi_stream  &m_stream;
    cdpi_bytes    m_content;
    std::list<cdpi_bytes>   m_chunks;
    ptr_cdpi_event_listener m_listener;


    bool parse_method(std::list<cdpi_bytes> &bytes);
    bool parse_response(std::list<cdpi_bytes> &bytes);
    bool parse_head(std::list<cdpi_bytes> &bytes);
    bool parse_body(std::list<cdpi_bytes> &bytes);
    bool parse_chunk_len(std::list<cdpi_bytes> &bytes);
    bool parse_chunk_body(std::list<cdpi_bytes> &bytes);
    bool parse_chunk_el(std::list<cdpi_bytes> &bytes);

    void set_header(std::string key, std::string val);
    void set_trailer(std::string key, std::string val);

    bool is_in_mime_to_read();
};


#endif // CDPI_HTTP_HPP
