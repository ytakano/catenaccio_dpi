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

cdpi_udp::cdpi_udp() : m_is_dns_53(true),
                       m_is_del(false),
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

            data = packet.m_bytes.m_ptr.get() + sizeof(udphdr);
            len  = packet.m_bytes.m_len - sizeof(udphdr);

            {
                // try to read DNS when port is 53
                if ((m_is_dns_53 &&
                     (ntohs(packet.m_id_dir.get_port_src()) == 53 ||
                      ntohs(packet.m_id_dir.get_port_dst()) == 53)) ||
                    ! m_is_dns_53) {
                    ptr_cdpi_dns p_dns(new cdpi_dns);

                    if (p_dns->decode(data, len)) {
                        m_listener->in_datagram(CDPI_EVENT_DNS,
                                                packet.m_id_dir,
                                                boost::dynamic_pointer_cast<cdpi_proto>(p_dns));
                        continue;
                    }
                }
            }

            {
                // try to read bencode
                ptr_cdpi_bencode bc(new cdpi_bencode);
                istringstream    iss(string(data, len));
                istream         *is = &iss;

                if (bc->decode(*is)) {
                    m_listener->in_datagram(CDPI_EVENT_BENCODE,
                                            packet.m_id_dir, 
                                            boost::dynamic_pointer_cast<cdpi_proto>(bc));

                    continue;
                }
            }
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
