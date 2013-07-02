#include "cdpi_tcp.hpp"

#include <unistd.h>

#include <sys/socket.h>

#ifdef __linux__
    #define __FAVOR_BSD
#endif

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <arpa/inet.h>

#include <boost/bind.hpp>

using namespace std;

#define TCP_GC_TIMER 120

cdpi_tcp::cdpi_tcp() : m_is_del(false),
                       m_thread_run(boost::bind(&cdpi_tcp::run, this)),
                       m_thread_gc(boost::bind(&cdpi_tcp::garbage_collector,
                                               this))
{

}

cdpi_tcp::~cdpi_tcp()
{
    m_is_del = true;

    {
        boost::mutex::scoped_lock lock(m_mutex);
        m_condition.notify_one();
    }
    m_thread_run.join();

    {
        boost::mutex::scoped_lock lock(m_mutex_gc);
        m_condition_gc.notify_one();
    }
    m_thread_gc.join();
}

void
cdpi_tcp::garbage_collector()
{
    for (;;) {

        boost::mutex::scoped_lock lock_gc(m_mutex_gc);
        m_condition_gc.timed_wait(lock_gc, boost::posix_time::milliseconds(TCP_GC_TIMER * 1000));

        if (m_is_del) {
            return;
        }

        {
            boost::mutex::scoped_lock lock(m_mutex);

            std::map<cdpi_id, ptr_cdpi_tcp_flow>::iterator it;

            // close half opened connections
            for (it = m_flow.begin(); it != m_flow.end(); ++it) {
                if (((it->second->m_flow1.m_is_syn &&
                      ! it->second->m_flow2.m_is_syn) ||
                     (it->second->m_flow1.m_is_fin &&
                       ! it->second->m_flow2.m_is_fin)) &&
                    time(NULL) - it->second->m_flow1.m_time > TCP_GC_TIMER) {

                    it->second->m_flow1.m_is_rm = true;

                    cdpi_id_dir id_dir;

                    id_dir.m_id  = it->first;
                    id_dir.m_dir = FROM_ADDR1;

                    m_events.insert(id_dir);
                } else if (((! it->second->m_flow1.m_is_syn &&
                             it->second->m_flow2.m_is_syn) ||
                            (! it->second->m_flow1.m_is_fin &&
                            it->second->m_flow2.m_is_fin)) &&
                           time(NULL) - it->second->m_flow2.m_time > TCP_GC_TIMER) {

                    it->second->m_flow2.m_is_rm = true;

                    cdpi_id_dir id_dir;

                    id_dir.m_id  = it->first;
                    id_dir.m_dir = FROM_ADDR2;

                    m_events.insert(id_dir);
                }
            }
        }
    }
}

