#include "cdpi_mongo.hpp"

#include <unistd.h>

#include <boost/regex.hpp>
#include <openssl/x509.h>

#include <sstream>

using namespace std;

string mongo_server("localhost");
static const char *dht_nodes = "DHT.nodes";
static const char *http_requests = "HTTP.requests";
static const char *dns_requests  = "DNS.requests";
static const char *ssl_requests  = "SSL.requests";

static boost::regex regex_http_uri("^http://.+/.*$");

my_event_listener::my_event_listener() : m_is_verbose(false)
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
            // read body if MIME is text/html
            // ptr_cdpi_http p_http = PROTO_TO_HTTP(proto);
            // p_http->add_mime_to_read("text/html");
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
    case CDPI_EVENT_SSL_CLIENT_HELLO:
    case CDPI_EVENT_SSL_SERVER_HELLO:
    case CDPI_EVENT_SSL_CERTIFICATE:
    {
        in_ssl(cev, id_dir, PROTO_TO_SSL(stream.get_proto(id_dir)));
        break;
    }
    default:
        ;
    }
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
                                server->m_content);
            } else if (encoding == "deflate") {
                decompress_zlib(content.m_ptr.get(), content.m_len,
                                server->m_content);
            } else {
                server->m_content = string(content.m_ptr.get(), content.m_len);
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

    if (m_is_verbose)
        cout << doc.toString() << endl;

    m_mongo.insert(http_requests, doc);
}

void
my_event_listener::in_datagram(cdpi_event cev, const cdpi_id_dir &id_dir,
                               ptr_cdpi_proto data)
{
    switch (cev) {
    case CDPI_EVENT_BENCODE:
        // for BitTorrent DHT
        //in_bencode(id_dir, PROTO_TO_BENCODE(data));
        break;
    case CDPI_EVENT_DNS:
        in_dns(id_dir, PROTO_TO_DNS(data));
        break;
    default:
        ;
    }
}

void
my_event_listener::in_ssl(cdpi_event cev, const cdpi_id_dir &id_dir,
                          ptr_cdpi_ssl p_ssl)
{
    std::map<cdpi_id, ssl_info>::iterator it;

    it = m_ssl.find(id_dir.m_id);

    if (it == m_ssl.end()) {
        m_ssl[id_dir.m_id];
        it = m_ssl.find(id_dir.m_id);
    }

    switch (cev) {
    case CDPI_EVENT_SSL_CLIENT_HELLO:
    {
        char ip_src[INET6_ADDRSTRLEN], ip_dst[INET6_ADDRSTRLEN];

        id_dir.get_addr_src(ip_src, sizeof(ip_src));
        id_dir.get_addr_dst(ip_dst, sizeof(ip_dst));

        it->second.m_client      = p_ssl;
        it->second.m_client_ip   = ip_src;
        it->second.m_server_ip   = ip_dst;
        it->second.m_client_port = ntohs(id_dir.get_port_src());
        it->second.m_server_port = ntohs(id_dir.get_port_dst());

        break;
    }
    case CDPI_EVENT_SSL_SERVER_HELLO:
    {
        it->second.m_server = p_ssl;

        if (! it->second.m_server->get_session_id().is_zero()) {
            insert_ssl(it->second);
        }

        break;
    }
    case CDPI_EVENT_SSL_CERTIFICATE:
    {
        insert_ssl(it->second);
        break;
    }
    default:
        ;
    }
}

