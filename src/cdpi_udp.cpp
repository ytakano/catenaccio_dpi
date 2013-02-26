#include "cdpi_udp.hpp"

#include "cdpi_bencode.hpp"

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <iostream>
#include <sstream>
#include <string>

using namespace std;

cdpi_udp::cdpi_udp() : m_is_del(false),
                       m_thread(boost::bind(&cdpi_udp::run, this))
{

}

cdpi_udp::~cdpi_udp()
{
    m_is_del = true;

    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_condition.notify_one();
    }
    m_thread.join();
}

void
cdpi_udp::run()
{
    for (;;) {
        {
            boost::mutex::scoped_lock lock(m_mutex);

            cdpi_udp_packet packet;
            int   len;
            char *data;

            while (m_queue.size() == 0) {
                m_condition.wait(lock);

                if (m_is_del)
                    return;
            }

            packet = m_queue.front();
            m_queue.pop();

            cout << "input UDP:" << endl;

            print_binary(packet.m_bytes.m_ptr.get(),
                         packet.m_bytes.m_len);

            cout << endl;
            // TODO: analyze data

            data = packet.m_bytes.m_ptr.get() + sizeof(udphdr);
            len  = packet.m_bytes.m_len - sizeof(udphdr);

            cdpi_bencode  bc;
            istringstream iss(string(data, len));
            istream      *is = &iss;

            if (bc.decode(*is)) {
                m_listener->in_datagram(CDPI_EVENT_BENCODE,
                                        packet.m_id_dir, &bc);
            }
        }
    }
}

void
cdpi_udp::input_udp(cdpi_id &id, cdpi_direction dir, char *buf, int len)
{
    switch (id.get_l3_proto()) {
    case IPPROTO_IP:
        input_udp4(id, dir, buf, len);
        break;
    case IPPROTO_IPV6:
        // TODO:
        break;
    default:
        break;
    }
}

void
cdpi_udp::input_udp4(cdpi_id &id, cdpi_direction dir, char *buf, int len)
{
    cdpi_udp_packet packet;
    ip     *iph;
    udphdr *udph;
    int     hlen;

    iph  = (ip*)buf;
    hlen = iph->ip_hl * 4;
    udph = (udphdr*)(buf + hlen);

    // TODO: checksum

    packet.m_id_dir.m_id  = id;
    packet.m_id_dir.m_dir = dir;
    packet.m_bytes.set_buf((char*)udph, len - hlen);

    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_queue.push(packet);

        m_condition.notify_one();
    }
}
