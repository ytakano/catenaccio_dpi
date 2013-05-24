#include "cdpi_dns.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <event.h>

#include <mongo/client/dbclient.h>

#include <iostream>
#include <string>
#include <queue>

using namespace std;

#define QUERY_CYCLE     200
#define QUERY_PER_CYCLE 1000


string mongo_server("localhost:27017");
mongo::DBClientConnection mongo_conn;
event_base   *ev_base;
event        *ev_dns;
event        *ev_send;
queue<string> dns_servers;
uint16_t      query_id;
int           sock_fd;

void callback_dns(evutil_socket_t fd, short what, void *arg);

char query[30];

/*
 * struct query {
 *     uint16_t id;
 *     uint16_t flag;        // 0x0100 (Standard Query), [2]
 *     uint16_t qd_count;    // 1, [4]
 *     uint16_t an_count;    // 0, [6]
 *     uint16_t ns_count;    // 0, [8]
 *     uint16_t ar_count;    // 0, [10]
 *     char     name[14];    // VERSION.BIND, [12]
 *     uint16_t query_type;  // 0x0010 (TXT), [26]
 *     uint16_t query_class; // 0x0003 (CHAOS), [28]
 * };
*/

void
init_query()
{
    uint16_t n;

    memset(query, 0, sizeof(query));

    // id
    srand(time(NULL));
    n = htons((uint16_t)(rand() & 0x0000ffff));
    memcpy(query, &n, 2);

    query_id = n;

    // flag
    n = htons(0x0100);
    memcpy(&query[2], &n, 2);

    // qd_count
    n = htons(1);
    memcpy(&query[4], &n, 2);

    // name, 0x07 "VERSION"
    query[12] = 7;
    memcpy(&query[13], "VERSION", 7);

    // name, 0x04, "BIND"
    query[20] = 4;
    memcpy(&query[21], "BIND", 4);

    // query type
    n = htons(0x0010);
    memcpy(&query[26], &n, 2);

    // query class
    n = htons(0x0003);
    memcpy(&query[28], &n, 2);
}

void
quit_query(evutil_socket_t fd, short what, void *arg)
{
    exit(0);
}

void
send_query(evutil_socket_t fd, short what, void *arg)
{
    int n = 0;

    while (! dns_servers.empty()) {
        sockaddr_in saddr;
        string addr = dns_servers.front();

        memset(&saddr, sizeof(saddr), 0);

        dns_servers.pop();

        int s = inet_pton(AF_INET, addr.c_str(), &saddr.sin_addr);
        if (s < 0) {
            perror("inet_pton");
            continue;
        }

        saddr.sin_port   = htons(53);
        saddr.sin_family = AF_INET;

        sendto(sock_fd, query, sizeof(query), 0,
               (sockaddr*)&saddr, sizeof(saddr));

        n++;

        if (n > QUERY_PER_CYCLE) {
            break;
        }
    }

    if (dns_servers.empty()) {
        event_del(ev_send);
        event_free(ev_send);

        timeval tv = {3, 0};
        event *ev = event_new(ev_base, -1, EV_TIMEOUT, quit_query, NULL);
        event_add(ev, &tv);
    }
}

void
callback_dns(evutil_socket_t fd, short what, void *arg)
{
    switch (what) {
    case EV_READ:
    {
        char buf[1024];
        sockaddr_in saddr;
        socklen_t   slen = sizeof(saddr);
        ssize_t readlen;

        readlen = recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&saddr, &slen);

        if (readlen < 0)
            break;

        if (ntohs(saddr.sin_port) == 53) {
            char addr[128];

            inet_ntop(AF_INET, &saddr.sin_addr, addr, sizeof(addr));

            cout << "reply from " << addr << ":"
                 << ntohs(saddr.sin_port) << endl;

            cdpi_dns dns;

            if (dns.decode(buf, readlen)) {
                const list<cdpi_dns_rr> &ans = dns.get_answer();
                list<cdpi_dns_rr>::const_iterator it;

                for (it = ans.begin(); it != ans.end(); ++it) {
                    if (ntohs(it->m_type) == DNS_TYPE_TXT &&
                        ntohs(it->m_class) == DNS_CLASS_CH) {
                        mongo::BSONObjBuilder b1, b2, b3;
                        mongo::BSONObj        doc1, doc2;
                        ptr_cdpi_dns_txt      p_txt;

                        p_txt = DNS_RDATA_TO_TXT(it->m_rdata);

                        b1.append("_id", addr);
                        b2.append("ver", p_txt->m_txt);
                        b3.append("$set", b2.obj());

                        doc1 = b1.obj();
                        doc2 = b3.obj();

                        mongo_conn.update("DNS.servers", doc1, doc2);

                        cout << p_txt->m_txt << endl;

                        break;
                    }
                }
            }
        }
        break;
    }
    default:
        ;
    }
}

void
get_servers()
{
    auto_ptr<mongo::DBClientCursor> cur = mongo_conn.query("DNS.servers",
                                                           mongo::BSONObj());

    while (cur->more()) {
        mongo::BSONObj p = cur->next();
        dns_servers.push(p.getStringField("_id"));
    }
}

void
init()
{
    string errmsg;

    if (! mongo_conn.connect(mongo_server, errmsg)) {
        cerr << errmsg << endl;
        exit(-1);
    }

    ev_base = event_base_new();
    if (! ev_base) {
        std::cerr << "couldn't new event_base" << std::endl;
        exit(-1);
    }

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(-1);
    }

    ev_dns = event_new(ev_base, sock_fd, EV_READ | EV_PERSIST, callback_dns,
                       NULL);
    event_add(ev_dns, NULL);

    timeval tv = {0, QUERY_CYCLE * 1000};
    ev_send = event_new(ev_base, -1, EV_TIMEOUT | EV_PERSIST, send_query, NULL);
    event_add(ev_send, &tv);

    init_query();
    get_servers();

    event_base_dispatch(ev_base);
}

void
print_usage(char *cmd)
{
    cout << "usage: " << cmd << " -m localhost:27017"
         << "\n\t -m: an address of MongoDB. localhost:27017 is a default value"
         << "\n\t -h: show this help"
         << endl;
}

int
main(int argc, char *argv[])
{
    int opt;
    const char *optstr = "m:h";

    while ((opt = getopt(argc, argv, optstr)) != -1) {
        switch (opt) {
        case 'm':
            mongo_server = optarg;
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            return 0;
        }
    }

    init();

    return 0;
}
