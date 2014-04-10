#include "cdpi_callback.hpp"

#include "cdpi_id.hpp"

#include <netinet/in.h>

cdpi_callback::cdpi_callback() : m_stream(new cdpi_stream),
                                 m_tcp(m_stream)
{

}

void
cdpi_callback::operator() (char *bytes, size_t len, uint8_t proto) {
    cdpi_direction dir;
    cdpi_id        id;
    char          *l4hdr;

    dir = id.set_iph(bytes, proto, &l4hdr);

    switch (id.get_l4_proto()) {
    case IPPROTO_TCP:
        m_tcp.input_tcp(id, dir, bytes, len, l4hdr);
        break;
    case IPPROTO_UDP:
        m_udp.input_udp(id, dir, bytes, len, l4hdr);
        break;
    default:
        ;
    }
}
