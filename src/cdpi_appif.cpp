#include "cdpi_appif.hpp"
#include "cdpi_conf.hpp"
#include "cdpi_callback.hpp"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

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

// #include <event2/thread.h>

using namespace std;
namespace fs = boost::filesystem;

void ux_read(int fd, short events, void *arg);
bool read_loopback7(int fd, cdpi_appif *appif);
bool read_loopback3(int fd, cdpi_appif *appif);

cdpi_appif::cdpi_appif(cdpi_callback &callback, cdpi_tcp &tcp) :
    m_fd7(-1),
    m_fd3(-1),
    m_lb7_format(IF_BINARY),
    m_callback(callback),
    m_tcp(tcp),
    m_home(new fs::path(fs::current_path())),
    m_is_lru(true)
{

}

cdpi_appif::~cdpi_appif()
{

}

void
cdpi_appif::run()
{
    boost::upgrade_lock<boost::shared_mutex> up_lock(m_rw_mutex);
    boost::upgrade_to_unique_lock<boost::shared_mutex> wlock(up_lock);

    assert(! m_thread_listen);

    m_thread_listen = ptr_thread(new boost::thread(boost::bind(&cdpi_appif::ux_listen, this)));
    m_thread_stream = ptr_thread(new boost::thread(boost::bind(&cdpi_appif::consume_event, this)));
}

void
ux_accept(int fd, short events, void *arg)
{
    int sock = accept(fd, NULL, NULL);
    cdpi_appif *appif = static_cast<cdpi_appif*>(arg);

    boost::upgrade_lock<boost::shared_mutex> up_lock(appif->m_rw_mutex);
    boost::upgrade_to_unique_lock<boost::shared_mutex> wlock(up_lock);

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

    cout << "accepted on " << it->second->m_name
         << " (fd = " << sock << ")" << endl;

    if (fd == appif->m_fd7) {
        appif->m_lb7_state[sock] = cdpi_appif::ptr_loopback_state(new cdpi_appif::loopback_state);
    }
}

void
ux_close(int fd, cdpi_appif *appif)
{
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

        cout << "closed on " << it1->second->m_name
             << " (fd = " << fd << ")" << endl;


        appif->m_fd2uxpeer.erase(it1);
    }

    shutdown(fd, SHUT_RDWR);
    close(fd);

    auto it3 = appif->m_lb7_state.find(fd);
    if (it3 != appif->m_lb7_state.end()) {
        cdpi_id_dir id_dir;
        cdpi_bytes  bytes;

        for (auto it4 = it3->second->streams.begin();
             it4 != it3->second->streams.end(); ++it4) {
            id_dir.m_id  = *it4;
            id_dir.m_dir = FROM_NONE;

            appif->in_event(STREAM_DESTROYED, id_dir, bytes);
        }

        appif->m_lb7_state.erase(it3);
    }
}

void
ux_read(int fd, short events, void *arg)
{
    cdpi_appif *appif = static_cast<cdpi_appif*>(arg);
    boost::upgrade_lock<boost::shared_mutex> up_lock(appif->m_rw_mutex);

    auto it1 = appif->m_fd2uxpeer.find(fd);
    if (it1 != appif->m_fd2uxpeer.end()) {
        // read from loopback interface
        if (it1->second->m_name == "loopback7") {
            if (read_loopback7(fd, appif)) {
                boost::upgrade_to_unique_lock<boost::shared_mutex> wlock(up_lock);
                ux_close(fd, appif);
            }
            return;
        } else if (it1->second->m_name == "loopback3") {
            if (read_loopback3(fd, appif)) {
                boost::upgrade_to_unique_lock<boost::shared_mutex> wlock(up_lock);
                ux_close(fd, appif);
            }
        } else {
            char buf[4096];
            int  recv_size = read(fd, buf, sizeof(buf) - 1);

            if (recv_size <= 0) {
                boost::upgrade_to_unique_lock<boost::shared_mutex> wlock(up_lock);
                ux_close(fd, appif);
            }
        }
    }
}

