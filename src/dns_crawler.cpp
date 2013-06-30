#include "cdpi_dns.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <event.h>

#include <mongo/client/dbclient.h>

#include <iostream>
#include <set>
#include <string>

using namespace std;

#define QUERY_CYCLE  250 // [ms]
#define QUERIES_PER_CYCLE 1500

string mongo_server("localhost:27017");
mongo::DBClientConnection mongo_conn;
event_base *ev_base;
event      *ev_dns_a;
event      *ev_dns_ver;
event      *ev_send;
event      *ev_exit;
uint32_t    total = 0;
int         sockfd_a;
int         sockfd_ver;

volatile int n1 = 0;
volatile int n2 = 0;
volatile int n3 = 0;
volatile int n4 = 0;

uint8_t arr1[256];
uint8_t arr2[256];
uint8_t arr3[256];
uint8_t arr4[256];

unsigned int send_total = 0;

void callback_dns_a(evutil_socket_t fd, short what, void *arg);
void callback_dns_ver(evutil_socket_t fd, short what, void *arg);

void
get_epoch_millis(mongo::Date_t &date)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    date.millis = ((unsigned long long)(tv.tv_sec) * 1000 +
                   (unsigned long long)(tv.tv_usec) / 1000);
}

char query_a[39];
char query_ver[30];

