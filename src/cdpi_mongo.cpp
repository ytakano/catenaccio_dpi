#include "cdpi_mongo.hpp"

#include <unistd.h>

#include <boost/regex.hpp>

using namespace std;

string mongo_server("localhost");
static const char *dht_nodes = "DHT.nodes";
static const char *http_requests = "HTTP.requests";

static boost::regex regex_http_uri("^http://.+/.*$");

my_event_listener::my_event_listener()
{
    string errmsg;

    if (! m_mongo.connect(mongo_server, errmsg)) {
        cerr << errmsg << endl;
        exit(-1);
    }

    m_mongo.ensureIndex(http_requests, mongo::fromjson("{uri: 1}"));
    m_mongo.ensureIndex(http_requests, mongo::fromjson("{referer: 1}"));
    m_mongo.ensureIndex(http_requests, mongo::fromjson("{date: 1}"));
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
        ptr_cdpi_proto proto = stream.get_proto(id_dir);

        switch (proto->get_proto_type()) {
        case PROTO_HTTP_SERVER:
        {
            ptr_cdpi_http p_http = PROTO_TO_HTTP(proto);

            p_http->add_mime_to_read("text/html");

            break;
        }
        default:
            ;
        }

        break;
    }
    case CDPI_EVENT_HTTP_READ:
    {
        in_http(id_dir, PROTO_TO_HTTP(stream.get_proto(id_dir)));
        break;
    }
    default:
        ;
    };
}

void
my_event_listener::in_http(const cdpi_id_dir &id_dir, ptr_cdpi_http p_http)
{
    std::map<cdpi_id, http_info>::iterator it;

    it = m_http.find(id_dir.m_id);

    if (it == m_http.end()) {
        m_http[id_dir.m_id];
        it = m_http.find(id_dir.m_id);
    }

    char src[64];
    id_dir.get_addr_src(src, sizeof(src));

    switch (p_http->get_proto_type()) {
    case PROTO_HTTP_CLIENT:
    {
        ptr_http_client_info client(new http_client_info);

        client->m_method = p_http->get_method();
        client->m_ver    = p_http->get_ver();
        client->m_uri    = p_http->get_uri();
        client->m_ip     = src;
        client->m_port   = id_dir.get_port_src();


        // read header
        std::list<std::string> keys;
        std::list<std::string>::iterator it_key;

        p_http->get_header_keys(keys);

        for (it_key = keys.begin(); it_key != keys.end(); ++it_key) {
            client->m_header[*it_key] = p_http->get_header(*it_key);
        }


        // read trailer
        keys.clear();
        p_http->get_trailer_keys(keys);

        for (it_key = keys.begin(); it_key != keys.end(); ++it_key) {
            client->m_header[*it_key] = p_http->get_header(*it_key);
        }


        // push client info
        it->second.m_client.push(client);

        break;
    }
    case PROTO_HTTP_SERVER:
    {
        ptr_http_server_info server(new http_server_info);
        cdpi_bytes content;

        server->m_code = p_http->get_res_code();
        server->m_msg  = p_http->get_res_msg();
        server->m_ver  = p_http->get_ver();
        server->m_ip   = src;
        server->m_port = id_dir.get_port_src();


        // read content
        content = p_http->get_content();

        if (content.m_ptr) {
            string encoding = p_http->get_header("content-encoding");
            if (encoding == "gzip") {
                decompress_gzip(content.m_ptr.get(), content.m_len,
                                server->m_html);
            } else if (encoding == "deflate") {
                decompress_zlib(content.m_ptr.get(), content.m_len,
                                server->m_html);
            } else {
                server->m_html = string(content.m_ptr.get(), content.m_len);
            }
        }


        // read header
        std::list<std::string> keys;
        std::list<std::string>::iterator it_key;

        p_http->get_header_keys(keys);

        for (it_key = keys.begin(); it_key != keys.end(); ++it_key) {
            server->m_header[*it_key] = p_http->get_header(*it_key);
        }


        // read trailer
        keys.clear();
        p_http->get_trailer_keys(keys);

        for (it_key = keys.begin(); it_key != keys.end(); ++it_key) {
            server->m_header[*it_key] = p_http->get_header(*it_key);
        }


        // push server info
        it->second.m_server.push(server);

        break;
    }
    default:
        ;
    }

    while (it->second.m_client.size() > 0 && it->second.m_server.size() > 0) {
        ptr_http_client_info  client = it->second.m_client.front();
        ptr_http_server_info  server = it->second.m_server.front();

        insert_http(client, server);

        it->second.m_client.pop();
        it->second.m_server.pop();
    }
}

