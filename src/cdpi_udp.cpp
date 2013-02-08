#include "cdpi_udp.hpp"

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

cdpi_udp::cdpi_udp() : m_thread(boost::bind(&cdpi_udp::run, this))
{

}

cdpi_udp::~cdpi_udp()
{
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

            cdpi_udp_data udp_data;

            udp_data = m_queue.front();

            // TODO: analyze data
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
    cdpi_udp_data data;
    ip     *iph;
    udphdr *udph;
    int     hlen;

    iph  = (ip*)buf;
    hlen = iph->ip_hl * 4;
    udph = (udphdr*)(buf + hlen);

    // TODO: checksum

    data.m_id_dir.m_id  = id;
    data.m_id_dir.m_dir = dir;
    data.m_bytes.set_buf((char*)udph, len - hlen);

    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_queue.push(data);

        m_condition.notify_one();
    }
}
