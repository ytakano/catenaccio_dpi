#include "cdpi_tcp.hpp"

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <arpa/inet.h>

#include <boost/bind.hpp>

using namespace std;

cdpi_tcp::cdpi_tcp() : m_thread(boost::bind(&cdpi_tcp::run, this))
{

}

cdpi_tcp::~cdpi_tcp()
{

}

void
cdpi_tcp::input_tcp(cdpi_id &id, cdpi_direction dir, char *buf, int len)
{
    switch (id.get_l3_proto()) {
    case IPPROTO_IP:
    case IPPROTO_IPV4:
        input_tcp4(id, dir, buf, len);
        break;
    case IPPROTO_IPV6:
    default:
        break;
    }
}

void
cdpi_tcp::run()
{
    for (;;) {
        cdpi_tcp_event tcp_event;
        
        {
            boost::mutex::scoped_lock lock(m_mutex);
            while (m_events.size() == 0) {
                m_condition.wait(lock);
            }

            cdpi_tcp_event_cont::nth_index<1>::type &e1 = m_events.get<1>();

            // consume event
            tcp_event = e1.front();
            e1.pop_front();
        }

        bool ret;
        do {
            cdpi_tcp_packet packet;
            ret = get_packet(tcp_event.m_id, tcp_event.m_dir, packet);

            if (packet.m_flags & TH_SYN) {
                // TODO: event, connection opened
            } else if (packet.m_flags & TH_FIN &&
                       recv_fin(tcp_event.m_id, tcp_event.m_dir)) {
                // TODO: event, connection closed
            } else if (packet.m_flags & TH_RST) {
                // TODO: event, connection reset
                recv_rst(tcp_event.m_id, tcp_event.m_dir);
            } else {
                // TODO: event, data in
            }
        } while (ret);

        if (num_packets(tcp_event.m_id, tcp_event.m_dir) > MAX_PACKETS) {
            // TODO: event, strange connection
        }
    }
}

int
cdpi_tcp::num_packets(const cdpi_id &id, cdpi_direction dir)
{
    boost::mutex::scoped_lock lock(m_mutex);

    map<cdpi_id, ptr_cdpi_tcp_flow>::iterator it_flow;
    cdpi_tcp_uniflow *p_uniflow;

    it_flow = m_flow.find(id);
    if (it_flow == m_flow.end())
        return 0;

    if (dir == FROM_ADDR1)
        p_uniflow = &it_flow->second->m_flow1;
    else
        p_uniflow = &it_flow->second->m_flow2;

    return p_uniflow->m_packets.size();
}

bool
cdpi_tcp::recv_fin(const cdpi_id &id, cdpi_direction dir)
{
    boost::mutex::scoped_lock lock(m_mutex);

    map<cdpi_id, ptr_cdpi_tcp_flow>::iterator it_flow;
    cdpi_tcp_uniflow *peer;

    it_flow = m_flow.find(id);
    if (it_flow == m_flow.end())
        return false;

    if (dir == FROM_ADDR1)
        peer = &it_flow->second->m_flow2;
    else
        peer = &it_flow->second->m_flow1;

    if (peer->m_is_fin) {
        m_flow.erase(it_flow);
        return true;
    }

    return false;
}

void
cdpi_tcp::recv_rst(const cdpi_id &id, cdpi_direction dir)
{
    boost::mutex::scoped_lock lock(m_mutex);

    map<cdpi_id, ptr_cdpi_tcp_flow>::iterator it_flow;

    it_flow = m_flow.find(id);
    if (it_flow == m_flow.end())
        return;

    m_flow.erase(it_flow);
}

bool
cdpi_tcp::get_packet(const cdpi_id &id, cdpi_direction dir,
                     cdpi_tcp_packet &packet)
{
    boost::mutex::scoped_lock lock(m_mutex);

    map<cdpi_id, ptr_cdpi_tcp_flow>::iterator it_flow;
    cdpi_tcp_uniflow *p_uniflow;

    it_flow = m_flow.find(id);
    if (it_flow == m_flow.end())
        return false;

    if (dir == FROM_ADDR1)
        p_uniflow = &it_flow->second->m_flow1;
    else
        p_uniflow = &it_flow->second->m_flow2;

    
    map<uint32_t, cdpi_tcp_packet>::iterator it_pkt;

    it_pkt = p_uniflow->m_packets.find(p_uniflow->m_min_seq);
    if (it_pkt == p_uniflow->m_packets.end())
        return false;

    packet = it_pkt->second;

    p_uniflow->m_packets.erase(it_pkt);

    if (packet.m_flags & TH_SYN) {
        p_uniflow->m_min_seq++;
    } else if (packet.m_flags & TH_FIN) {
        p_uniflow->m_is_fin = true;
        recv_fin(id, dir);
    } else if (packet.m_flags & TH_RST) {
        recv_rst(id, dir);
    } else {
        p_uniflow->m_min_seq = packet.m_seq + packet.m_data_len;
    }

    return true;
}

void
cdpi_tcp::input_tcp4(cdpi_id &id, cdpi_direction dir, char *buf, int len)
{
    map<cdpi_id, ptr_cdpi_tcp_flow>::iterator it_flow;
    ptr_cdpi_tcp_flow p_tcp_flow;
    cdpi_tcp_packet   packet;
    ip     *iph;
    tcphdr *tcph;

    iph  = (ip*)buf;
    tcph = (tcphdr*)(buf + iph->ip_hl * 4);

    // TODO: checksum

    {
        boost::mutex::scoped_lock lock(m_mutex);

        it_flow = m_flow.find(id);

        if ((tcph->th_flags & TH_SYN) && it_flow == m_flow.end()) {
            p_tcp_flow = ptr_cdpi_tcp_flow(new cdpi_tcp_flow);
            m_flow[id] = p_tcp_flow;
        } else if (it_flow == m_flow.end()) {
            return;
        } else {
            p_tcp_flow = it_flow->second;
        }
    }

    packet.m_bytes.set_buf(buf, len);
    packet.m_seq      = ntohl(tcph->th_seq);
    packet.m_flags    = tcph->th_flags;
    packet.m_data_pos = iph->ip_hl * 4 + tcph->th_off * 4;
    packet.m_data_len = len - packet.m_data_pos;
    packet.m_nxt_seq  = packet.m_seq + packet.m_data_len;
    packet.m_read_pos = 0;

    {
        boost::mutex::scoped_lock lock(m_mutex);

        cdpi_tcp_uniflow *p_uniflow;
        
        if (dir == FROM_ADDR1) {
            p_uniflow = &p_tcp_flow->m_flow1;
        } else if (dir == FROM_ADDR2) {
            p_uniflow = &p_tcp_flow->m_flow2;
        } else {
            return;
        }

        if (packet.m_flags & TH_SYN) {
            p_uniflow->m_min_seq = packet.m_seq;
        } else if (! packet.m_flags & TH_RST &&
                   (int32_t)packet.m_seq - (int32_t)p_uniflow->m_min_seq < 0) {
            return;
        }

        if (packet.m_flags & TH_SYN || packet.m_flags & TH_FIN ||
            packet.m_flags & TH_RST || packet.m_data_len > 0) {
            p_uniflow->m_packets[packet.m_seq] = packet;
        }

        p_uniflow->m_time = time(NULL);


        // produce event
        cdpi_tcp_event tcp_event;

        tcp_event.m_id  = id;
        tcp_event.m_dir = dir;

        if (m_events.find(tcp_event) == m_events.end()) {
            m_events.insert(tcp_event);
        }

        m_condition.notify_one();
    }
}
