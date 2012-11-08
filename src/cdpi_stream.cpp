#include "cdpi_stream.hpp"

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
        break;
    case STREAM_DESTROYED:
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
    info->m_proto = PROTO_NONE;

    m_info[id_dir] = info;
}

void
cdpi_stream::destroy_stream(const cdpi_id_dir &id_dir)
{
    m_info.erase(id_dir);
}