bool
read_loopback3(int fd, cdpi_appif *appif)
{
    char    buf[8192];
    ssize_t len = read(fd, buf, sizeof(buf));

    if (len <= 0)
        return true;

    if ((buf[0] & 0xf0) == 0x40) {
        appif->m_callback(buf, len, IPPROTO_IP);
    } else if ((buf[0] & 0xf0) == 0x60) {
        appif->m_callback(buf, len, IPPROTO_IPV6);
    }

    return false;
}

bool
read_loopback7(int fd, cdpi_appif *appif)
{
    cdpi_appif_header *header;
    cdpi_id_dir        id_dir;

    auto it = appif->m_lb7_state.find(fd);
    assert(it != appif->m_lb7_state.end());

    header = &it->second->header;

    if (it->second->is_header) {
        if (appif->m_lb7_format == cdpi_appif::IF_BINARY) {
            // read binary header
            ssize_t len = read(fd, header, sizeof(*header));

            if (len <= 0) {
                // must close fd
                return true;
            } else if (len != sizeof(*header)) {
                cerr << "CAUTION! LOOPBACK 7 RECEIVED INVALID HEADER!: socket = "
                     << fd << endl;
                return false;
            }

            header->hop++;
        } else {
            // read text header
            map<string, string> h;
            string s;

            for (;;) {
                char c;
                ssize_t len = read(fd, &c, 1);

                if (len <= 0) {
                    // must close fd
                    return true;
                }

                if (c == '\n')
                    break;

                s += c;
            }

            stringstream ss1(s);
            while (ss1) {
                string elm;
                std::getline(ss1, elm, ',');

                stringstream ss2(elm);
                string key, val;
                std::getline(ss2, key, '=');
                std::getline(ss2, val);

                h[key] = val;
            }

            uint8_t l3_proto, l4_proto;

            if (h["l3"] == "ipv4") {
                l3_proto = IPPROTO_IP;
            } else if (h["l3"] == "ipv6") {
                l3_proto = IPPROTO_IPV6;
            } else {
                return false;
            }

            if (h["l4"] == "tcp") {
                l4_proto = IPPROTO_TCP;
            } else if (h["l4"] == "udp") {
                l4_proto = IPPROTO_UDP;
            } else {
                return false;
            }

            if (inet_pton(l3_proto, h["ip1"].c_str(), &header->l3_addr1) <= 0) {
                cerr << "CAUTION! LOOPBACK 7 RECEIVED INVALID HEADER!: header = "
                     << s << endl;
                return false;
            }

            if (inet_pton(l3_proto, h["ip2"].c_str(), &header->l3_addr2) <= 0) {
                cerr << "CAUTION! LOOPBACK 7 RECEIVED INVALID HEADER!: header = "
                     << s << endl;
                return false;
            }

            try {
                header->l4_port1 = boost::lexical_cast<int>(h["port1"]);
                header->l4_port2 = boost::lexical_cast<int>(h["port2"]);
                header->hop      = boost::lexical_cast<int>(h["hop"]);

                auto it_len = h.find("len");
                if (it_len != h.end()) {
                    header->len = boost::lexical_cast<int>(it->second);
                }
            } catch (boost::bad_lexical_cast e) {
                cerr << "CAUTION! LOOPBACK 7 RECEIVED INVALID HEADER!: header = "
                     << s << endl;
                return false;
            }

            header->hop++;

            if (h["event"] == "CREATED") {
                header->event = STREAM_CREATED;
            } else if (h["event"] == "DESTROYED") {
                header->event = STREAM_DESTROYED;
            } else if (h["event"] == "DATA") {
                header->event = STREAM_DATA;
            } else {
                return false;
            }

            if (h["from"] == "0") {
                header->from = FROM_ADDR1;
            } else if (h["from"] == "1") {
                header->from = FROM_ADDR2;
            } else {
                header->from = FROM_NONE;
            }
        }

        header->match = cdpi_appif::MATCH_NONE;

        id_dir.m_id.set_appif_header(*header);

        if (header->from == 0) {
            id_dir.m_dir = FROM_ADDR1;
        } else if (header->from == 1) {
            id_dir.m_dir = FROM_ADDR2;
        } else {
            id_dir.m_dir = FROM_NONE;
        }

        it->second->id_dir = id_dir;


        if (header->event == STREAM_DATA) {
            it->second->is_header = true;
            return false;
        } else if (header->event == STREAM_CREATED) {
            cdpi_bytes bytes;

            // invoke CREATED event
            appif->in_event(STREAM_CREATED, id_dir, bytes);

            it->second->streams.insert(id_dir.m_id);

            return false;
        } else if (header->event == STREAM_DESTROYED) {
            cdpi_bytes bytes;

            // invoke DESTROYED event
            appif->in_event(STREAM_DESTROYED, id_dir, bytes);

            it->second->streams.erase(id_dir.m_id);

            return false;
        } else {
            cerr << "CAUTION! LOOPBACK 7 RECEIVED INVALID EVENT!: event = "
                 << header->event << endl;
            return false;
        }
    } else {
        cdpi_bytes bytes;

        bytes.alloc(header->len);

        ssize_t len = read(fd, bytes.get_head(), header->len);

        if (len <= 0) {
            // must close fd
            return true;
        } else if (len != header->len) {
            cerr << "CAUTION! LOOPBACK 7 RECEIVED INVALID BODY LENGTH!: len = "
                 << header->len << endl;
            return false;
        }

        // invoke DATA event
        appif->in_event(STREAM_DATA, it->second->id_dir, bytes);

        return false;
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
cdpi_appif::ux_listen_ifrule(ptr_ifrule ifrule)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock == -1) {
        perror("socket");
        exit(-1);
    }
 
    struct sockaddr_un sa = {0};
    sa.sun_family = AF_UNIX;

    fs::path path;

    if (ifrule->m_proto == IF_UDP) {
        path = *m_home / fs::path("udp") / *ifrule->m_ux;
    } else if (ifrule->m_proto == IF_TCP) {
        path = *m_home / fs::path("tcp") / *ifrule->m_ux;
    } else {
        path = *m_home / *ifrule->m_ux;
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

    cout << "listening on " << path << " (" << ifrule->m_name << ")" << endl;

    event *ev = event_new(m_ev_base, sock, EV_READ | EV_PERSIST,
                          ux_accept, this);
    event_add(ev, NULL);

    m_fd2ifrule[sock] = ifrule;


    if (ifrule->m_name == "loopback7") {
        m_fd7 = sock;
    } else if (ifrule->m_name == "loopback3") {
        m_fd3 = sock;
    }
}


void
cdpi_appif::ux_listen()
{
    m_ev_base = event_base_new();
    if (m_ev_base == NULL) {
        cerr << "couldn't new ev_base" << endl;
        exit(-1);
    }

/*
    evthread_use_pthreads();
    if (evthread_make_base_notifiable(m_ev_base) < 0) {
        cerr << "couldn't make base notifiable " << endl;
        exit(-1);
    }
*/

    {
        boost::upgrade_lock<boost::shared_mutex> up_lock(m_rw_mutex);
        boost::upgrade_to_unique_lock<boost::shared_mutex> wlock(up_lock);

        makedir(*m_home);
        makedir(*m_home / fs::path("tcp"));
        makedir(*m_home / fs::path("udp"));

        for (auto it_tcp = m_ifrule_tcp.begin(); it_tcp != m_ifrule_tcp.end();
             ++it_tcp) {
            for (auto it = it_tcp->second.begin(); it != it_tcp->second.end(); ++it) {
                ux_listen_ifrule(*it);
            }
        }

        for (auto it_udp = m_ifrule_udp.begin(); it_udp != m_ifrule_udp.end();
             ++it_udp) {
            for (auto it = it_udp->second.begin(); it != it_udp->second.end(); ++it) {
                ux_listen_ifrule(*it);
            }
        }

        ux_listen_ifrule(m_ifrule7);
        ux_listen_ifrule(m_ifrule3);
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

            it2 = it1->second.find("timeout");
            if (it2 != it1->second.end()) {
                try {
                    m_tcp.set_timeout(boost::lexical_cast<time_t>(it2->second));
                } catch (boost::bad_lexical_cast e) {
                    cerr << "cannot convert \"" << it2->second
                         << "\" to time_t" << endl;
                    continue;
                }
            }

            it2 = it1->second.find("lru");
            if (it2 != it1->second.end()) {
                if (it2->second == "yes") {
                    m_is_lru = true;
                } else if (it2->second == "no") {
                    m_is_lru = false;
                } else {
                    // error
                }
            }

        } else {
            ptr_ifrule rule = ptr_ifrule(new ifrule);

            rule->m_name = it1->first;
            auto it3 = it1->second.find("if");
            if (it3 == it1->second.end()) {
                rule->m_ux = ptr_path(new fs::path(it1->first));
            } else {
                rule->m_ux = ptr_path(new fs::path(it3->second));
            }

            it3 = it1->second.find("up");
            if (it3 != it1->second.end()) {
                rule->m_up = ptr_regex(new boost::regex(it3->second));
            }

            it3 = it1->second.find("down");
            if (it3 != it1->second.end()) {
                rule->m_down = ptr_regex(new boost::regex(it3->second));
            }

            it3 = it1->second.find("nice");
            if (it3 != it1->second.end()) {
                try {
                    rule->m_nice = boost::lexical_cast<int>(it3->second);
                } catch (boost::bad_lexical_cast e) {
                    cerr << "cannot convert \"" << it3->second
                         << "\" to int" << endl;
                    continue;
                }
            }


            it3 = it1->second.find("proto");
            if (it3 != it1->second.end()) {
                if (it3->second == "TCP") {
                    rule->m_proto = IF_TCP;
                } else if (it3->second == "UDP") {
                    rule->m_proto = IF_UDP;
                } else {
                    // error
                }
            } else {
                // error
            }

            it3 = it1->second.find("format");
            if (it3 != it1->second.end()) {
                if (it3->second == "binary") {
                    rule->m_format = IF_BINARY;
                } else if (it3->second == "text") {
                    rule->m_format = IF_TEXT;
                } else {
                    // error
                }
            }

            it3 = it1->second.find("body");
            if (it3 != it1->second.end()) {
                if (it3->second == "yes") {
                    rule->m_is_body = true;
                } else if (it3->second == "no") {
                    rule->m_is_body = false;
                } else {
                    // error
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

            if (rule->m_proto == IF_UDP) {
                m_ifrule_udp[rule->m_nice].push_back(rule);
            } else if (rule->m_proto == IF_TCP) {
                m_ifrule_tcp[rule->m_nice].push_back(rule);
            } else if (rule->m_name == "loopback3") {
                m_ifrule3 = rule;
            } else if (rule->m_name == "loopback7") {
                m_ifrule7 = rule;
            }
        }
    }
}

void
cdpi_appif::in_event(cdpi_stream_event st_event,
                     const cdpi_id_dir &id_dir, cdpi_bytes bytes)
{
    appif_event ev;

    ev.st_event = st_event;
    ev.id_dir   = id_dir;
    ev.bytes    = bytes;

    // produce event
    boost::mutex::scoped_lock lock(m_mutex);
    m_ev_queue.push_back(ev);
    m_condition.notify_one();
}

void
cdpi_appif::consume_event()
{
    for (;;) {
        appif_event ev;
        {
            // consume event
            boost::mutex::scoped_lock lock(m_mutex);
            while (m_ev_queue.size() == 0) {
                m_condition.wait(lock);
            }

            ev = m_ev_queue.front();
            m_ev_queue.pop_front();
        }

        if (ev.id_dir.m_id.get_l4_proto() == IPPROTO_TCP) {
            in_stream_event(ev.st_event, ev.id_dir, ev.bytes);
        } else if (ev.id_dir.m_id.get_l4_proto() == IPPROTO_UDP) {
            in_datagram(ev.id_dir, ev.bytes);
        }
    }
}

void
cdpi_appif::in_stream_event(cdpi_stream_event st_event,
                            const cdpi_id_dir &id_dir, cdpi_bytes bytes)
{
    switch (st_event) {
    case STREAM_SYN:
    case STREAM_CREATED:
    {
        auto it = m_info.find(id_dir.m_id);

        if (it == m_info.end()) {
            ptr_info info = ptr_info(new stream_info(id_dir.m_id));

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

        if (send_tcp_data(it->second, id_dir)) {
            if (id_dir.m_dir == FROM_ADDR1 &&
                it->second->m_buf2.size() > 0) {
                cdpi_id_dir id_dir2;
                id_dir2.m_id  = id_dir.m_id;
                id_dir2.m_dir = FROM_ADDR2;
                send_tcp_data(it->second, id_dir2);
            } else if (id_dir.m_dir == FROM_ADDR2 &&
                       it->second->m_buf1.size() > 0) {
                cdpi_id_dir id_dir2;
                id_dir2.m_id  = id_dir.m_id;
                id_dir2.m_dir = FROM_ADDR1;
                send_tcp_data(it->second, id_dir2);
            }
        }

        break;
    }
    case STREAM_DESTROYED:
    {
        auto it = m_info.find(id_dir.m_id);

        if (it == m_info.end())
            return;

        it->second->m_is_buf1 = true;
        it->second->m_is_buf2 = true;

        if (! it->second->m_buf1.empty()) {
            cdpi_id_dir id_dir2 = id_dir;
            id_dir2.m_dir = FROM_ADDR1;
            send_tcp_data(it->second, id_dir2);
        }

        if (! it->second->m_buf2.empty()) {
            cdpi_id_dir id_dir2 = id_dir;
            id_dir2.m_dir = FROM_ADDR2;
            send_tcp_data(it->second, id_dir2);
        }


        if (it->second->m_ifrule) {
            // invoke DESTROYED event
            auto it2 = m_name2uxpeer.find(it->second->m_ifrule->m_name);

            if (it2 != m_name2uxpeer.end()) {
                for (auto it3 = it2->second.begin(); it3 != it2->second.end();
                     ++it3) {
                    write_head(*it3, id_dir, it->second->m_ifrule->m_format,
                               STREAM_DESTROYED, MATCH_NONE, 0,
                               &it->second->m_header);
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

bool
cdpi_appif::send_tcp_data(ptr_info p_info, cdpi_id_dir id_dir)
{
    bool is_classified = false;

    boost::upgrade_lock<boost::shared_mutex> up_lock(m_rw_mutex);

    if (! p_info->m_ifrule) {
        // classify
        if (p_info->m_is_buf1 && p_info->m_is_buf2) {
            for (auto it_tcp = m_ifrule_tcp.begin();
                 it_tcp != m_ifrule_tcp.end(); ++it_tcp) {
                for (auto it2 = it_tcp->second.begin();
                     it2 != it_tcp->second.end(); ++it2) {
                    bool is_port;

                    if ((*it2)->m_port.empty()) {
                        is_port = true;
                    } else {
                        is_port = false;
                        for (auto it3 = (*it2)->m_port.begin();
                             it3 != (*it2)->m_port.end(); ++it3) {

                            if ((it3->first <= ntohs(id_dir.get_port_src()) &&
                                 ntohs(id_dir.get_port_src()) <= it3->second) ||
                                (it3->first <= ntohs(id_dir.get_port_dst()) &&
                                 ntohs(id_dir.get_port_dst()) <= it3->second)) {
                                is_port = true;
                            }
                        }
                    }

                    if (! is_port)
                        continue;

                    if ((*it2)->m_up && (*it2)->m_down) {
                        char buf1[4096], buf2[4096];
                        int  len1, len2;

                        len1 = read_bytes(p_info->m_buf1, buf1, sizeof(buf1));
                        string s1(buf1, len1);

                        len2 = read_bytes(p_info->m_buf2, buf2, sizeof(buf2));
                        string s2(buf2, len2);

                        if (boost::regex_match(s1, *(*it2)->m_up) &&
                            boost::regex_match(s2, *(*it2)->m_down)) {
                            p_info->m_match_dir[0] = MATCH_UP;
                            p_info->m_match_dir[1] = MATCH_DOWN;
                        } else if (boost::regex_match(s2, *(*it2)->m_up) &&
                                   boost::regex_match(s1, *(*it2)->m_down)) {
                            p_info->m_match_dir[1] = MATCH_UP;
                            p_info->m_match_dir[0] = MATCH_DOWN;
                        } else {
                            continue;
                        }
                    }

                    // matched a rule
                    p_info->m_ifrule = *it2;

                    // invoke CREATED event
                    auto it4 = m_name2uxpeer.find((*it2)->m_name);

                    if (it4 != m_name2uxpeer.end()) {
                        for (auto it5 = it4->second.begin();
                             it5 != it4->second.end(); ++it5) {
                            write_head(*it5, id_dir,
                                       p_info->m_ifrule->m_format,
                                       STREAM_CREATED, MATCH_NONE, 0,
                                       &p_info->m_header);
                        }
                    }

                    is_classified = true;

                    {
                        boost::upgrade_to_unique_lock<boost::shared_mutex> wlock(up_lock);

                        if (m_is_lru && it2 != it_tcp->second.begin()) {
                            ptr_ifrule ifrule = *it2;
                            it_tcp->second.erase(it2);
                            it_tcp->second.push_front(ifrule);
                        }
                    }

                    goto classified;
                }
            }
        } else {
            return is_classified;
        }
    }

classified:

    if (p_info->m_ifrule) {
        // invoke DATA event and send data to I/F
        deque<cdpi_bytes> *bufs;

        if (id_dir.m_dir == FROM_ADDR1) {
            bufs = &p_info->m_buf1;
        } else if (id_dir.m_dir == FROM_ADDR2) {
            bufs = &p_info->m_buf2;
        } else {
            assert(false);
        }

        while (! bufs->empty()) {
            auto front = bufs->front();
            auto it2 = m_name2uxpeer.find(p_info->m_ifrule->m_name);

            if (it2 != m_name2uxpeer.end()) {
                for (auto it3 = it2->second.begin(); it3 != it2->second.end();
                     ++it3) {
                    match_dir mdir;

                    assert(id_dir.m_dir != FROM_NONE);

                    mdir = p_info->m_match_dir[id_dir.m_dir];

                    if (! write_head(*it3, id_dir, p_info->m_ifrule->m_format,
                                     STREAM_DATA, mdir, front.get_len(),
                                     &p_info->m_header)) {
                        continue;
                    }

                    // write data
                    if (p_info->m_ifrule->m_is_body) {
                        write(*it3, front.get_head(), front.get_len());
                    }
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
            return is_classified;
        }
    }

    return is_classified;
}

bool
cdpi_appif::write_head(int fd, const cdpi_id_dir &id_dir, ifformat format,
                       cdpi_stream_event event, match_dir match, int bodylen,
                       cdpi_appif_header *header)
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
        s += boost::lexical_cast<string>(htons(id_dir.get_port1()));

        s += ",port2=";
        s += boost::lexical_cast<string>(htons(id_dir.get_port2()));

        s += ",hop=";
        s += boost::lexical_cast<string>((int)id_dir.m_id.m_hop);

        if (id_dir.m_id.get_l3_proto() == IPPROTO_IP) {
            s += ",l3=ipv4";
        } else if (id_dir.m_id.get_l3_proto() == IPPROTO_IPV6) {
            s += ",l3=ipv6";
        }

        if (id_dir.m_id.get_l4_proto() == IPPROTO_TCP) {
            s += ",l4=tcp";
        } else if (id_dir.m_id.get_l4_proto() == IPPROTO_UDP) {
            s += ",l4=udp";
        }

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
                s += "1,";
            } else if (id_dir.m_dir == FROM_ADDR2) {
                s += "2,";
            } else {
                s += "none,";
            }

            s += "match=";
            if (match == MATCH_UP) {
                s += "up";
            } else if (match == MATCH_DOWN) {
                s += "down";
            } else {
                s += "none";
            }

            s += ",len=";
            s += boost::lexical_cast<string>(bodylen);
            s += "\n";
            break;
        default:
            assert(false);
        }

        if (write(fd, s.c_str(), s.size()) < 0) {
            cout << "write error: fd = " << fd << endl;
            return false;
        }
    } else {
        header->event    = event;
        header->from     = id_dir.m_dir;
        header->hop      = id_dir.m_id.m_hop;
        header->l3_proto = id_dir.m_id.get_l3_proto();
        header->l4_proto = id_dir.m_id.get_l4_proto();
        header->len      = bodylen;
        header->match    = match;

        if (write(fd, header, sizeof(*header)) < 0)
            return false;
    }

    return true;
}

cdpi_appif::stream_info::stream_info(const cdpi_id &id) :
    m_dsize1(0), m_dsize2(0), m_is_created(false), m_is_giveup(false)
{
    m_match_dir[0] = MATCH_NONE;
    m_match_dir[1] = MATCH_NONE;

    gettimeofday(&m_create_time, NULL);

    memset(&m_header, 0, sizeof(m_header));

    memcpy(&m_header.l3_addr1, &id.m_addr1->l3_addr,
           sizeof(m_header.l3_addr1));
    memcpy(&m_header.l3_addr2, &id.m_addr2->l3_addr,
           sizeof(m_header.l3_addr2));

    m_header.l4_port1 = id.m_addr1->l4_port;
    m_header.l4_port2 = id.m_addr2->l4_port;
}

void
cdpi_appif::in_datagram(const cdpi_id_dir &id_dir, cdpi_bytes bytes)
{
    boost::upgrade_lock<boost::shared_mutex> up_lock(m_rw_mutex);

    for (auto it_udp = m_ifrule_udp.begin(); it_udp != m_ifrule_udp.end();
         ++it_udp) {
        for (auto it = it_udp->second.begin(); it != it_udp->second.end();
             ++it) {
            bool is_port = false;

            for (auto it2 = (*it)->m_port.begin();
                 it2 != (*it)->m_port.end(); ++it2) {

                if ((it2->first <= ntohs(id_dir.get_port_src()) &&
                     ntohs(id_dir.get_port_src()) <= it2->second) ||
                    (it2->first <= ntohs(id_dir.get_port_dst()) &&
                     ntohs(id_dir.get_port_dst()) <= it2->second)) {
                    is_port = true;
                }
            }

            if (! is_port)
                continue;

            match_dir match = MATCH_NONE;
            if ((*it)->m_up) {
                string s(bytes.get_head(), bytes.get_len());

                if (! boost::regex_match(s, *(*it)->m_up)) {
                    continue;
                }

                match = MATCH_UP;
            }

            cdpi_appif_header header;

            memset(&header, 0, sizeof(header));

            memcpy(&header.l3_addr1, &id_dir.m_id.m_addr1->l3_addr,
                   sizeof(header.l3_addr1));
            memcpy(&header.l3_addr2, &id_dir.m_id.m_addr2->l3_addr,
                   sizeof(header.l3_addr2));

            header.l4_port1 = id_dir.m_id.m_addr1->l4_port;
            header.l4_port2 = id_dir.m_id.m_addr2->l4_port;
            header.event    = DATAGRAM_DATA;
            header.from     = id_dir.m_dir;
            header.hop      = id_dir.m_id.m_hop;
            header.l3_proto = id_dir.m_id.get_l3_proto();
            header.len      = bytes.get_len();
            header.match    = match;

            auto it3 = m_name2uxpeer.find((*it)->m_name);

            if (it3 != m_name2uxpeer.end()) {
                for (auto it4 = it3->second.begin();
                     it4 != it3->second.end(); ++it4) {
                    if (! write_head(*it4, id_dir, (*it)->m_format, STREAM_DATA,
                                     match, bytes.get_len(), &header)) {
                        continue;
                    }

                    if ((*it)->m_is_body) {
                        if (write(*it4, bytes.get_head(), bytes.get_len()) < 0) {
                            continue;
                        }
                    }
                }
            }

            {
                boost::upgrade_to_unique_lock<boost::shared_mutex> wlock(up_lock);

                if (m_is_lru && it != it_udp->second.begin()) {
                    ptr_ifrule ifrule = *it;
                    it_udp->second.erase(it);
                    it_udp->second.push_front(ifrule);
                }
            }

            return;
        }
    }
}
