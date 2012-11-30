#include "cdpi_divert.hpp"
#include "cdpi_tcp.hpp"
#include "cdpi_pcap.hpp"

#include <unistd.h>

#include <netinet/in.h>

#include <iostream>
#include <string>

using namespace std;

class my_event_listener : public cdpi_event_listener {
public:
    virtual void operator() (cdpi_event cev, const cdpi_id_dir &id_dir,
                             cdpi_stream &stream) {
    }
};

extern char *optarg;
extern int optind, opterr, optopt;

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
    
    while ((opt = getopt(argc, argv, "d4:pi:h")) != -1) {
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
        case 'h':
        default:
            print_usage(argv[0]);
            return 0;
        }
    }

    if (is_pcap) {
        boost::shared_ptr<my_event_listener> listener(new my_event_listener);
        run_pcap(dev, boost::dynamic_pointer_cast<cdpi_event_listener>(listener));
    } else {
        boost::shared_ptr<my_event_listener> listener(new my_event_listener);
        run_divert(dvt_port, boost::dynamic_pointer_cast<cdpi_event_listener>(listener));
    }

    return 0;
}
