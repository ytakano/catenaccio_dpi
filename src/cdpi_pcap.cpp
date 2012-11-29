#include "cdpi_pcap.hpp"

#include <pcap/pcap.h>

#include <net/ethernet.h>
#include <netinet/ip.h>

#include <iostream>

using namespace std;

void
pcap_callback(uint8_t *user, const struct pcap_pkthdr *h, const uint8_t *bytes)
{
    cdpi_pcap *pcap = (cdpi_pcap*)user;

    pcap->callback(h, bytes);
}

void
cdpi_pcap::callback(const struct pcap_pkthdr *h, const uint8_t *bytes)
{
    uint8_t proto;
    const uint8_t *ip_hdr = get_ip_hdr(bytes, h->caplen, proto);

    if (ip_hdr == NULL)
        return;

    switch (proto) {
    case IPPROTO_IP:{
        ip *iph = (ip*)ip_hdr;
        uint16_t off = ntohs(iph->ip_off);

        if (off & IP_MF || (off & 0x1fff) > 0)
            return;

        if (m_callback_ipv4)
            (*m_callback_ipv4)((char*)ip_hdr, ntohs(iph->ip_len));

        break;
    }
    case IPPROTO_IPV6:
        // TODO:
    default:
        break;
    }
}

void
cdpi_pcap::set_dev(std::string dev)
{
    m_dev = dev;
}

void
cdpi_pcap::run()
{
    pcap_t *handle;
    char errbuf[PCAP_ERRBUF_SIZE];

    if (m_dev == "") {
        char *dev = pcap_lookupdev(errbuf);
        if (dev == NULL) {
            cerr << "Couldn't find default device: " << errbuf << endl;
            return;
        }

        m_dev = dev;
    }

    cout << "start caputring " << m_dev << endl;

    handle = pcap_open_live(m_dev.c_str(), 1518, 1, 1000, errbuf);

    if (handle == NULL) {
        cerr << "Couldn't open device " << m_dev << ": " << errbuf << endl;
        return;
    }

    m_dl_type = pcap_datalink(handle);

    switch (pcap_loop(handle, -1, pcap_callback, (u_char*)this)) {
    case 0:
        break;
    case -1:
        cerr << "An error was encouterd while pcap_loop()" << endl;
        break;
    case -2:
        break;
    }
}

const uint8_t *
cdpi_pcap::get_ip_hdr(const uint8_t *bytes, uint32_t len, uint8_t &proto)
{
    const uint8_t *ip_hdr = NULL;

    switch (m_dl_type) {
    case DLT_EN10MB: {
        if (len < sizeof(ether_header))
            break;

        const ether_header *ehdr = (const ether_header*)bytes;


        switch (ntohs(ehdr->ether_type)) {
        case ETHERTYPE_IP:
            proto = IPPROTO_IP;
            ip_hdr = bytes + sizeof(ether_header);
            break;
        case ETHERTYPE_IPV6:
            proto = IPPROTO_IPV6;
            ip_hdr = bytes + sizeof(ether_header);
            break;
        default:
            break;
        }

        break;
    }
    case DLT_IEEE802_11:
        // TODO
    default:
        break;
    }

    return ip_hdr;
}
