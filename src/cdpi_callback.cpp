#include "cdpi_callback.hpp"

#include "cdpi_id.hpp"

#include <netinet/in.h>


void
cdpi_callback::operator() (char *bytes, size_t len, uint8_t proto) {
    cdpi_direction dir;
    cdpi_id        id;
    char          *l4hdr;

    dir = id.set_iph(bytes, proto, &l4hdr);

    if (id.get_l3_proto() == IPPROTO_IPV6) {
        int i;

        i = 1 + 1;
    }

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
