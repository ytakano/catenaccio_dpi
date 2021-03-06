#include "cdpi_dns.hpp"

#include <mongo/client/dbclient.h>

#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <set>
#include <string>

#include <unbound.h>

using namespace std;

#define RESOLVE_MAX 4096

static const char *http_soa   = "HTTP.soa";
static char *dns_server = NULL;

mongo::DBClientConnection mongo_conn;

void get_epoch_millis(mongo::Date_t &date)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    date.millis = ((unsigned long long)(tv.tv_sec) * 1000 +
                   (unsigned long long)(tv.tv_usec) / 1000);
}

void
ubcallback(void *mydata, int err, struct ub_result *result)
{
    if (err < 0)
        return;

    int *resolving = (int*)mydata;

    (*resolving)--;

    cout << *resolving << endl;

    if (result->havedata) {
        cdpi_dns dns_decoder;

        if (dns_decoder.decode((char*)result->answer_packet, result->answer_len)) {
            const std::list<cdpi_dns_rr> &answer = dns_decoder.get_answer();
            std::list<cdpi_dns_rr>::const_iterator it_ans;

            for (it_ans = answer.begin(); it_ans != answer.end(); ++it_ans) {
                // TODO: handle CNAME
                if (ntohs(it_ans->m_type) == 6 && ntohs(it_ans->m_class) == 1) {
                    ptr_cdpi_dns_soa p_soa = DNS_RDATA_TO_SOA(it_ans->m_rdata);

                    mongo::BSONObjBuilder b1, b2;
                    mongo::BSONObj obj;
                    mongo::Date_t  date;

                    get_epoch_millis(date);

                    b2.append("_id", it_ans->m_name);
                    b2.append("mname", p_soa->m_mname);
                    b2.append("rname", p_soa->m_rname);
                    b2.append("serial", p_soa->m_serial);
                    b2.append("refresh", p_soa->m_refresh);
                    b2.append("retry", p_soa->m_retry);
                    b2.append("expire", p_soa->m_expire);
                    b2.append("minimum", p_soa->m_minimum);
                    b2.append("date", date);

                    obj = b2.obj();

                    cout << obj.toString() << endl;

                    b1.append("_id", it_ans->m_name);

                    mongo_conn.update(http_soa, b1.obj(), obj, true);
                }
            }
        }
    }

    ub_resolve_free(result);
}

void
get_domains(set<string> &domains) {
    auto_ptr<mongo::DBClientCursor> cur = mongo_conn.query("HTTP.trunc_hosts",
                                                           mongo::BSONObj());

    while (cur->more()) {
        mongo::BSONObj p = cur->next();
        domains.insert(p.getStringField("value"));
    }
}

void
resolve_soa() {
    set<string> domains;
    set<string>::iterator it;
    ub_ctx *ctx;
    volatile int resolving = 0;

    get_domains(domains);

    ctx = ub_ctx_create();
    if (! ctx) {
        cout << "error: could not open unbound context\n" << endl;
        exit(-1);
    }

    if (dns_server != NULL)
        ub_ctx_set_fwd(ctx, dns_server);

    for (it = domains.begin(); it != domains.end(); ++it) {
        int retval;

        cout << *it << endl;

        retval = ub_resolve_async(ctx, (char*)it->c_str(),
                                  6, // TYPE SOA
                                  1, // CLASS IN
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

    resolve_soa();

    return 0;
}