void
my_event_listener::get_ssl_obj(ptr_cdpi_ssl p_ssl, mongo::BSONObjBuilder &b)
{
    mongo::BSONArrayBuilder cipher, compress;
    cdpi_bytes session_id;

    session_id = p_ssl->get_session_id();

    b.append("unix time", p_ssl->get_gmt_unix_time());
    b.append("random", bin2str((char*)p_ssl->get_random(), 32));

    if (session_id.m_ptr)
        b.append("session id",
                 bin2str(session_id.m_ptr.get(), session_id.m_len));

    const std::list<uint16_t> &cipher_suites = p_ssl->get_cipher_suites();
    std::list<uint16_t>::const_iterator it;

    for (it = cipher_suites.begin(); it != cipher_suites.end(); ++it) {
        std::string c = p_ssl->num_to_cipher(*it);

        if (c.size() == 0) {
            cipher.append(*it);
        } else {
            cipher.append(c);
        }
    }

    if (cipher.arrSize() > 0) {
        b.append("cipher suite", cipher.arr());
    }

    const std::list<uint8_t> &compression_methods = p_ssl->get_compression_methods();
    std::list<uint8_t>::const_iterator it2;
    for (it2 = compression_methods.begin();
         it2 != compression_methods.end(); ++it2) {
        compress.append(p_ssl->num_to_compression(*it2));
    }

    if (compress.arrSize() > 0) {
        b.append("compression method", compress.arr());
    }
}

void
my_event_listener::insert_ssl(ssl_info &info)
{
    mongo::BSONObjBuilder b, b1, b2;

    get_ssl_obj(info.m_client, b1);
    get_ssl_obj(info.m_server, b2);

    b.append("client ip", info.m_client_ip);
    b.append("server ip", info.m_server_ip);
    b.append("client port", info.m_client_port);
    b.append("server port", info.m_server_port);
    b.append("client hello", b1.obj());
    b.append("server hello", b2.obj());

    mongo::BSONArrayBuilder arr;
    get_x509(info, arr);

    mongo::BSONObj obj = b.obj();

    if (m_is_verbose)
        cout << obj.toString() << endl;

    m_mongo.insert(ssl_requests, obj);
}

void
my_event_listener::get_x509(ssl_info &info, mongo::BSONArrayBuilder &arr)
{
    long l;
    ASN1_INTEGER *bs;
    const std::list<cdpi_bytes> &certs = info.m_server->get_certificats();
    std::list<cdpi_bytes>::const_iterator it;

    for (it = certs.begin(); it != certs.end(); ++it) {
        // decode X509 certification
        mongo::BSONObjBuilder b;
        X509 *cert;
        const unsigned char *p = (const unsigned char*)it->m_ptr.get();

        cert = d2i_X509(NULL, &p, it->m_len);

        if (cert == NULL)
            return;

        X509_print_fp(stdout, cert);

        // version
        l = X509_get_version(cert);
        b.append("version", (int)l);

        // serial
        bs = X509_get_serialNumber(cert);
        if (bs->length <= (int)sizeof(int)) {
            l = ASN1_INTEGER_get(bs);
            b.append("serial", (int)l);
        } else {
            b.append("serial", bin2str((char*)bs->data, bs->length));
        }

        // signature
        BIO *mem = BIO_new(BIO_s_mem());
        string sig;

        if (X509_signature_print(mem, cert->sig_alg, cert->signature) > 0) {
            while (! BIO_eof(mem)) {
                char c;
                BIO_read(mem, &c, 1);
                sig += c;
            }

            // erase "    Signature Algorithm: "
            sig.erase(0, 25);
        }

        istringstream is(sig);

        char buf[128];
        is.getline(buf, sizeof(buf));

        BIO_free(mem);
        b.append("signature algorithm", buf);

        string dump;
        while (is) {
            char c;
            is.get(c);

            if (c != ' ' && c != ':' && c != '\n' && c != '\r')
                dump += c;
        }
        b.append("signature", dump);

        // TODO: issuer

        arr.append(b.obj());

        X509_free(cert);
    }

    if (arr.arrSize() > 0)
        cout << arr.arr().toString() << endl;
}