/*
 * struct query_a {
 *     uint16_t id;
 *     uint16_t flag;        // 0x0000 (Standard Query), [2]
 *     uint16_t qd_count;    // 1, [4]
 *     uint16_t an_count;    // 0, [6]
 *     uint16_t ns_count;    // 0, [8]
 *     uint16_t ar_count;    // 0, [10]
 *     char     name[23];    // jupitoris.jaist.ac.jp, [12]
 *     uint16_t query_type;  // 0x0001 (A), [35]
 *     uint16_t query_class; // 0x0001 (IN), [37]
 * };
 *
 * struct query_ver {
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
init_query_a()
{
    uint16_t n;

    memset(query_a, 0, sizeof(query_a));

    // qd_count
    n = htons(1);
    memcpy(&query_a[4], &n, 2);

    // name, 0x09 "jupitoris"
    query_a[12] = 9;
    memcpy(&query_a[13], "jupitoris", 9);

    // name, 0x05, "jaist"
    query_a[22] = 5;
    memcpy(&query_a[23], "jaist", 5);

    // name, 0x02, "ac"
    query_a[28] = 2;
    memcpy(&query_a[29], "ac", 2);

    // name, 0x02, "jp"
    query_a[31] = 2;
    memcpy(&query_a[32], "jp", 2);
    
    // query type
    n = htons(0x0001);
    memcpy(&query_a[35], &n, 2);

    // query class
    n = htons(0x0001);
    memcpy(&query_a[37], &n, 2);
}

void
init_query_ver()
{
    uint16_t n;

    memset(query_ver, 0, sizeof(query_ver));

    // flag
    n = htons(0x0100);
    memcpy(&query_ver[2], &n, 2);

    // qd_count
    n = htons(1);
    memcpy(&query_ver[4], &n, 2);

    // name, 0x07 "VERSION"
    query_ver[12] = 7;
    memcpy(&query_ver[13], "VERSION", 7);

    // name, 0x04, "BIND"
    query_ver[20] = 4;
    memcpy(&query_ver[21], "BIND", 4);

    // query type
    n = htons(0x0010);
    memcpy(&query_ver[26], &n, 2);

    // query class
    n = htons(0x0003);
    memcpy(&query_ver[28], &n, 2);
}

void
init_arr(uint8_t *arr)
{
    int i;

    for (i = 0; i < 256; i++) {
        arr[i] = i;
    }

    for (i = 0; i < 256; i++) {
        int r = rand() % 256;
        uint8_t tmp;

        tmp = arr[i];
        arr[i] = arr[r];
        arr[r] = tmp;
    }
}

void
exit_callback(evutil_socket_t fd, short what, void *arg)
{
    exit(0);
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

    j = n2;
    k = n3;
    m = n4;

    for (i = n1; i < 256; i++) {
        for (j = n2; j < 256; j++) {
            for (k = n3; k < 256; k++) {
                for (m = n4; m < 256; m++) {
                    if (arr4[m] >= 224 || // multicast & reserved
                        arr4[m] == 0   || // reserved
                        arr4[m] == 127 || // localhost
                        arr4[m] == 10  || // class A private
                        (arr4[m] == 172 &&
                         16 <= arr3[k] && arr3[k] <= 31)   || // class B private
                        (arr4[m] == 192 && arr3[k] == 168) || // class C private
                        (arr4[m] == 169 && arr3[k] == 254) || // link local
                        (arr4[m] == 202 && arr3[k] == 241 &&
                         arr2[j] == 109)                   || // 農林水産省
                        (arr4[m] == 166 && arr3[k] == 119) || // 農林水産省
                        (arr4[m] == 128 && arr3[k] == 100)) { // トロント大学
                        continue;
                    }

                    char *p = (char*)&saddr.sin_addr;

                    p[0] = arr4[m];
                    p[1] = arr3[k];
                    p[2] = arr2[j];
                    p[3] = arr1[i];

                    query_a[0] = p[0] ^ p[2];
                    query_a[1] = p[1] ^ p[3];

                    sendto(sockfd_a, query_a, sizeof(query_a), 0,
                           (sockaddr*)&saddr, sizeof(saddr));

                    send_total++;
                    n++;
                    if (n >= QUERIES_PER_CYCLE) {
                        m++;
                        goto end_loop;
                    }
                }
                n4 = 0;
            }
            n3 = 0;
        }
        n2 = 0;
    }

end_loop:
    n1 = i;
    n2 = j;
    n3 = k;
    n4 = m;

    cout << send_total << endl;

    if (n < QUERIES_PER_CYCLE) {
        event_del(ev_send);
        event_free(ev_send);

        timeval tv = {10, 0};
        ev_exit = event_new(ev_base, -1, EV_TIMEOUT, exit_callback, NULL);
        event_add(ev_exit, &tv);
    }

    return;
}

void
callback_dns_ver(evutil_socket_t fd, short what, void *arg)
{
    switch (what) {
    case EV_READ:
    {
        for (;;) {
            char buf[1024];
            sockaddr_in saddr;
            socklen_t slen = sizeof(saddr);
            ssize_t readlen;

            readlen = recvfrom(fd, buf, sizeof(buf), MSG_DONTWAIT,
                               (sockaddr*)&saddr, &slen);

            if (readlen < 0)
                break;

            if (ntohs(saddr.sin_port) != 53)
                break;

            char addr[128];
            char *p = (char*)&saddr.sin_addr;

            inet_ntop(AF_INET, p, addr, sizeof(addr));

            cdpi_dns dns;

            if (dns.decode(buf, readlen)) {
                const list<cdpi_dns_rr> &ans = dns.get_answer();
                list<cdpi_dns_rr>::const_iterator it;

                auto_ptr<mongo::DBClientCursor> cur;
                mongo::BSONObjBuilder b1, b2, b3;
                mongo::BSONObj doc;
                mongo::Date_t date2;

                get_epoch_millis(date2);

                b1.append("_id", addr);
                b2.append("date2", date2);
                b2.append("rcode_ver", dns.get_rcode());

                for (it = ans.begin(); it != ans.end(); ++it) {
                    if (ntohs(it->m_type) == DNS_TYPE_TXT &&
                        ntohs(it->m_class) == DNS_CLASS_CH) {
                        ptr_cdpi_dns_txt p_txt;

                        p_txt = DNS_RDATA_TO_TXT(it->m_rdata);

                        b2.append("ver", p_txt->m_txt);

                        break;
                    }
                }

                b3.append("$set", b2.obj());

                //cout << doc.toString() << endl;

                mongo_conn.update("DNSCrawl.servers", b1.obj(), b3.obj());
            }
        }
        break;
    }
    default:
        ;
    }
}

void
callback_dns_a(evutil_socket_t fd, short what, void *arg)
{
    switch (what) {
    case EV_READ:
    {
        for (;;) {
            char buf[1024];
            sockaddr_in saddr;
            socklen_t   slen = sizeof(saddr);
            ssize_t     readlen;

            readlen = recvfrom(fd, buf, sizeof(buf), MSG_DONTWAIT,
                               (sockaddr*)&saddr, &slen);

            if (readlen < 0)
                break;

            if (ntohs(saddr.sin_port) != 53)
                break;

            char addr[128];
            char *p = (char*)&saddr.sin_addr;

            inet_ntop(AF_INET, p, addr, sizeof(addr));

            cdpi_dns dns;

            if (dns.decode(buf, readlen)) {
                uint16_t sum;
                bool     is_eq_dst;

                ((char*)&sum)[0] = p[0] ^ p[2];
                ((char*)&sum)[1] = p[1] ^ p[3];

                if (memcmp(&sum, buf, sizeof(sum)) == 0) {
                    is_eq_dst = true;
                } else {
                    is_eq_dst = false;
                }

                auto_ptr<mongo::DBClientCursor> cur;
                mongo::BSONObjBuilder b;
                mongo::Date_t         date1;

                get_epoch_millis(date1);

                b.append("_id", addr);
                b.append("date1", date1);
                b.append("is_eq_dst", is_eq_dst);
                b.append("is_ra", dns.is_ra());
                b.append("rcode_a", dns.get_rcode());

                mongo_conn.insert("DNSCrawl.servers", b.obj());

                query_ver[0] = p[0] ^ p[2];
                query_ver[1] = p[1] ^ p[3];

                sendto(sockfd_ver, query_ver, sizeof(query_ver), 0,
                       (sockaddr*)&saddr, slen);
            }
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

    mongo_conn.dropDatabase("DNSCrawl");
    mongo_conn.ensureIndex("DNSCrawl.servers",
                           mongo::fromjson("{date1: 1}"));
    mongo_conn.ensureIndex("DNSCrawl.servers",
                           mongo::fromjson("{date2: 1}"));

    ev_base = event_base_new();
    if (! ev_base) {
        std::cerr << "couldn't new event_base" << std::endl;
        exit(-1);
    }

    init_query_a();
    init_query_ver();

    sockfd_a = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_a < 0) {
        perror("socket");
        exit(-1);
    }

    sockfd_ver = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ver < 0) {
        perror("socket");
        exit(-1);
    }

    init_arr(arr1);
    init_arr(arr2);
    init_arr(arr3);
    init_arr(arr4);

    ev_dns_a = event_new(ev_base, sockfd_a, EV_READ | EV_PERSIST,
                         callback_dns_a, NULL);
    event_add(ev_dns_a, NULL);

    ev_dns_ver = event_new(ev_base, sockfd_ver, EV_READ | EV_PERSIST,
                           callback_dns_ver, NULL);
    event_add(ev_dns_ver, NULL);

    timeval tv = {0, QUERY_CYCLE * 1000};
    ev_send = event_new(ev_base, -1, EV_TIMEOUT | EV_PERSIST, send_query, NULL);
    event_add(ev_send, &tv);
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
