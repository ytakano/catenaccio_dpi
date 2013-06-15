#include <event.h>
#include <mongo/client/dbclient.h>

using namespace std;

string mongo_server("localhost:27017");
mongo::DBClientConnection mongo_conn;
event_base *ev_base;
event *ev_count;

void
count(evutil_socket_t fd, short what, void *arg)
{
    mongo::BSONObj bsonQry = BSON("collStats" << "nodes");
    mongo::BSONObj ret;

    mongo_conn.runCommand("DHT", bsonQry, ret);

    int count = ret.getIntField("count");

    cout << time(NULL) << " " << count << endl;
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

    timeval tv = {1, 0};
    ev_count = event_new(ev_base, -1, EV_TIMEOUT | EV_PERSIST, count, NULL);
    event_add(ev_count, &tv);
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
