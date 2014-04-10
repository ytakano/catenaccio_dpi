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
    }
}

void
ux_read(int fd, short events, void *arg)
{
    if (! (events & EV_READ))
        return;

    for (;;) {
        char buf[4096];
        int  recv_size = read(fd, buf, sizeof(buf) - 1);

        if (recv_size > 0) {
            continue;
        } else if (recv_size == 0) {
            break;
        } else {
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

                appif->m_fd2uxpeer.erase(it1);
            }

            shutdown(fd, SHUT_RDWR);
            close(fd);

            break;
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

        for (auto it = m_ifrule.begin(); it != m_ifrule.end(); ++it) {
            sock = socket(AF_UNIX, SOCK_STREAM, 0);

            if (sock == -1) {
                perror("socket");
                exit(-1);
            }
 
            struct sockaddr_un sa = {0};
            sa.sun_family = AF_UNIX;
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
                cout << "add the rule" << endl;
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
                }

                break;
            }
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

    run();
}
