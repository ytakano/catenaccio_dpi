#include "cdpi.hpp"

#include <unistd.h>

#include <mongo/client/dbclient.h>

using namespace std;

string mongo_server("localhost");

class my_event_listener : public cdpi_event_listener {
public:
    my_event_listener() {
        string errmsg;

        if (! m_mongo.connect(mongo_server, errmsg)) {
            cerr << errmsg << endl;
            exit(-1);
        }
    }

    virtual ~my_event_listener() {
    }

    virtual void in_stream(cdpi_event cev, const cdpi_id_dir &id_dir,
                           cdpi_stream &stream) {
    }

    virtual void in_datagram(cdpi_event cev, const cdpi_id_dir &id_dir,
                             ptr_cdpi_proto data) {
    }

private:
    mongo::DBClientConnection m_mongo;
};

extern char *optarg;
extern int   optind, opterr, optopt;

void
print_usage(char *cmd)
{
    cout << "if you want to use divert socket (FreeBSD/MacOS X only), use -d option\n\t"
         << cmd << " -d -4 [divert port for IPv4]\n\n"
         << "if you want to use pcap, use -p option\n\t"
         << cmd << " -p -i [NIF]"
         << endl;
}

int
main(int argc, char *argv[])
{
    int opt;
    int dvt_port = 100;
    int is_pcap  = true;
    string dev;
    
    while ((opt = getopt(argc, argv, "d4:pi:hm:")) != -1) {
        switch (opt) {
        case 'd':
            is_pcap = false;
            break;
        case '4':
            dvt_port = atoi(optarg);
            break;
        case 'p':
            is_pcap = true;
            break;
        case 'i':
            dev = optarg;
            break;
        case 'm':
            mongo_server = optarg;
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            return 0;
        }
    }

    if (is_pcap) {
        run_pcap<my_event_listener>(dev);
    } else {
        run_divert<my_event_listener>(dvt_port);
    }

    return 0;
}
