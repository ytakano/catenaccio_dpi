#include "cdpi_http.hpp"
#include "cdpi_stream.hpp"

#include <boost/regex.hpp>

#include <assert.h>

using namespace std;

static boost::regex regex_http_req("(^[A-Z]+ .+ HTTP/1.0\r?\n.*|^[A-Z]+ .+ HTTP/1.1\r?\n(([a-zA-Z]|-)+: .+\r?\n)*.*)");
static boost::regex regex_http_res("(^HTTP/1.0 [0-9]{3} .+\r?\n.*|^HTTP/1.1 [0-9]{3} .+\r?\n(([a-zA-Z]|-)+: .+\r?\n)*.*)");

cdpi_http::cdpi_http(cdpi_proto_type type, const cdpi_id_dir &id_dir,
                     cdpi_stream &stream, ptr_cdpi_event_listener listener) :
    m_body_read(0), m_id_dir(id_dir), m_stream(stream),
    m_listener(listener)
{
    assert(type == PROTO_HTTP_CLIENT || type == PROTO_HTTP_SERVER);

    if (type == PROTO_HTTP_CLIENT) {
        m_state = HTTP_METHOD;
    } else if (type == PROTO_HTTP_SERVER) {
        m_state = HTTP_RESPONSE;
    }

    m_type = type;
}

cdpi_http::~cdpi_http()
{

}

bool
cdpi_http::is_http_client(list<cdpi_bytes> &bytes)
{
    if (bytes.size() == 0)
        return false;

    cdpi_bytes buf = bytes.front();
    string     data(buf.m_ptr.get() + buf.m_pos,
                    buf.m_ptr.get() + buf.m_pos + buf.m_len);

    return boost::regex_match(data, regex_http_req);
}

bool
cdpi_http::is_http_server(list<cdpi_bytes> &bytes)
{
    if (bytes.size() == 0)
        return false;

    cdpi_bytes buf = bytes.front();
    string     data(buf.m_ptr.get() + buf.m_pos,
                    buf.m_ptr.get() + buf.m_pos + buf.m_len);

    return boost::regex_match(data, regex_http_res);
}

