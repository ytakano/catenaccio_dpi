#include "cdpi_mongo.hpp"

#include <unistd.h>

using namespace std;

string mongo_server("localhost");

my_event_listener::my_event_listener()
{
    string errmsg;

    if (! m_mongo.connect(mongo_server, errmsg)) {
        cerr << errmsg << endl;
        exit(-1);
    }
}

my_event_listener::~my_event_listener()
{

}

void
my_event_listener::in_stream(cdpi_event cev, const cdpi_id_dir &id_dir,
                            cdpi_stream &stream)
{
    switch (cev) {
    case CDPI_EVENT_STREAM_OPEN:
        open_tcp(id_dir);
        break;
    case CDPI_EVENT_STREAM_CLOSE:
        close_tcp(id_dir, stream);
        break;
    case CDPI_EVENT_PROTOCOL_DETECTED:
    {
        break;
    }
    case CDPI_EVENT_HTTP_READ:
    {
        break;
    }
    default:
        ;
    };
}

void
my_event_listener::in_datagram(cdpi_event cev, const cdpi_id_dir &id_dir,
                              ptr_cdpi_proto data)
{

}

void
my_event_listener::open_tcp(cdpi_id_dir id_dir)
{
    map<cdpi_id, tcp_info>::iterator it;

    it = m_tcp.find(id_dir.m_id);
    if (it == m_tcp.end()) {
        m_tcp[id_dir.m_id].m_num_open++;
        it = m_tcp.find(id_dir.m_id);
    } else {
        it->second.m_num_open++;
    }

    if (it->second.m_num_open == 2)
        it->second.m_is_opened = true;
}

void
my_event_listener::close_tcp(cdpi_id_dir id_dir, cdpi_stream &stream)
{
    map<cdpi_id, tcp_info>::iterator it;

    it = m_tcp.find(id_dir.m_id);
    if (it == m_tcp.end())
        return;

    it->second.m_num_open--;

    if (it->second.m_num_open == 0) {
        // TODO: add to mongoDB

        m_tcp.erase(it);
    }
}


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
