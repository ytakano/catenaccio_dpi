#include "cdpi_dns.hpp"

#include <mongo/client/dbclient.h>

#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <map>
#include <string>
#include <vector>

#include <event.h>
#include <event2/util.h>
#include <event2/dns.h>

using namespace std;

#define RESOLVE_MAX (1024 * 20)

const char *dns_server = "8.8.8.8";

event_base *event_base = NULL;
evdns_base *evdns_base = NULL;

mongo::DBClientConnection mongo_conn;
auto_ptr<mongo::DBClientCursor> mongo_cur;

int total = 0;

void
main_callback(int result, char type, int count, int ttl, void *addrs,
              void *orig)
{
    int i;

    cout << total++ << endl;

    for (i = 0; i < count; ++i) {
        if (type == DNS_PTR) {
            mongo::BSONObjBuilder b1, b2, b3;
            mongo::BSONObj doc;
            char *addr = (char*)orig;

            cout << addr << " " << ((char**)addrs)[i] << endl;

            b1.append("_id", addr);
            b2.append("fqdn", ((char**)addrs)[i]);
            b3.append("$set", b2.obj());

            mongo_conn.update("DNSCrawl.servers", b1.obj(), b3.obj());
        }
    }

    free((char*)orig);


    if (mongo_cur->more()) {
        mongo::BSONObj p = mongo_cur->next();
        string  saddr = p.getStringField("_id");
        in_addr addr;

        if (evutil_inet_pton(AF_INET, saddr.c_str(), &addr)!=1) {
            return;
        }

        char *s = strdup(saddr.c_str());
        evdns_base_resolve_reverse(evdns_base, &addr, 0, main_callback, s);
    }
}

void
resolve_ptr()
{
    int i;

    for (i = 0; i < RESOLVE_MAX; i++) {
        if (! mongo_cur->more())
            break;

        mongo::BSONObj p = mongo_cur->next();
        string  saddr = p.getStringField("_id");
        in_addr addr;

        if (evutil_inet_pton(AF_INET, saddr.c_str(), &addr)!=1) {
            continue;
        }

        char *s = strdup(saddr.c_str());
        evdns_base_resolve_reverse(evdns_base, &addr, 0, main_callback, s);
    }
}

void
print_usage(char *cmd)
{
    cout << "usage: " << cmd << " -m localhost:27017"
         << "\n\t -m: an address of MongoDB. localhost:27017 is a default value"
         << "\n\t -f: an address of DNS server to query"
         << "\n\t -h: show this help"
         << endl;
}

int
main(int argc, char *argv[]) {
    int opt;
    const char *optstr = "m:f:h";
    const char *mongo_server = "localhost:27017";
    string errmsg;

    while ((opt = getopt(argc, argv, optstr)) != -1) {
        switch (opt) {
        case 'm':
            mongo_server = optarg;
            break;
        case 'f':
            dns_server = optarg;
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            return 0;
        }
    }

    if (! mongo_conn.connect(mongo_server, errmsg)) {
        cerr << errmsg << endl;
        return -1;
    }

    event_base = event_base_new();
    evdns_base = evdns_base_new(event_base, 0);
    
    evdns_base_nameserver_ip_add(evdns_base, dns_server);

    mongo_cur = mongo_conn.query("DNSCrawl.servers", mongo::BSONObj(), 0, 0, 0,
                                 16);

    resolve_ptr();

    event_base_dispatch(event_base);

    return 0;
}
