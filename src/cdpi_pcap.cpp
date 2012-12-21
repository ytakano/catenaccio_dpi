#include "cdpi_pcap.hpp"
#include "cdpi_divert.hpp"

#include <unistd.h>

#include <net/ethernet.h>
#include <netinet/ip.h>

#include <iostream>

using namespace std;

boost::shared_ptr<cdpi_pcap> pcp;
bool m_is_running = false;


void
run_pcap(string dev, ptr_cdpi_event_listener listener)
{
    for (;;) {
        if (m_is_running) {
            stop_pcap();
            sleep(1);
        } else {
            break;
        }
    }

    m_is_running = true;

    pcp = boost::shared_ptr<cdpi_pcap>(new cdpi_pcap);
    boost::shared_ptr<cdpi_tcp> p_tcp(new cdpi_tcp);

    p_tcp->set_event_listener(listener);

    pcp->set_dev(dev);
    pcp->set_callback_ipv4(boost::shared_ptr<cdpi_callback>(new cb_ipv4(p_tcp)));
    pcp->run();

    m_is_running = false;
}

void
stop_pcap()
{
    pcp->stop();
}

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

    if (m_is_break) {
        pcap_breakloop(m_handle);
        return;
    }

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
    char errbuf[PCAP_ERRBUF_SIZE];

    if (m_dev == "") {
        char *dev = pcap_lookupdev(errbuf);
        if (dev == NULL) {
            cerr << "Couldn't find default device: " << errbuf << endl;
            return;
        }

        m_dev = dev;
    }

    cout << "start capturing " << m_dev << endl;

    m_handle = pcap_open_live(m_dev.c_str(), 1518, 1, 1000, errbuf);

    if (m_handle == NULL) {
        cerr << "Couldn't open device " << m_dev << ": " << errbuf << endl;
        return;
    }

    m_dl_type = pcap_datalink(m_handle);

    for (;;) {
        switch (pcap_dispatch(m_handle, -1, pcap_callback, (u_char*)this)) {
        case 0:
            if (m_is_break)
                return;
            break;
        case -1:
        {
            char *err = pcap_geterr(m_handle);
            cerr << "An error was encouterd while pcap_dispatch(): "
                 << err << endl;
            break;
        }
        case -2:
            return;
        }
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