void
cdpi_http::parse(list<cdpi_bytes> &bytes)
{
    for (;;) {
         bool ret = false;

         switch (m_state) {
         case HTTP_METHOD:
             ret = parse_method(bytes);
             break;
         case HTTP_RESPONSE:
             ret = parse_response(bytes);
             break;
         case HTTP_HEAD:
         case HTTP_CHUNK_TRAILER:
             ret = parse_head(bytes);
             break;
         case HTTP_BODY:
             ret = parse_body(bytes);
             break;
         case HTTP_CHUNK_LEN:
             ret = parse_chunk_len(bytes);
             break;
         case HTTP_CHUNK_BODY:
             ret = parse_chunk_body(bytes);
             break;
         case HTTP_CHUNK_EL:
             ret = parse_chunk_el(bytes);
             break;
         }

         if (! ret)
             break;
     }
 }

 bool
 cdpi_http::parse_method(list<cdpi_bytes> &bytes)
 {
     int   len;
     int   remain;
     int   n;
     char  buf[1024 * 4];
     char *p = buf;

     len = remain = read_bytes_ec(bytes, buf, sizeof(buf), '\n');

     if (len == 0 || buf[len - 1] != '\n')
         return false;


     m_header.clear();


     // read method
     n = find_char((char*)p, remain, ' ');
     if (n < 0)
         throw cdpi_parse_error(__FILE__, __LINE__);

     m_method.push(string(p, p + n));
     p += n + 1;
     remain -= n + 1;

     // read URI
     n = find_char((char*)p, remain, ' ');
     if (n < 0)
         throw cdpi_parse_error(__FILE__, __LINE__);

     m_uri = string(p, p + n);
     p += n + 1;
     remain -= n + 1;

     // read version
     n = find_char((char*)p, remain, '\n');
     if (n < 0)
         throw cdpi_parse_error(__FILE__, __LINE__);

     if (n > 0 && p[n - 1] == '\r')
         m_ver = string(p, p + n - 1);
     else
         m_ver = string(p, p + n);

     // skip read buffer
     skip_bytes(bytes, len);

     // change state to HTTP_HEAD
     m_state = cdpi_http::HTTP_HEAD;

     // event http method
     m_listener->in_stream(CDPI_EVENT_HTTP_READ_METHOD, m_id_dir, m_stream);

     return true;
 }

 bool
 cdpi_http::parse_response(list<cdpi_bytes> &bytes)
 {
     int  n;
     int  remain;
     int  len;
     char buf[1024 * 4];
     char *p = buf;

     len = remain = read_bytes_ec(bytes, buf, sizeof(buf), '\n');

     if (len == 0 || buf[len - 1] != '\n')
         return false;


     m_header.clear();


     // read http version
     n = find_char((char*)p, remain, ' ');
     if (n < 0)
         throw cdpi_parse_error(__FILE__, __LINE__);

     m_ver = string(p, p + n);

     p += n + 1;
     remain -= n + 1;

     // read status code
     n = find_char((char*)p, remain, ' ');
     if (n < 0)
         throw cdpi_parse_error(__FILE__, __LINE__);

     m_code = string(p, p + n);

     p += n + 1;
     remain -= n + 1;

     // read responce message
     n = find_char((char*)p, remain, '\n');
     if (n < 0)
         throw cdpi_parse_error(__FILE__, __LINE__);

     if (n > 0 && p[n - 1] == '\r')
         m_res_msg = string(p, p + n - 1);
     else
         m_res_msg = string(p, p + n);

     // skip read buffer
     skip_bytes(bytes, len);

     // change state to HTTP_HEAD
     m_state = cdpi_http::HTTP_HEAD;

     cout << m_ver << " " << m_code << " " << m_res_msg << endl;

     // event http response
     m_listener->in_stream(CDPI_EVENT_HTTP_READ_RESPONSE, m_id_dir, m_stream);

     return true;
 }

 bool
 cdpi_http::parse_head(list<cdpi_bytes> &bytes)
 {
     char buf[1024 * 8];

     for (;;) {
         const int len = read_bytes_ec(bytes, buf, sizeof(buf), '\n');


        if (len == 0 || buf[len - 1] != '\n') {
            return false;
        }

        if ((len == 2 && memcmp(buf, "\r\n", 2) ==0) ||
            (len == 1 && buf[0] == '\n')) {

            switch (m_type) {
            case PROTO_HTTP_CLIENT:
            {
                skip_bytes(bytes, len);

                if (m_state == HTTP_CHUNK_TRAILER) {
                    m_state = HTTP_METHOD;

                    // event trailer
                    m_listener->in_stream(CDPI_EVENT_HTTP_READ_TRAILER,
                                          m_id_dir, m_stream);
                } else if (m_method.back() == "CONNECT") {
                    cout << "proxy: connect" << endl;

                    // event proxy
                    m_listener->in_stream(CDPI_EVENT_HTTP_READ_HEAD,
                                          m_id_dir, m_stream);

                    m_listener->in_stream(CDPI_EVENT_HTTP_PROXY,
                                          m_id_dir, m_stream);

                    throw cdpi_proxy();
                } else {
                    string con_len, tr_enc;

                    con_len = get_header("content-length");
                    tr_enc  = get_header("transfer-encoding");

                    to_lower_str(tr_enc);

                    if (con_len != "") {
                        m_state = cdpi_http::HTTP_BODY;
                    } else if (tr_enc == "chunked") {
                        m_state = cdpi_http::HTTP_CHUNK_LEN;
                    } else {
                        m_state = cdpi_http::HTTP_METHOD;
                    }

                    // event head
                    m_listener->in_stream(CDPI_EVENT_HTTP_READ_HEAD,
                                          m_id_dir, m_stream);
                }

                break;
            }
            case PROTO_HTTP_SERVER:
            {
                string method;

                if (m_peer) {
                    if (m_peer->m_method.size() == 0)
                        return false;

                    method = m_peer->m_method.front();
                    m_peer->m_method.pop();
                } else {
                    return false;
                }

                skip_bytes(bytes, len);

                if (m_state == HTTP_CHUNK_TRAILER) {
                    m_state = HTTP_RESPONSE;

                    // event trailer
                    m_listener->in_stream(CDPI_EVENT_HTTP_READ_TRAILER,
                                          m_id_dir, m_stream);

                } else if (method == "CONNECT" && m_code == "200") {
                    cout << "proxy: connection established" << endl;

                    // event proxy
                    m_listener->in_stream(CDPI_EVENT_HTTP_READ_HEAD,
                                          m_id_dir, m_stream);

                    m_listener->in_stream(CDPI_EVENT_HTTP_PROXY,
                                          m_id_dir, m_stream);

                    throw cdpi_proxy();
                } else if (method == "HEAD" || m_code == "204" ||
                           m_code == "205" || m_code == "304") {
                    m_state = HTTP_RESPONSE;
                } else {
                    string tr_enc;

                    tr_enc = get_header("transfer-encoding");

                    if (tr_enc == "chunked") {
                        m_state = cdpi_http::HTTP_CHUNK_LEN;
                    } else {
                        m_state = cdpi_http::HTTP_BODY;
                    }

                    // event head
                    m_listener->in_stream(CDPI_EVENT_HTTP_READ_HEAD,
                                          m_id_dir, m_stream);
                }

                break;
            }
            default:
                throw cdpi_parse_error(__FILE__, __LINE__);
            }

            return true;
        } else {
            string key, val;
            int    pos = find_char((char*)buf, len, ':');
            char  *p;

            if (pos < 0) {
                skip_bytes(bytes, len);
                continue;
            }

            key = string(buf, buf + pos);

            p = buf + pos + 1;

            while (p < buf + len && *p == ' ')
                p++;

            if (p >= buf + len) {
                skip_bytes(bytes, len);
                continue;
            }

            for (char *p2 = p; p2 < buf + len; p2++) {
                if (*p2 == '\r' || *p2 == '\n') {
                    *p2 = '\0';
                    break;
                }
            }

            val = string((char*)p);

            set_header(key, val);

            skip_bytes(bytes, len);

            cout << key << " = " << val << endl;
        }
    }
}

