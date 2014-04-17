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

void ux_read(int fd, short events, void *arg);

cdpi_appif::cdpi_appif()
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

        for (auto it = m_ifrule.begin(); it != m_ifrule.end(); ++it) {
            sock = socket(AF_UNIX, SOCK_STREAM, 0);

            if (sock == -1) {
                perror("socket");
                exit(-1);
            }
 
            struct sockaddr_un sa = {0};
            sa.sun_family = AF_UNIX;

            cout << "ux: " << (*it)->m_ux << endl;
            strncpy(sa.sun_path, (*it)->m_ux.c_str(), sizeof(sa.sun_path));
 
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
                m_ifrule.push_back(rule);
                cout << "add the rule, ux: " << rule->m_ux << endl;
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
                } else if (key == "ux") {
                    rule->m_ux = value;
                } else if (key == "protocol") {
                    if (value == "TCP") {
                        rule->m_is_tcp = true;
                    } else if (value == "UDP") {
                        rule->m_is_udp = true;
                    }
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

            cout << "section: " << line << endl;
            state = KEY_VALUE;

            rule->m_name = line;

            break;
        }
        }
    }
}
