#include "cdpi_mongo.hpp"

#include <unistd.h>

using namespace std;

string mongo_server("localhost");
const char *dht_nodes = "DHT.nodes";

my_event_listener::my_event_listener()
{
    string errmsg;

    if (! m_mongo.connect(mongo_server, errmsg)) {
        cerr << errmsg << endl;
        exit(-1);
    }

    m_mongo.ensureIndex(dht_nodes,
                        mongo::fromjson("{ID: 1}"), true);
}

my_event_listener::~my_event_listener()
{

}

void
my_event_listener::get_epoch_millis(mongo::Date_t &date)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    date.millis = ((unsigned long long)(tv.tv_sec) * 1000 +
                   (unsigned long long)(tv.tv_usec) / 1000);
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
    switch (cev) {
    case CDPI_EVENT_BENCODE:
        in_bencode(id_dir, PROTO_TO_BENCODE(data));
        break;
    default:
        ;
    }
}

void
my_event_listener::in_bencode(const cdpi_id_dir &id_dir, ptr_cdpi_bencode bc)
{
    cdpi_bencode::ptr_ben_data data = bc->get_data();
    cdpi_bencode::ptr_ben_dict dict;
    cdpi_bencode::ptr_ben_str  y_val;

    if (! data || data->get_type() != cdpi_bencode::DICT)
        return;

    dict = BEN_TO_DICT(data);

    data = dict->get_data("y", 1);

    if (! data || data->get_type() != cdpi_bencode::STR)
        return;

    y_val = BEN_TO_STR(data);

    switch (y_val->m_ptr[0]) {
    case 'r': // response
    {
        cdpi_bencode::ptr_ben_dict r_val;
        cdpi_bencode::ptr_ben_str  nodes_val; 

        // get response dictionary
        data = dict->get_data("r", 1);

        if (! data || data->get_type() != cdpi_bencode::DICT)
            return;

        r_val = BEN_TO_DICT(data);


        // get nodes
        data = r_val->get_data("nodes", 5);

        if (! data || data->get_type() != cdpi_bencode::STR)
            return;

        nodes_val = BEN_TO_STR(data);

        in_dht_nodes(id_dir, nodes_val);

        break;
    }
    case 'q': // query
    case 'e': // error
    default:
        ;
    }
}

void
my_event_listener::in_dht_nodes(const cdpi_id_dir &id_dir,
                                cdpi_bencode::ptr_ben_str bstr)
{
    int   len   = bstr->m_len;
    char *nodes = bstr->m_ptr.get();

    for (int i = 0; i < len / 26; i++) {
        uint32_t ip;
        uint16_t port;
        char     id[20];
        char     id_str[41];
        char     ip_str[16];
        char     port_str[8];

        const char hex[] = {'0', '1', '2', '3', 
                            '4', '5', '6', '7',
                            '8', '9', 'a', 'b',
                            'c', 'd', 'e', 'f'};

        memcpy(id, nodes, sizeof(id));
        nodes += sizeof(id);

        memcpy(&ip, nodes, sizeof(ip));
        nodes += sizeof(ip);

        memcpy(&port, nodes, sizeof(port));
        nodes += sizeof(port);

        id_str[40] = '\0';
        for (int j = 0; j < sizeof(id); j++) {
            id_str[j * 2]     = hex[(id[j] >> 4) & 0x0f];
            id_str[j * 2 + 1] = hex[id[j] & 0x0f];
        }

        inet_ntop(AF_INET, &ip, ip_str, sizeof(ip_str));

        // add to mongoDB

        mongo::BSONObjBuilder b1, b2, b3;
        mongo::BSONObj doc1, doc2, doc3;
        mongo::Date_t  date;

        get_epoch_millis(date);

        // insert ID
        b1.append("ID", id_str);
        doc1 = b1.obj();

        cout << doc1.toString() << endl;

        m_mongo.insert(dht_nodes, doc1);

        // update ip, port, date
        b2.append("ip", ip_str);
        b2.append("port", port);
        b2.append("date", date);
        doc2 = b2.obj();

        b3.append("$set", doc2);
        doc3 = b3.obj();

        cout << doc3.toString() << endl;

        m_mongo.update(dht_nodes, doc1, doc3);
    }
}

void
my_event_listener::open_tcp(const cdpi_id_dir &id_dir)
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
my_event_listener::close_tcp(const cdpi_id_dir &id_dir, cdpi_stream &stream)
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
         << cmd << " -p -i [NIF]\n\n"
         << "if you want to speficy IP address and port number of mongoDB, use -m option\n\t"
         << cmd << " -m localhost:27017 -p -i en0"
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