void
cdpi_http::set_header(string key, string val)
{
    to_lower_str(key);
    m_header[key] = val;
}

string
cdpi_http::get_header(string key)
{
    map<string, string>::iterator it;

    to_lower_str(key);

    it = m_header.find(key);
    if (it == m_header.end())
        return "";

    return it->second;
}

bool
cdpi_http::parse_body(list<cdpi_bytes> &bytes)
{
    stringstream ss;
    int  content_len;
    int  len;
    char buf[1024];

    ss << get_header("content-length");
    ss >> content_len;

    while (m_body_read < content_len) {
        len = content_len - m_body_read;
        len = len < (int)sizeof(buf) ? len : sizeof(buf);

        len = skip_bytes(bytes, len);

        if (len == 0)
            break;

        m_body_read += len;
    }

    if (m_body_read >= content_len) {
        m_header.clear();

        switch (m_type) {
        case PROTO_HTTP_CLIENT:
            m_state = cdpi_http::HTTP_METHOD;
            break;
        case PROTO_HTTP_SERVER:
            m_state = cdpi_http::HTTP_RESPONSE;
            break;
        default:
            // not to reach
            assert(m_type == PROTO_HTTP_CLIENT ||
                   m_type == PROTO_HTTP_SERVER);
            return false;
        }

        m_body_read = 0;

        // event body
        m_listener->in_stream(CDPI_EVENT_HTTP_READ_BODY, m_id_dir, m_stream);

        return true;
    }

    return false;
}

bool
cdpi_http::parse_chunk_len(list<cdpi_bytes> &bytes)
{
    stringstream ss;
    int  len;
    char buf[128];

    len = read_bytes_ec(bytes, buf, sizeof(buf), '\n');

    if (len == 0)
        return false;

    if (len > 0 && buf[len - 1] != '\n') {
        // TODO: parse error
        return false;
    }

    ss << buf;
    ss >> hex >> m_chunk_len;

    if (m_chunk_len == 0) {
        m_state = cdpi_http::HTTP_CHUNK_EL;
    } else {
        m_state = cdpi_http::HTTP_CHUNK_BODY;
    }

    skip_bytes(bytes, len);

    return true;
}

bool
cdpi_http::parse_chunk_body(list<cdpi_bytes> &bytes)
{
    int  len;
    char buf[1024];

    while (m_body_read < m_chunk_len) {
        len = m_chunk_len - m_body_read;
        len = len < (int)sizeof(buf) ? len : sizeof(buf);

        len = skip_bytes(bytes, len);

        if (len == 0)
            break;

        m_body_read += len;
    }

    if (m_body_read >= m_chunk_len) {
        m_state = HTTP_CHUNK_EL;
        return true;
    }

    return false;
}

bool
cdpi_http::parse_chunk_el(list<cdpi_bytes> &bytes)
{
    int  len;
    char buf[8];

    len = read_bytes_ec(bytes, buf, sizeof(buf), '\n');

    if (len == 1 && buf[0] == '\n' ||
        len == 2 && memcmp(buf, "\r\n", 2) == 0) {
        skip_bytes(bytes, len);

        if (m_chunk_len == 0) {
            m_state = cdpi_http::HTTP_CHUNK_TRAILER;
        } else {
            m_state = cdpi_http::HTTP_CHUNK_LEN;
        }

        m_body_read = 0;

        // event body
        m_listener->in_stream(CDPI_EVENT_HTTP_READ_BODY, m_id_dir, m_stream);

        return true;
    }

    return false;
}