string
my_event_listener::get_full_uri(string host, string server_ip, string uri)
{
    if (boost::regex_match(uri, regex_http_uri)) {
        return uri;
    } else if (host.size() == 0) {
        return "http://" + server_ip + uri;
    } else {
        return "http://" + host + uri;
    }
}

void
my_event_listener::insert_http(ptr_http_client_info client,
                               ptr_http_server_info server)
{
    mongo::BSONObjBuilder b, b_h1, b_h2;
    mongo::BSONObj        doc;
    mongo::Date_t         date;
    string full_uri;

    get_epoch_millis(date);

    full_uri = get_full_uri(client->m_header["host"], server->m_ip,
                            client->m_uri);

    map<string, string>::iterator it_header;

    for (it_header = client->m_header.begin();
         it_header != client->m_header.end(); ++it_header) {
        b_h1.append(it_header->first, it_header->second);
    }

    for (it_header = server->m_header.begin();
         it_header != server->m_header.end(); ++it_header) {
        b_h2.append(it_header->first, it_header->second);
    }

    b.append("client header", b_h1.obj());
    b.append("server header", b_h2.obj());
    b.append("client ip", client->m_ip);
    b.append("client port", client->m_port);
    b.append("client ver", client->m_ver);
    b.append("server ip", server->m_ip);
    b.append("server port", server->m_port);
    b.append("server ver", server->m_ver);
    b.append("method", client->m_method);
    b.append("response code", server->m_code);
    b.append("response msg", server->m_msg);
    b.append("date", date);
    b.append("uri", full_uri);


    it_header = client->m_header.find("referer");
    if (it_header != client->m_header.end()) {
        b.append("referer", it_header->second);
    }

    doc = b.obj();

    cout << doc.toString() << endl;

    m_mongo.insert(http_requests, doc);
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

        static const char hex[] = {'0', '1', '2', '3', 
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
        for (int j = 0; j < (int)sizeof(id); j++) {
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
        b1.append("_id", id_str);
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
        m_http.erase(id_dir.m_id);
    }
}


extern char *optarg;
extern int   optind, opterr, optopt;

void
print_usage(char *cmd)
{
#ifdef USE_DIVERT
    cout << "if you want to use divert socket (FreeBSD/MacOS X only), use -d option\n\t"
         << cmd << " -d -4 [divert port for IPv4]\n\n"
         << "if you want to use pcap, use -p option\n\t"
         << cmd << " -p -i [NIF]\n\n"
         << "if you want to speficy IP address and port number of mongoDB, use -m option\n\t"
         << cmd << " -m localhost:27017 -p -i en0"
         << endl;
#else

    cout << "-i option tells a network interface to capture\n\t"
         << cmd << "-i [NIF]\n"
         << "if you want to speficy IP address and port number of mongoDB, use -m option\n\t"
         << cmd << " -m localhost:27017 -i en0"
         << endl;
#endif // USE_DIVERT
}

int
main(int argc, char *argv[])
{
    int opt;
    int dvt_port = 100;
    string dev;

#ifdef USE_DIVERT
    bool is_pcap  = true;
    const char *optstr = "d4:pi:h";
#else
    const char *optstr = "i:h";
#endif // USE_DIVERT

    while ((opt = getopt(argc, argv, optstr)) != -1) {
        switch (opt) {
#ifdef USE_DIVER
        case 'd':
            is_pcap = false;
            break;
        case '4':
            dvt_port = atoi(optarg);
            break;
        case 'p':
            is_pcap = true;
            break;
#endif // USE_DIVERT
        case 'i':
            dev = optarg;
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            return 0;
        }
    }

#ifdef USE_DIVERT
    if (is_pcap) {
        run_pcap<my_event_listener>(dev);
    } else {
        run_divert<my_event_listener>(dvt_port);
    }
#else
    run_pcap<my_event_listener>(dev);
#endif // USE_DIVERT

    return 0;
}