/*
void
X509_print_ex(BIO *bp, X509 *x, unsigned long nmflags, unsigned long cflag)
{
    long l;
    int ret=0,i;
    char *m=NULL,mlch = ' ';
    int nmindent = 0;
    X509_CINF *ci;
    ASN1_INTEGER *bs;
    EVP_PKEY *pkey=NULL;
    const char *neg;

    if((nmflags & XN_FLAG_SEP_MASK) == XN_FLAG_SEP_MULTILINE) {
        mlch = '\n';
        nmindent = 12;
    }

    if(nmflags == X509_FLAG_COMPAT)
        nmindent = 16;

    ci=x->cert_info;
    if(!(cflag & X509_FLAG_NO_HEADER))
    {
        if (BIO_write(bp,"Certificate:\n",13) <= 0) goto err;
        if (BIO_write(bp,"    Data:\n",10) <= 0) goto err;
    }
    if(!(cflag & X509_FLAG_NO_VERSION))
    {
        l=X509_get_version(x);
        if (BIO_printf(bp,"%8sVersion: %lu (0x%lx)\n","",l+1,l) <= 0) goto err;
    }
    if(!(cflag & X509_FLAG_NO_SERIAL))
    {
        if (BIO_write(bp,"        Serial Number:",22) <= 0) goto err;

        bs=X509_get_serialNumber(x);
        if (bs->length <= (int)sizeof(long))
        {
            l=ASN1_INTEGER_get(bs);
            if (bs->type == V_ASN1_NEG_INTEGER)
            {
                l= -l;
                neg="-";
            }
            else
                neg="";
            if (BIO_printf(bp," %s%lu (%s0x%lx)\n",neg,l,neg,l) <= 0)
                goto err;
        }
        else
        {
            neg=(bs->type == V_ASN1_NEG_INTEGER)?" (Negative)":"";
            if (BIO_printf(bp,"\n%12s%s","",neg) <= 0) goto err;

            for (i=0; i<bs->length; i++)
            {
                if (BIO_printf(bp,"%02x%c",bs->data[i],
                               ((i+1 == bs->length)?'\n':':')) <= 0)
                    goto err;
            }
        }

    }

    if(!(cflag & X509_FLAG_NO_SIGNAME))
    {
        if(X509_signature_print(bp, x->sig_alg, NULL) <= 0)
            goto err;
#if 0
        if (BIO_printf(bp,"%8sSignature Algorithm: ","") <= 0) 
            goto err;
        if (i2a_ASN1_OBJECT(bp, ci->signature->algorithm) <= 0)
            goto err;
        if (BIO_puts(bp, "\n") <= 0)
            goto err;
#endif
    }

    if(!(cflag & X509_FLAG_NO_ISSUER))
    {
        if (BIO_printf(bp,"        Issuer:%c",mlch) <= 0) goto err;
        if (X509_NAME_print_ex(bp,X509_get_issuer_name(x),nmindent, nmflags) < 0) goto err;
        if (BIO_write(bp,"\n",1) <= 0) goto err;
    }
    if(!(cflag & X509_FLAG_NO_VALIDITY))
    {
        if (BIO_write(bp,"        Validity\n",17) <= 0) goto err;
        if (BIO_write(bp,"            Not Before: ",24) <= 0) goto err;
        if (!ASN1_TIME_print(bp,X509_get_notBefore(x))) goto err;
        if (BIO_write(bp,"\n            Not After : ",25) <= 0) goto err;
        if (!ASN1_TIME_print(bp,X509_get_notAfter(x))) goto err;
        if (BIO_write(bp,"\n",1) <= 0) goto err;
    }
    if(!(cflag & X509_FLAG_NO_SUBJECT))
    {
        if (BIO_printf(bp,"        Subject:%c",mlch) <= 0) goto err;
        if (X509_NAME_print_ex(bp,X509_get_subject_name(x),nmindent, nmflags) < 0) goto err;
        if (BIO_write(bp,"\n",1) <= 0) goto err;
    }
    if(!(cflag & X509_FLAG_NO_PUBKEY))
    {
        if (BIO_write(bp,"        Subject Public Key Info:\n",33) <= 0)
            goto err;
        if (BIO_printf(bp,"%12sPublic Key Algorithm: ","") <= 0)
            goto err;
        if (i2a_ASN1_OBJECT(bp, ci->key->algor->algorithm) <= 0)
            goto err;
        if (BIO_puts(bp, "\n") <= 0)
            goto err;

        pkey=X509_get_pubkey(x);
        if (pkey == NULL)
        {
            BIO_printf(bp,"%12sUnable to load Public Key\n","");
            ERR_print_errors(bp);
        }
        else
        {
            EVP_PKEY_print_public(bp, pkey, 16, NULL);
            EVP_PKEY_free(pkey);
        }
    }

    if (!(cflag & X509_FLAG_NO_EXTENSIONS))
        X509V3_extensions_print(bp, "X509v3 extensions",
                                ci->extensions, cflag, 8);

    if(!(cflag & X509_FLAG_NO_SIGDUMP))
    {
        if(X509_signature_print(bp, x->sig_alg, x->signature) <= 0) goto err;
    }
    if(!(cflag & X509_FLAG_NO_AUX))
    {
        if (!X509_CERT_AUX_print(bp, x->aux, 0)) goto err;
    }
    ret=1;
err:
    if (m != NULL) OPENSSL_free(m);
    return(ret);
}
*/

