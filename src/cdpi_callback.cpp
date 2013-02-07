#include "cdpi_callback.hpp"

#include "cdpi_id.hpp"

#include <netinet/in.h>


void
cdpi_callback::operator() (char *bytes, size_t len, uint8_t proto) {
    cdpi_direction dir;
    cdpi_id        id;

    dir = id.set_iph(bytes, proto);

    switch (id.get_l4_proto()) {
    case IPPROTO_TCP:
        m_tcp.input_tcp(id, dir, bytes, len);
        break;
    case IPPROTO_UDP:
        break;
    default:
        ;
    }
}
