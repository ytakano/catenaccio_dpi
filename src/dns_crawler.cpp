#include "cdpi_dns.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <mongo/client/dbclient.h>

#include <iostream>
#include <set>
#include <string>

#include <boost/thread.hpp>

using namespace std;

#define QUERY_CYCLE  20 // [ms]
#define QUERIES_PER_CYCLE 300

string mongo_server("localhost:27017");
mongo::DBClientConnection mongo_conn(true);
int         sockfd_a;
int         sockfd_ver;

uint8_t arr1[256];
uint8_t arr2[256];
uint8_t arr3[256];
uint8_t arr4[256];

unsigned int send_total = 0;
unsigned int recv_total = 0;
int send_count = 1;

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
send_query()
{
    sockaddr_in saddr;
    int i, j, k, m;
    int n = 0;

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_port   = htons(53);
    saddr.sin_family = AF_INET;

    for (i = 0; i < 256; i++) {
        for (j = 0; j < 256; j++) {
            for (k = 0; k < 256; k++) {
                for (m = 0; m < 256; m++) {
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
                        n = 0;
                        boost::this_thread::sleep(boost::posix_time::milliseconds(QUERY_CYCLE));
                        if (send_count < 1000) {
                            send_count++;
                        } else {
                            cout << send_total << " "
                                 << (double)recv_total / (double)send_total * (double)0xffffffff
                                 << endl;
                            send_count = 1;
                        }
                    }
                }
            }
        }
    }

    cout << send_total << endl;
    boost::this_thread::sleep(boost::posix_time::milliseconds(30000));

    return;
}

void
recv_dns_ver()
{
    for (;;) {
        char buf[1024];
        sockaddr_in saddr;
        socklen_t slen = sizeof(saddr);
        ssize_t readlen;

        readlen = recvfrom(sockfd_ver, buf, sizeof(buf), 0, (sockaddr*)&saddr,
                           &slen);

        if (readlen < 0) {
            if (errno == EINTR)
                continue;

            break;
        }

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

            try {
                mongo_conn.update("DNSCrawl.servers", b1.obj(), b3.obj());
            } catch (...) {
                cout << "exception: " << addr << endl;
            }
        }
    }
}

void
recv_dns_a()
{
    for (;;) {
        char buf[1024];
        sockaddr_in saddr;
        socklen_t   slen = sizeof(saddr);
        ssize_t     readlen;

        readlen = recvfrom(sockfd_a, buf, sizeof(buf), 0, (sockaddr*)&saddr,
                           &slen);

        if (readlen < 0) {
            if (errno == EINTR)
                continue;

            break;
        }

        char addr[128];
        char *p = (char*)&saddr.sin_addr;

        inet_ntop(AF_INET, p, addr, sizeof(addr));

        cdpi_dns dns;

        if (dns.decode(buf, readlen)) {
            uint16_t sum;
            bool     is_eq_dst;

            recv_total++;

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
            b.append("recv_a_port", ntohs(saddr.sin_port));

            try {
                mongo_conn.insert("DNSCrawl.servers", b.obj());
            } catch (...) {
                cout << "exception: " << addr << endl;
            }

            query_ver[0] = p[0] ^ p[2];
            query_ver[1] = p[1] ^ p[3];

            saddr.sin_port = htons(53);

            sendto(sockfd_ver, query_ver, sizeof(query_ver), 0,
                   (sockaddr*)&saddr, slen);
        }
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
}

void
get_dns_ver()
{
    init();

    boost::thread thr_recv_a(recv_dns_a);
    boost::thread thr_recv_ver(recv_dns_ver);

    send_query();
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