void
my_event_listener::in_dns(const cdpi_id_dir &id_dir, ptr_cdpi_dns p_dns)
{
    mongo::BSONArrayBuilder qarr;
    mongo::BSONArrayBuilder ans_arr;
    mongo::BSONArrayBuilder auth_arr;
    mongo::BSONArrayBuilder add_arr;
    mongo::BSONObjBuilder b;
    mongo::Date_t         date;

    char ip_src[128], ip_dst[128];

    id_dir.get_addr_src(ip_src, sizeof(ip_src));
    id_dir.get_addr_dst(ip_dst, sizeof(ip_dst));

    get_epoch_millis(date);

    b.append("ip src", ip_src);
    b.append("ip dst", ip_dst);
    b.append("port src", ntohs(id_dir.get_port_src()));
    b.append("port dst", ntohs(id_dir.get_port_dst()));
    b.append("date", date);
    b.append("id", ntohs(p_dns->get_id()));
    b.append("opcode", p_dns->get_opcode());
    b.append("rcode", p_dns->get_rcode());

    if (p_dns->is_qr())
        b.append("is_qr", true);

    if (p_dns->is_aa())
        b.append("is_aa", true);

    if (p_dns->is_tc())
        b.append("is_tc", true);

    if (p_dns->is_rd())
        b.append("is_rd", true);

    if (p_dns->is_ra())
        b.append("is_ra", true);

    if (p_dns->is_ad())
        b.append("is_ad", true);

    if (p_dns->is_cd())
        b.append("is_cd", true);


    // query
    std::list<cdpi_dns_question>::const_iterator it_q;

    for (it_q = p_dns->get_question().begin();
         it_q != p_dns->get_question().end(); ++it_q) {
        mongo::BSONObjBuilder b1;

        b1.append("qname", it_q->m_qname + ".");
        b1.append("qtype", ntohs(it_q->m_qtype));
        b1.append("qclass", ntohs(it_q->m_qclass));

        qarr.append(b1.obj());
    }

    b.append("query", qarr.arr());

    // answer
    dns_rr(p_dns->get_answer(), ans_arr);
    if (ans_arr.arrSize() > 0)
        b.append("answer", ans_arr.arr());

    // authority
    dns_rr(p_dns->get_authority(), auth_arr);
    if (auth_arr.arrSize() > 0)
        b.append("authority", auth_arr.arr());

    // additional
    dns_rr(p_dns->get_additional(), add_arr);
    if (add_arr.arrSize() > 0)
        b.append("additional", add_arr.arr());

    mongo::BSONObj obj = b.obj();

    if (m_is_verbose)
        cout << obj.toString() << endl;

    m_mongo.insert(dns_requests, obj);
}

