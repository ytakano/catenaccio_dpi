#include "cdpi_appif.hpp"
#include "cdpi_conf.hpp"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <list>
#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>

using namespace std;
namespace fs = boost::filesystem;

void ux_read(int fd, short events, void *arg);

cdpi_appif::cdpi_appif() : m_home(new fs::path(fs::current_path()))
{

}

cdpi_appif::~cdpi_appif()
{

}

void
cdpi_appif::run()
{
    boost::mutex::scoped_lock lock(m_mutex);

    assert(! m_thread_listen);

    m_thread_listen = ptr_thread(new boost::thread(boost::bind(&cdpi_appif::ux_listen, this)));
}

void
ux_accept(int fd, short events, void *arg)
{
    int sock = accept(fd, NULL, NULL);
    cdpi_appif *appif = static_cast<cdpi_appif*>(arg);

    boost::mutex::scoped_lock lock(appif->m_mutex);

    auto it = appif->m_fd2ifrule.find(fd);
    if (it == appif->m_fd2ifrule.end()) {
        return;
    }

    event *ev = event_new(appif->m_ev_base, sock, EV_READ | EV_PERSIST,
                          ux_read, arg);
    event_add(ev, NULL);

    auto peer = cdpi_appif::ptr_uxpeer(new cdpi_appif::uxpeer);
    peer->m_fd   = sock;
    peer->m_ev   = ev;
    peer->m_name = it->second->m_name;

    appif->m_fd2uxpeer[sock] = peer;
    appif->m_name2uxpeer[it->second->m_name].insert(sock);
}

