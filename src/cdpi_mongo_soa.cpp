#include <mongo/client/dbclient.h>

#include <stdlib.h>
#include <unistd.h>

#include <set>
#include <string>

#include <unbound.h>

using namespace std;

#define RESOLVE_MAX 4096

void
ubcallback(void *mydata, int err, struct ub_result *result)
{
    int *resolving = (int*)mydata;

    (*resolving)--;

    cout << *resolving << endl;

    if (result->havedata) {
        cout << result->qname << ": " << result->len[0] << "[bytes], ";
        // TODO: unpack result
        cout.write(result->data[0], result->len[0]);
        cout << endl;
    }

    ub_resolve_free(result);
}

void
get_domains(string server, set<string> &domains) {
    mongo::DBClientConnection conn;
    string errmsg;

    if (! conn.connect(server, errmsg)) {
        cerr << errmsg << endl;
        exit(-1);
    }

    auto_ptr<mongo::DBClientCursor> cur = conn.query("HTTP.trunc_hosts",
                                                     mongo::BSONObj());

    while (cur->more()) {
        mongo::BSONObj p = cur->next();
        domains.insert(p.getStringField("value"));
    }
}

void
resolve_soa(string server) {
    set<string> domains;
    set<string>::iterator it;
    ub_ctx *ctx;
    volatile int resolving = 0;

    get_domains(server, domains);

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

int
main(int argc, char *argv[]) {
    resolve_soa("localhost:27017");

    return 0;
}