void
my_event_listener::dns_rr(const std::list<cdpi_dns_rr> &rr,
                          mongo::BSONArrayBuilder &arr)
{
    std::list<cdpi_dns_rr>::const_iterator it;

    for (it = rr.begin(); it != rr.end(); ++it) {
        mongo::BSONObjBuilder b;

        b.append("name", it->m_name + ".");
        b.append("type", ntohs(it->m_type));
        b.append("class", ntohs(it->m_class));
        b.append("ttl", ntohl(it->m_ttl));

        switch (ntohs(it->m_type)) {
        case DNS_TYPE_A:
        {
            ptr_cdpi_dns_a p_a = DNS_RDATA_TO_A(it->m_rdata);
            char addr[32];

            inet_ntop(PF_INET, &p_a->m_a, addr, sizeof(addr));

            b.append("a", addr);
            break;
        }
        case DNS_TYPE_AAAA:
        {
            ptr_cdpi_dns_aaaa p_aaaa = DNS_RDATA_TO_AAAA(it->m_rdata);
            char addr[INET6_ADDRSTRLEN];

            inet_ntop(PF_INET6, &p_aaaa->m_aaaa, addr, sizeof(addr));

            b.append("aaaa", addr);
            break;
        }
        case DNS_TYPE_PTR:
        {
            ptr_cdpi_dns_ptr p_ptr = DNS_RDATA_TO_PTR(it->m_rdata);
            b.append("ptr", p_ptr->m_ptr);
            break;
        }
        case DNS_TYPE_NS:
        {
            ptr_cdpi_dns_ns p_ns = DNS_RDATA_TO_NS(it->m_rdata);
            b.append("ns", p_ns->m_ns);
            break;
        }
        case DNS_TYPE_CNAME:
        {
            ptr_cdpi_dns_cname p_cname = DNS_RDATA_TO_CNAME(it->m_rdata);
            b.append("cname", p_cname->m_cname);
            break;
        }
        case DNS_TYPE_TXT:
        {
            ptr_cdpi_dns_txt p_txt = DNS_RDATA_TO_TXT(it->m_rdata);
            b.append("txt", p_txt->m_txt);
            break;
        }
        case DNS_TYPE_MX:
        {
            ptr_cdpi_dns_mx p_mx = DNS_RDATA_TO_MX(it->m_rdata);
            b.append("preference", ntohs(p_mx->m_preference));
            b.append("exchange", p_mx->m_exchange);
            break;
        }
        case DNS_TYPE_HINFO:
        {
            ptr_cdpi_dns_hinfo p_hinfo = DNS_RDATA_TO_HINFO(it->m_rdata);
            b.append("cpu", p_hinfo->m_cpu);
            b.append("os", p_hinfo->m_os);
            break;
        }
        case DNS_TYPE_SOA:
        {
            ptr_cdpi_dns_soa p_soa = DNS_RDATA_TO_SOA(it->m_rdata);
            b.append("mname", p_soa->m_mname);
            b.append("rname", p_soa->m_rname);
            b.append("serial", ntohl(p_soa->m_serial));
            b.append("refresh", ntohl(p_soa->m_refresh));
            b.append("retry", ntohl(p_soa->m_retry));
            b.append("expire", ntohl(p_soa->m_expire));
            b.append("minumum", ntohl(p_soa->m_minimum));
            break;
        }
        case DNS_TYPE_DNSKEY:
        {
            // TODO
            ptr_cdpi_dns_dnskey p_dnskey = DNS_RDATA_TO_DNSKEY(it->m_rdata);
            break;
        }
        default:
            ;
        }

        arr.append(b.obj());
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
        m_tcp.erase(it);
        m_http.erase(id_dir.m_id);
        m_ssl.erase(id_dir.m_id);
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
    string dev;

#ifdef USE_DIVERT
    int  dvt_port = 100;
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
