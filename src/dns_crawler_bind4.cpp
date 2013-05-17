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
#include <set>
#include <string>

using namespace std;

#define QUERY_CYCLE  200 // [ms]
#define CYCLE_PER_QUERY 5

string mongo_server("localhost:27017");
mongo::DBClientConnection mongo_conn;
auto_ptr<mongo::DBClientCursor> mongo_cursor;
event_base *ev_base;
event      *ev_dns;
event      *ev_send;
uint16_t    query_id;
int         sockfd;

unsigned int send_total = 0;

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
send_query(evutil_socket_t fd, short what, void *arg)
{
    sockaddr_in saddr;
    int i, j, k, m;
    int n = 0;

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_port   = htons(53);
    saddr.sin_family = AF_INET;

    while (mongo_cursor->more()) {
        mongo::BSONObj p = mongo_cursor->next();
        string addr = p.getStringField("_id");

        cout << addr << endl;

        if (inet_aton(addr.c_str(), &saddr.sin_addr) == 0) {
            continue;
        }

        sendto(sockfd, query, sizeof(query), 0,
               (sockaddr*)&saddr, sizeof(saddr));

        send_total++;
        n++;
        if (n >= CYCLE_PER_QUERY) {
            break;
        }
    }

    cout << send_total << endl;

    if (n < CYCLE_PER_QUERY) {
        event_del(ev_send);
        event_free(ev_send);
    }

    return;
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

        if (ntohs(saddr.sin_port) != 53)
            break;

        char addr[128];

        inet_ntop(AF_INET, &saddr.sin_addr, addr, sizeof(addr));

        cdpi_dns dns;

        if (dns.decode(buf, readlen)) {
            const list<cdpi_dns_rr> &ans = dns.get_answer();
            list<cdpi_dns_rr>::const_iterator it;

            mongo::BSONObjBuilder b;
            mongo::BSONObj        doc;

            b.append("_id", addr);

            for (it = ans.begin(); it != ans.end(); ++it) {
                if (ntohs(it->m_type) == DNS_TYPE_TXT &&
                    ntohs(it->m_class) == DNS_CLASS_CH) {
                    ptr_cdpi_dns_txt      p_txt;

                    p_txt = DNS_RDATA_TO_TXT(it->m_rdata);

                    b.append("ver", p_txt->m_txt);

                    break;
                }
            }

            doc = b.obj();

            cout << doc.toString() << endl;

            mongo_conn.insert("DNSCrawl.servers_bind4", doc);
        }

        break;
    }
    default:
        ;
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

    init_query();

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("socket");
        exit(-1);
    }

    ev_dns = event_new(ev_base, sockfd, EV_READ | EV_PERSIST, callback_dns, NULL);
    event_add(ev_dns, NULL);

    timeval tv = {0, QUERY_CYCLE * 1000};
    ev_send = event_new(ev_base, -1, EV_TIMEOUT | EV_PERSIST, send_query, NULL);
    event_add(ev_send, &tv);

    mongo_cursor = mongo_conn.query("DNSCrawl.servers",
                                    QUERY("type" << "BIND"
                                          << "type_ver" << "4.x"));
}

void
get_dns_ver()
{
    init();

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

    get_dns_ver();

    return 0;
}
