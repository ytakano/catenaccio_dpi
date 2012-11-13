#include "cdpi_stream.hpp"
#include "cdpi_http.hpp"

using namespace std;

cdpi_stream::cdpi_stream()
{

}

cdpi_stream::~cdpi_stream()
{

}

void
cdpi_stream::in_stream_event(cdpi_stream_event st_event,
                             const cdpi_id_dir &id_dir, cdpi_bytes bytes)
{
    switch (st_event) {
    case STREAM_CREATED:
        create_stream(id_dir);
        break;
    case STREAM_DATA_IN:
        in_data(id_dir, bytes);
        break;
    case STREAM_DESTROYED:
    {
        cdpi_bytes bytes;
        in_data(id_dir, bytes);
    }
    case STREAM_ERROR:
        destroy_stream(id_dir);
        break;
    }
}

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

    if (bytes.m_ptr)
        it->second->m_bytes.push_back(bytes);

    if (it->second->m_type == PROTO_NONE ||
        it->second->m_type == PROTO_HTTP_PROXY) {
        // protocol detection
        if (cdpi_http::is_http_client(it->second->m_bytes)) {
            it->second->m_type = PROTO_HTTP_CLIENT;
            it->second->m_proto = ptr_cdpi_proto(new cdpi_http(PROTO_HTTP_CLIENT));

            cout << "protocol: HTTP Client" << endl;
        } else if (cdpi_http::is_http_server(it->second->m_bytes)) {
            it->second->m_type  = PROTO_HTTP_SERVER;
            it->second->m_proto = ptr_cdpi_proto(new cdpi_http(PROTO_HTTP_SERVER));

            // set client
            map<cdpi_id_dir, ptr_cdpi_stream_info>::iterator it_peer;
            cdpi_id_dir id_dir_peer;

            id_dir_peer.m_dir =
                it->first.m_dir == FROM_ADDR1 ? FROM_ADDR2 : FROM_ADDR1;
            id_dir_peer.m_id  = id_dir.m_id;

            it_peer = m_info.find(id_dir_peer);
            if (it_peer != m_info.end() &&
                (it_peer->second->m_type == PROTO_HTTP_CLIENT ||
                 it_peer->second->m_type == PROTO_HTTP_PROXY)) {
                ptr_cdpi_http p_http;

                p_http = boost::dynamic_pointer_cast<cdpi_http>(it->second->m_proto);
                p_http->set_peer(boost::dynamic_pointer_cast<cdpi_http>(it_peer->second->m_proto));
            }

            cout << "protocol: HTTP Server" << endl;
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

            p_http = boost::dynamic_pointer_cast<cdpi_http>(it->second->m_proto);

            p_http->parse(it->second->m_bytes);
            
            break;
        }
        default:
            ;
        }
    } catch (cdpi_parse_error parse_error) {
        it->second->m_is_gaveup = true;
        it->second->m_bytes.clear();

        cout << parse_error.m_msg << endl;
    } catch (cdpi_proxy proxy) {
        it->second->m_type = PROTO_HTTP_PROXY;
    }
}