void
cdpi_tcp::run()
{
    for (;;) {
        cdpi_id_dir tcp_event;
#ifdef DEBUG
        char addr1[32], addr2[32];
#endif // DEBUG
        
        {
            boost::mutex::scoped_lock lock(m_mutex);
            while (m_events.size() == 0) {
                m_condition.wait(lock);

                if (m_is_del)
                    return;
            }

            cdpi_id_dir_cont::nth_index<1>::type &e1 = m_events.get<1>();

            // consume event
            tcp_event = e1.front();
            e1.pop_front();

#ifdef DEBUG
            inet_ntop(PF_INET, &tcp_event.m_id.m_addr1->l3_addr.b32,
                      addr1, sizeof(addr1));
            inet_ntop(PF_INET, &tcp_event.m_id.m_addr2->l3_addr.b32,
                      addr2, sizeof(addr2));
#endif // DEBUG


            // garbage collection
            std::map<cdpi_id, ptr_cdpi_tcp_flow>::iterator it_flow;

            it_flow = m_flow.find(tcp_event.m_id);

            if (it_flow == m_flow.end()) {
                continue;
            }

            if ((tcp_event.m_dir == FROM_ADDR1 &&
                 it_flow->second->m_flow1.m_is_rm) ||
                (tcp_event.m_dir == FROM_ADDR2 &&
                 it_flow->second->m_flow2.m_is_rm)) {

#ifdef DEBUG
                cout << "garbage collected: addr1 = "
                     << addr1 << ":"
                     << ntohs(tcp_event.m_id.m_addr1->l4_port)
                     << ", addr2 = "
                     << addr2 << ":"
                     << ntohs(tcp_event.m_id.m_addr2->l4_port)
                     << endl;
#endif // DEBUG

                cdpi_bytes bytes;

                tcp_event.m_dir = FROM_ADDR1;
                m_stream.in_stream_event(STREAM_ERROR, tcp_event, bytes);

                tcp_event.m_dir = FROM_ADDR2;
                m_stream.in_stream_event(STREAM_ERROR, tcp_event, bytes);

                lock.unlock();
                rm_flow(tcp_event.m_id, tcp_event.m_dir);
            }
        }

        cdpi_tcp_packet packet;

        while (get_packet(tcp_event.m_id, tcp_event.m_dir, packet)) {
            if (packet.m_flags & TH_SYN) {
#ifdef DEBUG
                cout << "connection opened: addr1 = "
                     << addr1 << ":"
                     << ntohs(tcp_event.m_id.m_addr1->l4_port)
                     << ", addr2 = "
                     << addr2 << ":"
                     << ntohs(tcp_event.m_id.m_addr2->l4_port)
                     << ", from = " << tcp_event.m_dir
                     << endl;
#endif // DEBUG

                cdpi_bytes bytes;
                m_stream.in_stream_event(STREAM_CREATED, tcp_event, bytes);
            } else if (packet.m_flags & TH_FIN) {
                cdpi_bytes bytes;

                m_stream.in_stream_event(STREAM_DATA_IN, tcp_event, bytes);

#ifdef DEBUG
                cout << "connection closed: addr1 = "
                     << addr1 << ":"
                     << ntohs(tcp_event.m_id.m_addr1->l4_port)
                     << ", addr2 = "
                     << addr2 << ":"
                     << ntohs(tcp_event.m_id.m_addr2->l4_port)
                     << ", from = " << tcp_event.m_dir
                     << endl;
#endif // DEBUG

                if (recv_fin(tcp_event.m_id, tcp_event.m_dir)) {
                    tcp_event.m_dir = FROM_ADDR1;
                    m_stream.in_stream_event(STREAM_DESTROYED, tcp_event, bytes);

                    tcp_event.m_dir = FROM_ADDR2;
                    m_stream.in_stream_event(STREAM_DESTROYED, tcp_event, bytes);
                }
            } else if (packet.m_flags & TH_RST) {
#ifdef DEBUG
                cout << "connection reset: addr1 = "
                     << addr1 << ":"
                     << ntohs(tcp_event.m_id.m_addr1->l4_port)
                     << ", addr2 = "
                     << addr2 << ":"
                     << ntohs(tcp_event.m_id.m_addr2->l4_port)
                     << endl;
#endif // DEBUG

                cdpi_bytes bytes;

                rm_flow(tcp_event.m_id, tcp_event.m_dir);

                tcp_event.m_dir = FROM_ADDR1;
                m_stream.in_stream_event(STREAM_DESTROYED, tcp_event, bytes);

                tcp_event.m_dir = FROM_ADDR2;
                m_stream.in_stream_event(STREAM_DESTROYED, tcp_event, bytes);
            } else {
                packet.m_bytes.m_len = packet.m_data_len;
                packet.m_bytes.m_pos = packet.m_data_pos;

                m_stream.in_stream_event(STREAM_DATA_IN, tcp_event,
                                         packet.m_bytes);
            }
        }

        if (num_packets(tcp_event.m_id, tcp_event.m_dir) > MAX_PACKETS) {
#ifdef DEBUG
            cout << "connection error(packets limit exceeded): addr1 = "
                 << addr1 << ":"
                 << ntohs(tcp_event.m_id.m_addr1->l4_port)
                 << ", addr2 = "
                 << addr2 << ":"
                 << ntohs(tcp_event.m_id.m_addr2->l4_port)
                 << endl;
#endif // DEBUG

            cdpi_bytes bytes;

            tcp_event.m_dir = FROM_ADDR1;
            m_stream.in_stream_event(STREAM_ERROR, tcp_event, bytes);

            tcp_event.m_dir = FROM_ADDR2;
            m_stream.in_stream_event(STREAM_ERROR, tcp_event, bytes);

            rm_flow(tcp_event.m_id, tcp_event.m_dir);
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
cdpi_tcp::rm_flow(const cdpi_id &id, cdpi_direction dir)
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
    if (it_pkt == p_uniflow->m_packets.end()) {
        return false;
    }

    packet = it_pkt->second;

    p_uniflow->m_packets.erase(it_pkt);

    if (packet.m_flags & TH_FIN) {
        p_uniflow->m_is_fin = true;
    }

    p_uniflow->m_min_seq = packet.m_nxt_seq;

    return true;
}

void
cdpi_tcp::input_tcp(cdpi_id &id, cdpi_direction dir, char *buf, int len,
                    char *l4hdr)
{
    map<cdpi_id, ptr_cdpi_tcp_flow>::iterator it_flow;
    ptr_cdpi_tcp_flow p_tcp_flow;
    cdpi_tcp_packet   packet;
    tcphdr *tcph;

    tcph = (tcphdr*)l4hdr;

/*
#ifdef DEBUG
    cout << "TCP flags: ";
    if (tcph->th_flags & TH_SYN)
        cout << "S";
    if (tcph->th_flags & TH_RST)
        cout << "R";
    if (tcph->th_flags & TH_ACK)
        cout << "A";
    if (tcph->th_flags & TH_FIN)
        cout << "F";
    cout << endl;
#endif
*/

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
    packet.m_data_pos = l4hdr - buf + tcph->th_off * 4;
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
            if (! p_uniflow->m_is_syn) {
                p_uniflow->m_min_seq = packet.m_seq;
                p_uniflow->m_is_syn  = true;
                packet.m_nxt_seq = packet.m_seq + 1;
            } else {
                return;
            }
        } else if (! packet.m_flags & TH_RST &&
                   (int32_t)packet.m_seq - (int32_t)p_uniflow->m_min_seq < 0) {
            return;
        }

        if (packet.m_flags & TH_SYN || packet.m_flags & TH_FIN ||
            packet.m_data_len > 0) {
            p_uniflow->m_packets[packet.m_seq] = packet;
        } else if (packet.m_flags & TH_RST) {
            p_uniflow->m_packets[p_uniflow->m_min_seq] = packet;
        }

        p_uniflow->m_time = time(NULL);


        // produce event
        cdpi_id_dir tcp_event;

        tcp_event.m_id  = id;
        tcp_event.m_dir = dir;

        if (m_events.find(tcp_event) == m_events.end()) {
            m_events.insert(tcp_event);
        }

        m_condition.notify_one();
    }
}