void
ux_read(int fd, short events, void *arg)
{
    cdpi_appif *appif = static_cast<cdpi_appif*>(arg);
    boost::mutex::scoped_lock lock(appif->m_mutex);

    char buf[4096];
    int  recv_size = read(fd, buf, sizeof(buf) - 1);

    if (recv_size > 0) {
        return;
    } else if (recv_size <= 0) {
        auto it1 = appif->m_fd2uxpeer.find(fd);
        if (it1 != appif->m_fd2uxpeer.end()) {
            auto it2 = appif->m_name2uxpeer.find(it1->second->m_name);
            if (it2 != appif->m_name2uxpeer.end()) {
                it2->second.erase(fd);

                if (it2->second.size() == 0) {
                    appif->m_name2uxpeer.erase(it2);
                }
            }

            event_del(it1->second->m_ev);
            event_free(it1->second->m_ev);

            appif->m_fd2uxpeer.erase(it1);
        }

        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
}

void
cdpi_appif::makedir(fs::path path)
{
    if (fs::exists(path)) {
        if (! fs::is_directory(path)) {
            cerr << path.string() << " is not directory" << endl;
            exit(-1);
        }
    } else {
        try {
            fs::create_directories(path);
        } catch (fs::filesystem_error e) {
            cerr << "cannot create directories: " << e.path1().string()
                 << endl;
            exit(-1);
        }
    }
}


void
cdpi_appif::ux_listen()
{
    int sock;

    m_ev_base = event_base_new();
    if (m_ev_base == NULL) {
        cerr << "cannot new ev_base" << endl;
        exit(-1);
    }

    {
        boost::mutex::scoped_lock lock(m_mutex);

        makedir(*m_home);
        makedir(*m_home / fs::path("tcp"));
        makedir(*m_home / fs::path("udp"));

        for (auto it = m_ifrule.begin(); it != m_ifrule.end(); ++it) {
            sock = socket(AF_UNIX, SOCK_STREAM, 0);

            if (sock == -1) {
                perror("socket");
                exit(-1);
            }
 
            struct sockaddr_un sa = {0};
            sa.sun_family = AF_UNIX;

            fs::path path;

            if ((*it)->m_proto == IF_UDP) {
                path = *m_home / fs::path("udp") / *(*it)->m_ux;
            } else if ((*it)->m_proto == IF_TCP) {
                path = *m_home / fs::path("tcp") / *(*it)->m_ux;
            } else {
                path = *m_home / *(*it)->m_ux;
            }

            strncpy(sa.sun_path, path.string().c_str(), sizeof(sa.sun_path));
 
            remove(sa.sun_path);
 
            if (::bind(sock, (struct sockaddr*) &sa,
                       sizeof(struct sockaddr_un)) == -1) {
                perror("bind");
                exit(-1);
            }

             if (listen(sock, 128) == -1) {
                perror("listen");
                exit(-1);
            }

             event *ev = event_new(m_ev_base, sock, EV_READ | EV_PERSIST,
                                   ux_accept, this);
             event_add(ev, NULL);

             m_fd2ifrule[sock] = *it;
        }
    }

    event_base_dispatch(m_ev_base);
}

void
cdpi_appif::read_conf(string conf)
{
    cdpi_conf c;

    if (! c.read_conf(conf))
        exit(-1);

    for (auto it1 = c.m_conf.begin(); it1 != c.m_conf.end(); ++it1) {
        if (it1->first == "global") {
            auto it2 = it1->second.find("home");
            if (it2 != it1->second.end()) {
                m_home = ptr_path(new fs::path(it2->second));
            }
        } else {
            ptr_ifrule rule = ptr_ifrule(new ifrule);

            rule->m_name = it1->first;
            rule->m_ux   = ptr_path(new fs::path(it1->first));

            auto it3 = it1->second.find("up");
            if (it3 != it1->second.end()) {
                rule->m_up = ptr_regex(new boost::regex(it3->second));
            }

            it3 = it1->second.find("down");
            if (it3 != it1->second.end()) {
                rule->m_down = ptr_regex(new boost::regex(it3->second));
            }

            it3 = it1->second.find("proto");
            if (it3 != it1->second.end()) {
                if (it3->second == "TCP") {
                    rule->m_proto = IF_TCP;
                } else if (it3->second == "UDP") {
                    rule->m_proto = IF_UDP;
                }
            }

            it3 = it1->second.find("format");
            if (it3 != it1->second.end()) {
                if (it3->second == "binary") {
                    rule->m_format = IF_BINARY;
                } else if (it3->second == "text") {
                    rule->m_format = IF_TEXT;
                }
            }

            it3 = it1->second.find("port");
            if (it3 != it1->second.end()) {
                stringstream ss(it3->second);

                while (ss) {
                    string port, n1, n2;
                    std::getline(ss, port, ',');

                    if (port.empty())
                        break;

                    port = trim(port);

                    stringstream ss2(port);
                    std::getline(ss2, n1, '-');
                    std::getline(ss2, n2);

                    n1 = trim(n1);
                    n2 = trim(n2);

                    pair<uint16_t, uint16_t> range;

                    try {
                        range.first = boost::lexical_cast<uint16_t>(n1);
                    } catch (boost::bad_lexical_cast e) {
                        cerr << "cannot convert \"" << n1
                             << "\" to uint16_t" << endl;
                        continue;
                    }

                    if (n2.size() > 0) {
                        try {
                            range.second = boost::lexical_cast<uint16_t>(n2);
                        } catch (boost::bad_lexical_cast e) {
                            cerr << "cannot convert \"" << n2
                                 << "\" to uint16_t" << endl;
                            continue;
                        }
                    } else {
                        range.second = range.first;
                    }

                    rule->m_port.push_back(range);
                }
            }

            m_ifrule.push_back(rule);
        }
    }
}

void
cdpi_appif::in_stream_event(cdpi_stream_event st_event,
                            const cdpi_id_dir &id_dir, cdpi_bytes bytes)
{
    switch (st_event) {
    case STREAM_SYN:
    {
        auto it = m_info.find(id_dir.m_id);

        if (it == m_info.end()) {
            ptr_info info = ptr_info(new stream_info);

            m_info[id_dir.m_id] = info;

            it = m_info.find(id_dir.m_id);
        }

        break;
    }
    case STREAM_DATA:
    {
        if (bytes.get_len() <= 0)
            return;

        auto it = m_info.find(id_dir.m_id);

        if (it == m_info.end())
            return;

        if (it->second->m_is_giveup)
            return;

        if (id_dir.m_dir == FROM_ADDR1) {
            it->second->m_buf1.push_back(bytes);
            it->second->m_dsize1 += bytes.get_len();
            it->second->m_is_buf1 = true;
        } else if (id_dir.m_dir == FROM_ADDR2) {
            it->second->m_buf2.push_back(bytes);
            it->second->m_dsize2 += bytes.get_len();
            it->second->m_is_buf2 = true;
        } else {
            return;
        }

        send_data(it->second, id_dir);

        break;
    }
    case STREAM_DESTROYED:
    {
        auto it = m_info.find(id_dir.m_id);

        if (it == m_info.end())
            return;

        it->second->m_is_buf1 = true;
        it->second->m_is_buf2 = true;

        send_data(it->second, id_dir);

        if (it->second->m_ifrule) {
            // invoke DESTROYED event
            boost::mutex::scoped_lock lock(m_mutex);

            auto it2 = m_name2uxpeer.find(it->second->m_ifrule->m_name);

            if (it2 != m_name2uxpeer.end()) {
                for (auto it3 = it2->second.begin(); it3 != it2->second.end();
                     ++it3) {
                    write_head(*it3, id_dir, it->second->m_ifrule->m_format,
                               STREAM_DESTROYED);
                }
            }
        }

        m_info.erase(it);

        break;
    }
    case STREAM_FIN:
    case STREAM_TIMEOUT:
    case STREAM_RST:
        // nothing to do
        break;
    default:
        assert(st_event != STREAM_CREATED);
    }
}

void
cdpi_appif::send_data(ptr_info p_info, cdpi_id_dir id_dir)
{
    if (! p_info->m_ifrule) {
        // classify
        if (p_info->m_is_buf1 && p_info->m_is_buf2) {
            for (auto it2 = m_ifrule.begin(); it2 != m_ifrule.end();
                 ++it2) {

                if ((*it2)->m_proto != IF_TCP)
                    continue;

                bool is_port = false;
                for (auto it3 = (*it2)->m_port.begin();
                     it3 != (*it2)->m_port.end(); ++it3) {

                    if ((it3->first <= ntohs(id_dir.get_port_src()) &&
                         ntohs(id_dir.get_port_src()) <= it3->second) ||
                        (it3->first <= ntohs(id_dir.get_port_dst()) &&
                         ntohs(id_dir.get_port_dst()) <= it3->second)) {
                        is_port = true;
                    }
                }

                if (! is_port)
                    continue;

                if ((*it2)->m_up && (*it2)->m_down) {
                    char buf1[4096], buf2[4096];
                    int  len1, len2;

                    len1 = read_bytes(p_info->m_buf1, buf1,
                                      sizeof(buf1));
                    string s1(buf1, len1);

                    len2 = read_bytes(p_info->m_buf2,
                                      buf2, sizeof(buf2));
                    string s2(buf2, len2);

                    if (boost::regex_match(s1, *(*it2)->m_up) &&
                        boost::regex_match(s2, *(*it2)->m_down)) {
                        p_info->m_buf1_dir = MATCH_UP;
                        p_info->m_buf2_dir = MATCH_DOWN;
                    } else if (boost::regex_match(s2, *(*it2)->m_up) &&
                               boost::regex_match(s1, *(*it2)->m_down)) {
                        p_info->m_buf1_dir = MATCH_DOWN;
                        p_info->m_buf2_dir = MATCH_UP;
                    } else {
                        continue;
                    }
                }

                // matched rule
                p_info->m_ifrule = *it2;

                // invoke CREATED event
                {
                    boost::mutex::scoped_lock lock(m_mutex);

                    auto it4 = m_name2uxpeer.find((*it2)->m_name);

                    if (it4 != m_name2uxpeer.end()) {
                        for (auto it5 = it4->second.begin();
                             it5 != it4->second.end(); ++it5) {
                            write_head(*it5, id_dir, p_info->m_ifrule->m_format,
                                       STREAM_CREATED);
                        }
                    }
                }
                break;
            }
        } else {
            return;
        }
    }

    if (p_info->m_ifrule) {
        // invoke DATA event and send data to I/F
        deque<cdpi_bytes> *bufs;

        if (id_dir.m_dir == FROM_ADDR1) {
            bufs = &p_info->m_buf1;
        } else {
            bufs = &p_info->m_buf2;
        }

        while (! bufs->empty()) {
            boost::mutex::scoped_lock lock(m_mutex);

            auto front = bufs->front();
            auto it2 = m_name2uxpeer.find(p_info->m_ifrule->m_name);

            if (it2 != m_name2uxpeer.end()) {
                for (auto it3 = it2->second.begin(); it3 != it2->second.end();
                     ++it3) {
                    write_head(*it3, id_dir, p_info->m_ifrule->m_format,
                               STREAM_DATA, front.get_len());

                    // write data
                    write(*it3, front.get_head(), front.get_len());
                }
            }

            bufs->pop_front();
        }

    } else {
        // give up?
        if (p_info->m_dsize1 > 4096 && p_info->m_dsize2 > 4096) {
            p_info->m_is_giveup = true;
            p_info->m_buf1.clear();
            p_info->m_buf2.clear();
            return;
        }
    }
}

void
cdpi_appif::write_head(int fd, const cdpi_id_dir &id_dir, ifformat format,
                       cdpi_stream_event event, int bodylen)
{
    if (format == IF_TEXT) {
        string s;
        char buf[256];

        s  = "ip1=";
        id_dir.get_addr1(buf, sizeof(buf));
        s += buf;

        s += ",ip2=";
        id_dir.get_addr2(buf, sizeof(buf));
        s += buf;

        s += ",port1=";
        s += boost::lexical_cast<string>(id_dir.get_port1());

        s += ",port2=";
        s += boost::lexical_cast<string>(id_dir.get_port2());

        s += ",hop=";
        s += boost::lexical_cast<string>((int)id_dir.m_id.m_hop);

        switch (event) {
        case STREAM_CREATED:
            s += ",event=CREATED\n";
            break;
        case STREAM_DESTROYED:
            s += ",event=DESTROYED\n";
            break;
        case STREAM_DATA:
            s += ",event=DATA,from=";

            if (id_dir.m_dir == FROM_ADDR1) {
                s += "1";
            } else {
                s += "2";
            }

            s += ",len=";
            s += boost::lexical_cast<string>(bodylen);
            s += "\n";
            break;
        default:
            assert(false);
        }

        write(fd, s.c_str(), s.size());
    } else {
        appif_header head;

        memset(&head, 0, sizeof(head));

        memcpy(&head.l3_addr1, &id_dir.m_id.m_addr1->l3_addr,
               sizeof(head.l3_addr1));
        memcpy(&head.l3_addr2, &id_dir.m_id.m_addr2->l3_addr,
               sizeof(head.l3_addr2));

        head.l4_port1 = id_dir.m_id.m_addr1->l4_port;
        head.l4_port2 = id_dir.m_id.m_addr2->l4_port;
        head.event    = event;
        head.from     = id_dir.m_dir;
        head.hop      = id_dir.m_id.m_hop;
        head.l3_proto = id_dir.m_id.get_l3_proto();
        head.len      = bodylen;

        write(fd, &head, sizeof(head));
    }
}
