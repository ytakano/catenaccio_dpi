#include "cdpi_id.hpp"

#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

using namespace boost;

cdpi_direction
cdpi_id::set_iph(char *iph, int protocol)
{
    switch (protocol) {
    case IPPROTO_IP:
    case IPPROTO_IPV4:
    {
        shared_ptr<cdpi_peer> addr1(new cdpi_peer);
        shared_ptr<cdpi_peer> addr2(new cdpi_peer);
        ip *iph4 = (ip*)iph;

        memset(addr1.get(), 0, sizeof(cdpi_peer));
        memset(addr2.get(), 0, sizeof(cdpi_peer));

        addr1->l3_addr.b32 = iph4->ip_src.s_addr;
        addr2->l3_addr.b32 = iph4->ip_dst.s_addr;

        if (iph4->ip_p == IPPROTO_TCP) {
            tcphdr *tcph = (tcphdr*)(iph + iph4->ip_hl * 4);

            addr1->l4_port = tcph->th_sport;
            addr2->l4_port = tcph->th_dport;
        } else if (iph4->ip_p == IPPROTO_UDP) {
            udphdr *udph = (udphdr*)(iph + iph4->ip_hl * 4);

            addr1->l4_port = udph->uh_sport;
            addr2->l4_port = udph->uh_dport;
        }

        m_l3_proto = IPPROTO_IP;
        m_l4_proto = iph4->ip_p;

        if (*addr1 < *addr2) {
            m_addr1 = addr1;
            m_addr2 = addr2;

            return FROM_ADDR1;
        } else {
            m_addr1 = addr2;
            m_addr2 = addr1;

            return FROM_ADDR2;
        }
    }
    case IPPROTO_IPV6:
    default:
        break;
    }

    return FROM_NONE;
}
