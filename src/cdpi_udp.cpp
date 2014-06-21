#include "cdpi_udp.hpp"

#include "cdpi_bencode.hpp"
#include "cdpi_dns.hpp"

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <iostream>
#include <sstream>
#include <string>

using namespace std;

cdpi_udp::cdpi_udp(ptr_cdpi_appif appif) :
    m_is_dns_53(true), m_is_del(false), m_appif(appif),
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

            while (m_queue.size() == 0) {
                m_condition.wait(lock);

                if (m_is_del)
                    return;
            }

            packet = m_queue.front();
            m_queue.pop();

            packet.m_bytes.skip(sizeof(udphdr));

            m_appif->in_event(STREAM_DATA, packet.m_id_dir, packet.m_bytes);
        }
    }
}

void
cdpi_udp::input_udp(cdpi_id &id, cdpi_direction dir, char *buf, int len,
                    char *l4hdr)
{
    cdpi_udp_packet packet;
    udphdr *udph;

    udph = (udphdr*)l4hdr;

    // TODO: checksum

    packet.m_id_dir.m_id  = id;
    packet.m_id_dir.m_dir = dir;
    packet.m_bytes.set_buf((char*)udph, len - (l4hdr - buf));

    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_queue.push(packet);

        m_condition.notify_one();
    }
}
