#ifndef CDPI_MONGO_HPP
#define CDPI_MONGO_HPP

#include "cdpi.hpp"

#include <mongo/client/dbclient.h>

#include <map>
#include <queue>
#include <string>


#include <boost/date_time/posix_time/posix_time.hpp>

class my_event_listener : public cdpi_event_listener {
public:
    my_event_listener();
    virtual ~my_event_listener();

    virtual void in_stream(cdpi_event cev, const cdpi_id_dir &id_dir,
                           cdpi_stream &stream);
    virtual void in_datagram(cdpi_event cev, const cdpi_id_dir &id_dir,
                             ptr_cdpi_proto data);

private:
    typedef boost::posix_time::ptime ptime;

    class tcp_info {
    public:
        bool  m_is_opened;
        int   m_num_open;
        ptime m_time_open;

        tcp_info() : m_num_open(0) { }
    };

    class http_client_info {
    public:
        std::map<std::string, std::string> m_header;
        std::string m_ver;
        std::string m_uri;
        std::string m_method;
        std::string m_ip;
        uint16_t    m_port;
    };

    class http_server_info {
    public:
        std::map<std::string, std::string> m_header;
        std::string m_ver;
        std::string m_code;
        std::string m_msg;
        std::string m_content;
        std::string m_ip;
        uint16_t    m_port;
    };

    typedef boost::shared_ptr<http_client_info> ptr_http_client_info;
    typedef boost::shared_ptr<http_server_info> ptr_http_server_info;

    class http_info {
    public:
        std::queue<ptr_http_client_info> m_client;
        std::queue<ptr_http_server_info> m_server;
    };

    class ssl_info {
    public:
        ptr_cdpi_ssl m_client;
        ptr_cdpi_ssl m_server;
        std::string  m_client_ip;
        std::string  m_server_ip;
        uint16_t     m_client_port;
        uint16_t     m_server_port;
    };

    mongo::DBClientConnection    m_mongo;
    std::map<cdpi_id, tcp_info>  m_tcp;
    std::map<cdpi_id, http_info> m_http;
    std::map<cdpi_id, ssl_info>  m_ssl;

    void open_tcp(const cdpi_id_dir &id_dir);
    void close_tcp(const cdpi_id_dir &id_dir, cdpi_stream &stream);
    void in_bencode(const cdpi_id_dir &id_dir, ptr_cdpi_bencode bc);
    void in_dht_nodes(const cdpi_id_dir &id_dir,
                      cdpi_bencode::ptr_ben_str bstr);
    // DNS
    void in_dns(const cdpi_id_dir &id_dir, ptr_cdpi_dns p_dns);
    void dns_rr(const std::list<cdpi_dns_rr> &rr, mongo::BSONArrayBuilder &arr);

    // HTTP
    void in_http(const cdpi_id_dir &id_dir, ptr_cdpi_http p_http);
    void insert_http(ptr_http_client_info client, ptr_http_server_info server);

    // SSL/TLS
    void in_ssl(cdpi_event cev, const cdpi_id_dir &id_dir, ptr_cdpi_ssl p_ssl);
    void insert_ssl(ssl_info &info);
    void get_ssl_obj(ptr_cdpi_ssl p_ssl, mongo::BSONObjBuilder &b);

    void get_epoch_millis(mongo::Date_t &date);
    std::string get_full_uri(std::string host, std::string server_ip,
                             std::string uri);

};

#endif // CDPI_MONGO_HPP
