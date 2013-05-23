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

#define QUERY_CYCLE  200 // [ms]
#define CYCLE_PER_QUERY 10000

string mongo_server("localhost:27017");
mongo::DBClientConnection mongo_conn;
event_base *ev_base;
event      *ev_dns;
event      *ev_send;
uint16_t    query_id;
int         sockfd;

volatile int n1 = 0;
volatile int n2 = 0;
volatile int n3 = 0;
volatile int n4 = 0;

uint8_t arr1[256];
uint8_t arr2[256];
uint8_t arr3[256];
uint8_t arr4[256];

unsigned int send_total = 0;

void callback_dns(evutil_socket_t fd, short what, void *arg);

void
get_epoch_millis(mongo::Date_t &date)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    date.millis = ((unsigned long long)(tv.tv_sec) * 1000 +
                   (unsigned long long)(tv.tv_usec) / 1000);
}

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
send_query(evutil_socket_t fd, short what, void *arg)
{
    sockaddr_in saddr;
    int i, j, k, m;
    int n = 0;

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_port   = htons(53);
    saddr.sin_family = AF_INET;

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
                        (arr4[m] == 150 && arr3[k] == 26)  || // 農林水産省
                        (arr4[m] == 166 && arr3[k] == 119)) { // 農林水産省
                        continue;
                    }

                    char *p = (char*)&saddr.sin_addr;

                    p[0] = arr4[m];
                    p[1] = arr3[k];
                    p[2] = arr2[j];
                    p[3] = arr1[i];

                    sendto(sockfd, query, sizeof(query), 0,
                           (sockaddr*)&saddr, sizeof(saddr));

                    mongo::BSONObjBuilder b;
                    mongo::BSONObj        doc;
                    mongo::Date_t         date;

                    char addr[128];

                    inet_ntop(AF_INET, &saddr.sin_addr, addr, sizeof(addr));

                    get_epoch_millis(date);

                    b.append("_id", addr);
                    b.append("date", date);

                    mongo_conn.insert("DNSCrawl.tmp_send_date", doc);

                    send_total++;
                    n++;
                    if (n >= CYCLE_PER_QUERY) {
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

            auto_ptr<mongo::DBClientCursor> cur;
            mongo::BSONObjBuilder b;
            mongo::BSONObj        p, doc;
            mongo::Date_t         recv_date, send_date;

            cur = mongo_conn.query("DNSCrawl.tmp_send_date", QUERY("_id" << addr));
            p   = cur->next();

            send_date = p.getField("date").Date();

            get_epoch_millis(recv_date);

            b.append("_id", addr);
            b.append("recv_date", recv_date);
            b.append("send_date", send_date);

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

            mongo_conn.insert("DNSCrawl.servers", doc);
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

    mongo_conn.dropDatabase("DNSCrawl.servers");
    mongo_conn.dropDatabase("DNSCrawl.tmp_send_date");
    mongo_conn.ensureIndex("DNSCrawl.servers", mongo::fromjson("{date: 1}"));

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

    init_arr(arr1);
    init_arr(arr2);
    init_arr(arr3);
    init_arr(arr4);

    ev_dns = event_new(ev_base, sockfd, EV_READ | EV_PERSIST, callback_dns, NULL);
    event_add(ev_dns, NULL);

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
