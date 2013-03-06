#include "cdpi_bencode.hpp"
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

        switch (cev) {
        case CDPI_EVENT_STREAM_OPEN:
            cout << "stream open: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << endl;
            break;
        case CDPI_EVENT_STREAM_CLOSE:
            cout << "stream close: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << endl;
            break;
        case CDPI_EVENT_PROTOCOL_DETECTED:
            break;
        case CDPI_EVENT_HTTP_READ_METHOD:
        {
            p_http = PROTO_TO_HTTP(stream.get_proto(id_dir));

            cout << "HTTP read method: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << endl;

            cout << "\tmethod: " << p_http->get_method()
                 << "\n\turi: " << p_http->get_uri()
                 << "\n\tver: " << p_http->get_ver() << endl;

            break;
        }
        case CDPI_EVENT_HTTP_READ_RESPONSE:
        {
            p_http = PROTO_TO_HTTP(stream.get_proto(id_dir));

            p_http->add_mime_to_read("text/html");

            cout << "HTTP read response: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
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
        {
            p_http = PROTO_TO_HTTP(stream.get_proto(id_dir));

            cout << "HTTP read body: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
                 << endl;

            cdpi_bytes buf;

            buf = p_http->get_content();

            if (buf.m_ptr) {
                string encoding = p_http->get_header("content-encoding");

                if (encoding == "gzip") {
                    vector<char> decompressed;

                    decompress_gzip(buf.m_ptr.get(), buf.m_len, decompressed);

                    cout.write(&decompressed[0], decompressed.size());
                } else if (encoding == "deflate") {
                    vector<char> decompressed;

                    decompress_zlib(buf.m_ptr.get(), buf.m_len, decompressed);

                    cout.write(&decompressed[0], decompressed.size());
                } else {
                    cout.write(buf.m_ptr.get(), buf.m_len);
                }
            }

            break;
        }
        case CDPI_EVENT_HTTP_READ_TRAILER:
        {
            p_http = PROTO_TO_HTTP(stream.get_proto(id_dir));

            cout << "HTTP read trailer: src = " << src << ":" << port_src
                 << ", dst = " << dst << ":" << port_dst
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
                             ptr_cdpi_proto data) {
        uint32_t addr_src = id_dir.get_ipv4_addr_src();
        uint32_t addr_dst = id_dir.get_ipv4_addr_dst();
        uint16_t port_src = ntohs(id_dir.get_port_src());
        uint16_t port_dst = ntohs(id_dir.get_port_dst());
        char     src[32], dst[32];

        inet_ntop(PF_INET, &addr_src, src, sizeof(src));
        inet_ntop(PF_INET, &addr_dst, dst, sizeof(dst));

        switch (cev) {
        case CDPI_EVENT_BENCODE:
        {
            ptr_cdpi_bencode p_ben = PROTO_TO_BENCODE(data);

            cout << "bencode: src = " << src << ":" << port_src 
                 << ", dst = " << dst << ":" << port_dst << endl;
            print_bencode(p_ben->get_data());
            cout << endl;

            break;
        }
        default:
            ;
        }
    }

    void print_bencode(cdpi_bencode::ptr_ben_data data) {
        switch (data->get_type()) {
        case cdpi_bencode::INT:
        {
            cdpi_bencode::ptr_ben_int p_int = BEN_TO_INT(data);
            cout << p_int->m_int;
            break;
        }
        case cdpi_bencode::STR:
        {
            cdpi_bencode::ptr_ben_str p_str = BEN_TO_STR(data);

            cout << "0x";
            print_binary(p_str->m_ptr.get(), p_str->m_len);
            break;
        }
        case cdpi_bencode::LIST:
        {
            cdpi_bencode::ptr_ben_list p_list = BEN_TO_LIST(data);
            list<cdpi_bencode::ptr_ben_data>::iterator it;

            cout << "[";

            for (it = p_list->m_list.begin(); it != p_list->m_list.end();
                 ++it) {
                print_bencode(*it);

                cout << ", ";
            }

            cout << "]";

            break;
        }
        case cdpi_bencode::DICT:
        {
            cdpi_bencode::ptr_ben_dict p_dict = BEN_TO_DICT(data);
            map<cdpi_bencode::bencode_str,
                cdpi_bencode::ptr_ben_data>::iterator it;

            cout << "{";

            for (it = p_dict->m_dict.begin(); it != p_dict->m_dict.end();
                 ++it) {

                cout << "\"";
                cout.write(it->first.m_ptr.get(), it->first.m_len);

                cout << "\": ";

                print_bencode(it->second);

                cout << ", ";
            }

            cout << "}";

            break;
        }
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
