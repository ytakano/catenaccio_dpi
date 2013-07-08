#include "cdpi_dns.hpp"

#include <mongo/client/dbclient.h>

#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <map>
#include <string>
#include <vector>

#include <unbound.h>

using namespace std;

#define RESOLVE_MAX 16384

static char *dns_server = NULL;

mongo::DBClientConnection mongo_conn(true, 0, -1);

void
split(const string &str, char delim, vector<string>& res){
    size_t current = 0, found;

    while((found = str.find_first_of(delim, current)) != string::npos){
        res.push_back(string(str, current, found - current));
        current = found + 1;
    }

    res.push_back(string(str, current, str.size() - current));
}

bool
ptr2addr(string ptr, string &addr)
{
    vector<string> sp;

    split(ptr, '.', sp);

    if (sp.size() < 6)
        return false;

    addr = sp[3] + "." + sp[2] + "." + sp[1] + "." + sp[0];

    return true;
}

void
ubcallback(void *mydata, int err, struct ub_result *result)
{
    int *resolving = (int*)mydata;

    (*resolving)--;

    if (result->havedata) {
        cdpi_dns dns_decoder;

        if (dns_decoder.decode((char*)result->answer_packet,
                               result->answer_len)) {
            const std::list<cdpi_dns_rr> &answer = dns_decoder.get_answer();
            std::list<cdpi_dns_rr>::const_iterator it_ans;

            for (it_ans = answer.begin(); it_ans != answer.end(); ++it_ans) {
                if (ntohs(it_ans->m_type) == 12 &&
                    ntohs(it_ans->m_class) == 1) {
                    ptr_cdpi_dns_ptr p_ptr = DNS_RDATA_TO_PTR(it_ans->m_rdata);
                    string addr;

                    if (! ptr2addr(it_ans->m_name, addr))
                        break;

                    cout << addr << " " << p_ptr->m_ptr << endl;

                    mongo::BSONObjBuilder b1, b2, b3;
                    mongo::BSONObj doc;
                    mongo::Date_t date2;

                    b1.append("_id", addr);
                    b2.append("fqdn", p_ptr->m_ptr);
                    b3.append("$set", b2.obj());

                    mongo_conn.update("DNSCrawl.servers", b1.obj(), b3.obj());
                }
            }
        }
    }

    ub_resolve_free(result);
}

bool
get_ptr(string addr, char *res, int res_len)
{
    in_addr iaddr;

    if(inet_pton(AF_INET, addr.c_str(), &iaddr) <= 0) {
        return false;
    }

    snprintf(res, res_len, "%u.%u.%u.%u.in-addr.arpa",
             (unsigned)((uint8_t*)&iaddr)[3], (unsigned)((uint8_t*)&iaddr)[2],
             (unsigned)((uint8_t*)&iaddr)[1], (unsigned)((uint8_t*)&iaddr)[0]);

    return true;
}

void
resolve_ptr() {
    int n = 0;
    ub_ctx *ctx;
    volatile int resolving = 0;
    auto_ptr<mongo::DBClientCursor> cur = mongo_conn.query("DNSCrawl.servers",
                                                           mongo::BSONObj());

    ctx = ub_ctx_create();
    if (! ctx) {
        cout << "error: could not open unbound context\n" << endl;
        exit(-1);
    }

    if (dns_server != NULL)
        ub_ctx_set_fwd(ctx, dns_server);

    while (cur->more()) {
        mongo::BSONObj p = cur->next();
        string saddr = p.getStringField("_id");
        char   addr[32];
        int    retval;

        cout << n++ << " " << saddr << endl;

        if (! get_ptr(saddr, addr, sizeof(addr))) {
            continue;
        }

        retval = ub_resolve_async(ctx, addr,
                                  12, // TYPE PTR
                                  1,  // CLASS IN
                                  (void*)&resolving, ubcallback, NULL);

        if (retval != 0) {
            continue;
        }

        resolving++;

        while (resolving >= RESOLVE_MAX) {
            if (ub_poll(ctx)) {
                ub_process(ctx);
            }
            sleep(1);
        }
    }

    ub_wait(ctx);
    ub_process(ctx);

    ub_ctx_delete(ctx);
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

    resolve_ptr();

    return 0;
}
