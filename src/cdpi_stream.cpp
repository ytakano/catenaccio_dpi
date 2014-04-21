#include "cdpi_stream.hpp"
#include "cdpi_ssl.hpp"
#include "cdpi_http.hpp"

using namespace std;
/*
cdpi_stream::cdpi_stream()
{

}

cdpi_stream::~cdpi_stream()
{

}

ptr_cdpi_proto
cdpi_stream::get_proto(cdpi_id_dir id_dir)
{
   map<cdpi_id_dir, ptr_cdpi_stream_info>::iterator it;

    it = m_info.find(id_dir);
    if (it == m_info.end())
        return ptr_cdpi_proto();

    return it->second->m_proto;
}

void
cdpi_stream::in_stream_event(cdpi_stream_event st_event,
                             const cdpi_id_dir &id_dir, cdpi_bytes bytes)
{
    switch (st_event) {
    case STREAM_OPEN:
    case STREAM_DATA:
    case STREAM_FIN:
    case STREAM_RST:
    case STREAM_TIMEOUT:
    case STREAM_DESTROYED:
        ;
    }
*/
/*
    switch (st_event) {
    case STREAM_OPEN:
        create_stream(id_dir);
        m_listener->in_stream(CDPI_EVENT_STREAM_OPEN, id_dir, *this);
        break;
    case STREAM_DATA_IN:
    {
        cdpi_id_dir id_dir2;
        cdpi_bytes  bytes2;

        id_dir2.m_id  = id_dir.m_id;
        id_dir2.m_dir = id_dir.m_dir == FROM_ADDR1 ? FROM_ADDR2 : FROM_ADDR1;

        in_data(id_dir, bytes);
        in_data(id_dir2, bytes2);
        
        break;
    }
    case STREAM_DESTROYED:
    case STREAM_ERROR:
        m_listener->in_stream(CDPI_EVENT_STREAM_CLOSE, id_dir, *this);
        destroy_stream(id_dir);
        break;
    }
*/
//}

