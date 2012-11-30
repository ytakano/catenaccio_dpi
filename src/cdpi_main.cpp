#include "cdpi_divert.hpp"
#include "cdpi_tcp.hpp"
#include "cdpi_pcap.hpp"

#include <unistd.h>

#include <netinet/in.h>

#include <iostream>
#include <string>

using namespace std;

class cb_ipv4 : public cdpi_callback {
public:
    cb_ipv4() { }
    virtual ~cb_ipv4() { }

    virtual void operator() (char *bytes, size_t len) {
        cdpi_direction dir;
        cdpi_id        id;

        dir = id.set_iph(bytes, IPPROTO_IPV4);

        switch (id.get_l4_proto()) {
        case IPPROTO_TCP:
            m_tcp.input_tcp(id, dir, bytes, len);
            break;
        default:
            ;
        }
    }

private:
    cdpi_tcp m_tcp;

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
        cdpi_pcap pcp;

        pcp.set_dev(dev);
        pcp.set_callback_ipv4(boost::shared_ptr<cdpi_callback>(new cb_ipv4));
        pcp.run();
    } else {
        event_base *ev_base = event_base_new();
        cdpi_divert dvt;

        if (!ev_base) {
            cerr << "couldn't new event_base" << endl;
            return -1;
        }

        dvt.set_ev_base(ev_base);
        dvt.set_callback_ipv4(boost::shared_ptr<cdpi_callback>(new cb_ipv4));
        dvt.run(dvt_port, 0);

        event_base_dispatch(ev_base);
    }

    return 0;
}
