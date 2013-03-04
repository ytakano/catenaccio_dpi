#include "cdpi_divert.hpp"
#include "cdpi_pcap.hpp"
#include "cdpi_http.hpp"

#include <unistd.h>

#include <arpa/inet.h>

#include <netinet/in.h>

#include <iostream>
#include <string>

using namespace std;

class my_event_listener : public cdpi_event_listener {
public:
    virtual void in_stream(cdpi_event cev, const cdpi_id_dir &id_dir,
                           cdpi_stream &stream) {

        
        // How to get IPv4 address and Port number
        
        uint32_t addr_src = id_dir.get_ipv4_addr_src();
        uint32_t addr_dst = id_dir.get_ipv4_addr_dst();
        uint16_t port_src = ntohs(id_dir.get_port_src());
        uint16_t port_dst = ntohs(id_dir.get_port_dst());
        char src[32], dst[32];

        inet_ntop(PF_INET, &addr_src, src, sizeof(src));
        inet_ntop(PF_INET, &addr_dst, dst, sizeof(dst));

        /*
         * // L3 and L4 protocol
         * // 今はIPv4とTCPだけなので必要ない
         *
         * if (id_dir.get_l3_proto() == IPPROTO_IP) {
         * } else if (id_dir.get_l3_proto() == IPPROTO_IPV6) {
         * }
         *
         * if (id_dir.get_l4_proto() == IPPROTO_TCP) {
         * } else if (id_dir.get_l4_proto() == IPPROTO_UDP) {
         * }
         */

        ptr_cdpi_http p_http;
        std::string id = id_dir.m_id.to_str();

        switch (cev) {
        case CDPI_EVENT_STREAM_OPEN:
            cout << "stream open: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << ", id = " << id
                 << endl;
            break;
        case CDPI_EVENT_STREAM_CLOSE:
            cout << "stream close: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << ", id = " << id
                 << endl;
            break;
        case CDPI_EVENT_PROTOCOL_DETECTED:
            break;
        case CDPI_EVENT_HTTP_READ_METHOD:
        {
            p_http = PROTO_TO_HTTP(stream.get_proto(id_dir));

            cout << "HTTP read method: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << ", id = " << id
                 << endl;

            cout << "\tmethod: " << p_http->get_method()
                 << "\n\turi: " << p_http->get_uri()
                 << "\n\tver: " << p_http->get_ver() << endl;

            break;
        }
        case CDPI_EVENT_HTTP_READ_RESPONSE:
        {
            p_http = PROTO_TO_HTTP(stream.get_proto(id_dir));

            cout << "HTTP read response: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << ", id = " << id
                 << endl;

            cout << "\tcode: " << p_http->get_res_code()
                 << "\n\tmsg: " << p_http->get_res_msg()
                 << "\n\tver: " << p_http->get_ver() << endl;

            break;
        }
        case CDPI_EVENT_HTTP_READ_HEAD:
        {
            p_http = PROTO_TO_HTTP(stream.get_proto(id_dir));

            cout << "HTTP read head: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << ", id = " << id
                 << endl;

            std::list<std::string> keys;
            std::list<std::string>::iterator it;

            p_http->get_header_keys(keys);

            for (it = keys.begin(); it != keys.end(); ++it) {
                cout << "\t" << *it << ": " << p_http->get_header(*it) << endl;
            }

            break;
        }
        case CDPI_EVENT_HTTP_READ_BODY:
            break;
        case CDPI_EVENT_HTTP_READ_TRAILER:
        {
            p_http = PROTO_TO_HTTP(stream.get_proto(id_dir));

            cout << "HTTP read trailer: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << ", id = " << id
                 << endl;

            std::list<std::string> keys;
            std::list<std::string>::iterator it;

            p_http->get_trailer_keys(keys);

            for (it = keys.begin(); it != keys.end(); ++it) {
                cout << "\t" << *it << ": " << p_http->get_trailer(*it) << endl;
            }

            break;
        }
        case CDPI_EVENT_HTTP_PROXY:
        {
            p_http = PROTO_TO_HTTP(stream.get_proto(id_dir));

            cout << "HTTP proxy connect: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << ", id = " << id
                 << endl;

            if (p_http->get_proto_type() == PROTO_HTTP_CLIENT) {
                cout << "\tmethod: " << p_http->get_method()
                     << "\n\turi: " << p_http->get_uri()
                     << "\n\tver: " << p_http->get_ver() << endl;
            }

            break;
        }
        default:
            break;
        }
    }

    virtual void in_datagram(cdpi_event cev, const cdpi_id_dir &id_dir,
                             cdpi_proto *data) {
        switch (cev) {
        case CDPI_EVENT_BENCODE:
            break;
        default:
            ;
        }
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