/*
void
cdpi_stream::create_stream(const cdpi_id_dir &id_dir)
{
    map<cdpi_id_dir, ptr_cdpi_stream_info>::iterator it;

    it = m_info.find(id_dir);
    if (it != m_info.end())
        return;


    ptr_cdpi_stream_info info(new cdpi_stream_info);

    m_info[id_dir] = info;
}

void
cdpi_stream::destroy_stream(const cdpi_id_dir &id_dir)
{
    m_info.erase(id_dir);
}

void
cdpi_stream::in_data(const cdpi_id_dir &id_dir, cdpi_bytes bytes)
{
    map<cdpi_id_dir, ptr_cdpi_stream_info>::iterator it;

    it = m_info.find(id_dir);
    if (it == m_info.end() || it->second->m_is_gaveup)
        return;

    if (bytes.m_ptr) {
        it->second->m_bytes.push_back(bytes);
        it->second->m_total_size += bytes.m_len;
    }

    if (it->second->m_type == PROTO_NONE ||
        it->second->m_type == PROTO_HTTP_PROXY) {
        // protocol detection
        if (cdpi_http::is_http_client(it->second->m_bytes)) {
            it->second->m_type  = PROTO_HTTP_CLIENT;
            it->second->m_proto = ptr_cdpi_proto(
                new cdpi_http(PROTO_HTTP_CLIENT, id_dir, *this,
                              m_listener));

            m_listener->in_stream(CDPI_EVENT_PROTOCOL_DETECTED, id_dir, *this);
        } else if (cdpi_http::is_http_server(it->second->m_bytes)) {
            it->second->m_type  = PROTO_HTTP_SERVER;
            it->second->m_proto = ptr_cdpi_proto(
                new cdpi_http(PROTO_HTTP_SERVER, id_dir, *this,
                              m_listener));

            // set client
            map<cdpi_id_dir, ptr_cdpi_stream_info>::iterator it_peer;
            cdpi_id_dir id_dir_peer;

            id_dir_peer.m_dir =
                it->first.m_dir == FROM_ADDR1 ? FROM_ADDR2 : FROM_ADDR1;
            id_dir_peer.m_id  = id_dir.m_id;

            it_peer = m_info.find(id_dir_peer);
            if (it_peer != m_info.end()) {
                if (it_peer->second->m_proxy.size() > 0) {
                    list<ptr_cdpi_proto>::reverse_iterator r_it;
                    int i = it->second->m_proxy.size();

                    for (r_it = it_peer->second->m_proxy.rbegin();
                         i > 0 && r_it != it_peer->second->m_proxy.rend();
                         ++it) {
                        i--;
                    }

                    if (i == 0) {
                        ptr_cdpi_http p_http, p_peer;

                        p_http = PROTO_TO_HTTP(it->second->m_proto);
                        p_peer = PROTO_TO_HTTP(*r_it);

                        if (p_peer)
                            p_http->set_peer(p_peer);

                    }
                } else if (it_peer->second->m_type == PROTO_HTTP_CLIENT ||
                           it_peer->second->m_type == PROTO_HTTP_PROXY) {
                    ptr_cdpi_http p_http;

                    p_http = PROTO_TO_HTTP(it->second->m_proto);
                    p_http->set_peer(PROTO_TO_HTTP(it_peer->second->m_proto));
                }
            }

            m_listener->in_stream(CDPI_EVENT_PROTOCOL_DETECTED, id_dir, *this);
        } else if (cdpi_ssl::is_ssl_client(it->second->m_bytes)) {
            it->second->m_type  = PROTO_SSL_CLIENT;
            it->second->m_proto = ptr_cdpi_proto(new cdpi_ssl(PROTO_SSL_CLIENT,
                                                              id_dir,
                                                              *this,
                                                              m_listener));

            m_listener->in_stream(CDPI_EVENT_PROTOCOL_DETECTED, id_dir, *this);
        } else if (cdpi_ssl::is_ssl_server(it->second->m_bytes)) {
            it->second->m_type  = PROTO_SSL_SERVER;
            it->second->m_proto = ptr_cdpi_proto(new cdpi_ssl(PROTO_SSL_SERVER,
                                                              id_dir,
                                                              *this,
                                                              m_listener));

            m_listener->in_stream(CDPI_EVENT_PROTOCOL_DETECTED, id_dir, *this);
        } else {
            if (it->second->m_bytes.size() > 8) {
                it->second->m_is_gaveup = true;
                it->second->m_bytes.clear();
            }
            return;
        }
    }

    try {
        switch (it->second->m_type) {
        case PROTO_HTTP_CLIENT:
        case PROTO_HTTP_SERVER:
        {
            ptr_cdpi_http p_http;

            p_http = PROTO_TO_HTTP(it->second->m_proto);

            p_http->parse(it->second->m_bytes);
            
            break;
        }
        case PROTO_SSL_CLIENT:
        case PROTO_SSL_SERVER:
        {
            ptr_cdpi_ssl p_ssl;

            p_ssl = PROTO_TO_SSL(it->second->m_proto);

            p_ssl->parse(it->second->m_bytes);
        }
        default:
            ;
        }
    } catch (cdpi_parse_error parse_error) {
        it->second->m_is_gaveup = true;
        it->second->m_bytes.clear();

        cout << "parse error: " << parse_error.m_msg << endl;
    } catch (cdpi_proxy proxy) {
        it->second->m_type = PROTO_HTTP_PROXY;
        it->second->m_proxy.push_back(it->second->m_proto);
    }
}

uint64_t
cdpi_stream::get_sent_bytes(cdpi_id_dir id_dir)
{
    map<cdpi_id_dir, ptr_cdpi_stream_info>::iterator it;

    it = m_info.find(id_dir);
    if (it == m_info.end())
        return 0;

    return it->second->m_total_size;
}

double
cdpi_stream::get_bps(cdpi_id_dir id_dir)
{
    map<cdpi_id_dir, ptr_cdpi_stream_info>::iterator it;

    it = m_info.find(id_dir);
    if (it == m_info.end())
        return 0.0;

    return (double)it->second->m_total_size * 8 / (double)(time(NULL) - it->second->m_init_time);
}
*/
