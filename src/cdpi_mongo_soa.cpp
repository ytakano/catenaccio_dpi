#include "cdpi_dns.hpp"

#include <mongo/client/dbclient.h>

#include <stdlib.h>
#include <unistd.h>

#include <set>
#include <string>

#include <unbound.h>

using namespace std;

#define RESOLVE_MAX 4096

static const char *http_soa = "HTTP.soa";

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
    int *resolving = (int*)mydata;

    (*resolving)--;

    cout << *resolving << endl;

    if (result->havedata) {
        cdpi_dns dns_decoder;

        if (dns_decoder.decode((char*)result->answer_packet, result->answer_len)) {
            const std::list<cdpi_dns_rr> &answer = dns_decoder.get_answer();
            std::list<cdpi_dns_rr>::const_iterator it_ans;

            for (it_ans = answer.begin(); it_ans != answer.end(); ++it_ans) {
                if (ntohs(it_ans->m_type) == 6 && ntohs(it_ans->m_class) == 1) {
                    ptr_cdpi_dns_soa p_soa = DNS_RDATA_TO_SOA(it_ans->m_rdata);

                    mongo::BSONObjBuilder b;
                    mongo::BSONObj obj;
                    mongo::Date_t  date;

                    get_epoch_millis(date);

                    b.append("_id", it_ans->m_name);
                    b.append("mname", p_soa->m_mname);
                    b.append("rname", p_soa->m_rname);
                    b.append("serial", p_soa->m_serial);
                    b.append("refresh", p_soa->m_refresh);
                    b.append("retry", p_soa->m_retry);
                    b.append("expire", p_soa->m_expire);
                    b.append("minimum", p_soa->m_minimum);
                    b.append("date", date);

                    obj = b.obj();

                    cout << obj.toString() << endl;

                    mongo_conn.insert(http_soa, obj);
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
print_usage()
{

}

int
main(int argc, char *argv[]) {
    int opt;
    const char *optstr = "m:h";
    const char *mongo_server = "localhost:27017";
    string errmsg;

    while ((opt = getopt(argc, argv, optstr)) != -1) {
        switch (opt) {
        case 'm':
            mongo_server = optarg;
            break;
        case 'h':
        default:
            print_usage();
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
