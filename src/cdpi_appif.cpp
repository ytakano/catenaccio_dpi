#include "cdpi_appif.hpp"
#include "cdpi_bytes.hpp"

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

    {
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

        cout << "accept: fd = " << sock << endl;
    }
}

void
ux_read(int fd, short events, void *arg)
{
    char buf[4096];
    int  recv_size = read(fd, buf, sizeof(buf) - 1);

    cout << "read: fd = " << fd << ", read_size = " << recv_size << endl;

    if (recv_size > 0) {
        cout.write(buf, recv_size);
        cout << endl;
        return;
    } else if (recv_size <= 0) {
        cdpi_appif *appif = static_cast<cdpi_appif*>(arg);
        boost::mutex::scoped_lock lock(appif->m_mutex);

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

        cout << "close: fd = " << fd << endl;
    }
}

void
cdpi_appif::makedir(fs::path path)
{
    cout << "path = " << path.string() << endl;

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

        cout << "m_home = " << m_home->string() << endl;

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

            cout << "ux: " << path.string() << endl;
            strncpy(sa.sun_path, path.string().c_str(), sizeof(sa.sun_path));
 
            remove(sa.sun_path);
 
            if (bind(sock, (struct sockaddr*) &sa,
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
    ifstream   ifs(conf);
    ptr_ifrule rule;
    string     section;

    enum {
        SECTION,
        KEY_VALUE,
    } state = SECTION;

    while (ifs) {
        int n = 1;
        string line;
        std::getline(ifs, line);

        stringstream s1(line);
        std::getline(s1, line, '#');

        switch (state) {
        case KEY_VALUE:
        {
            stringstream s3(line);
            char c;

            c = (char)s3.peek();

            if (c != ' ') {
                state = SECTION;

                if (section != "global") {
                    m_ifrule.push_back(rule);
                    cout << "add the rule, ux: " << rule->m_ux->string()
                         << endl;
                }
            } else {
                for (int i = 0; i < 4; i++) {
                    s3.get(c);

                    if (c != ' ') {
                        cerr << "An error occurred while reading config file \""
                             << conf << ":" << n
                             << "\". The indent must be 4 bytes white space."
                             << endl;
                        exit(-1);
                    }
                }

                string key, value;
                std::getline(s3, key, '=');
                std::getline(s3, value);

                key   = trim(key);
                value = trim(value);

                cout << "key: " << key << ", value: " << value << endl;

                if (key == "up") {
                    rule->m_up = ptr_regex(new boost::regex(value));
                } else if (key == "down") {
                    rule->m_down = ptr_regex(new boost::regex(value));
                } else if (key == "proto") {
                    if (value == "TCP") {
                        rule->m_proto = IF_TCP;
                    } else if (value == "UDP") {
                        rule->m_proto = IF_UDP;
                    }
                } else if (key == "format") {
                    if (value == "binary") {
                        rule->m_format = IF_BINARY;
                    } else if (value == "text") {
                        rule->m_format = IF_TEXT;
                    }
                } else if (key == "if") {
                    rule->m_ux = ptr_path(new fs::path(value));
                } else if (key == "port") {
                    stringstream s4(value);

                    while (s4) {
                        string port, n1, n2;
                        std::getline(s4, port, ',');

                        if (port.empty())
                            break;

                        port = trim(port);

                        stringstream s5(port);
                        std::getline(s5, n1, '-');
                        std::getline(s5, n2);

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

                        cout << "port range: " << range.first << " - "
                             << range.second << endl;

                        rule->m_port.push_back(range);
                    }
                } else if (section == "global") {
                    if (key == "home") {
                        m_home = ptr_path(new fs::path(value));
                    }
                }
            }

            break;
        }
        case SECTION:
        {
            stringstream s2(line);
            std::getline(s2, line, ':');

            rule = ptr_ifrule(new ifrule);
            line = trim(line);

            section = line;

            cout << "section: " << line << endl;
            state = KEY_VALUE;

            rule->m_name = line;

            break;
        }
        }
    }
}

void
cdpi_appif::in_stream_event(cdpi_stream_event st_event,
                            const cdpi_id_dir &id_dir, cdpi_bytes bytes)
{
    uint16_t sport, dport;
    char src[64], dst[64];
    cdpi_id_dir id_dir2 = id_dir;

    id_dir2.m_dir = FROM_NONE;

    id_dir2.get_addr_src(src, sizeof(src));
    id_dir2.get_addr_dst(dst, sizeof(dst));
    sport = ntohs(id_dir2.get_port_src());
    dport = ntohs(id_dir2.get_port_dst());

    switch (st_event) {
    case STREAM_SYN:
    {
        auto it = m_info.find(id_dir.m_id);

        if (it == m_info.end()) {
            ptr_info info = ptr_info(new stream_info);

            m_info[id_dir.m_id] = info;

            // TODO: invoke CREATED event

            cout << "created: src = " << src << ":" << sport
                 << ", dst = " << dst << ":" << dport << endl;
        }

        // TODO: invoke SYN event

        break;
    }
    case STREAM_DATA:
    {
        // TODO invoke DATA event

        break;
    }
    case STREAM_DESTROYED:
    {
        m_info.erase(id_dir.m_id);

        cout << "destroyed: src = " << src << ":" << sport
             << ", dst = " << dst << ":" << dport << endl;

        // TODO: invoke DESTROYED event

        break;
    }
    case STREAM_FIN:
    {
        // TODO: invoke FIN event

        break;
    }
    case STREAM_TIMEOUT:
    {
        // TODO: invoke TIMEOUT event

        break;
    }
    case STREAM_RST:
    {
        // TODO: invoke RST event

        break;
    }
    }
}
