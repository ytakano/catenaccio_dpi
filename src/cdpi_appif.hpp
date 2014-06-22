#ifndef CDPI_APPIF_HPP
#define CDPI_APPIF_HPP

#include "cdpi_id.hpp"

#include <event.h>

#include <sys/time.h>

#include <map>
#include <set>
#include <string>
#include <deque>

#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

enum cdpi_stream_event {
    // abstraction events
    STREAM_CREATED   = 0,
    STREAM_DESTROYED = 1,
    STREAM_DATA      = 2,

    // primitive event
    STREAM_SYN,
    STREAM_FIN,
    STREAM_TIMEOUT,
    STREAM_RST,
};

static const int DATAGRAM_DATA = STREAM_DATA; // SYNONYM

class cdpi_callback;
class cdpi_tcp;

class cdpi_appif {
public:
    cdpi_appif(cdpi_callback &callback, cdpi_tcp &tcp);
    virtual ~cdpi_appif();

    void read_conf(std::string conf);
    void run();
    void ux_listen();
    void consume_event();

    void in_event(cdpi_stream_event st_event,
                  const cdpi_id_dir &id_dir, cdpi_bytes bytes);
    void in_datagram(const cdpi_id_dir &id_dir, cdpi_bytes bytes);

private:
    typedef boost::shared_ptr<boost::regex> ptr_regex;
    typedef boost::shared_ptr<boost::filesystem::path> ptr_path;

    enum ifproto {
        IF_UDP,
        IF_TCP,
        IF_OTHER
    };

    enum ifformat {
        IF_BINARY,
        IF_TEXT
    };

    struct ifrule {
        ptr_regex   m_up, m_down;
        std::string m_name;
        ifproto     m_proto;
        ifformat    m_format;
        ptr_path    m_ux;
        bool        m_is_body;
        std::list<std::pair<uint16_t, uint16_t> > m_port;

        ifrule() : m_proto(IF_OTHER), m_format(IF_BINARY), m_is_body(true) { }
    };

    struct uxpeer {
        int          m_fd;
        event       *m_ev;
        std::string  m_name;
    };

    typedef boost::shared_ptr<ifrule>        ptr_ifrule;

    enum match_dir {
        MATCH_UP   = 0,
        MATCH_DOWN = 1,
        MATCH_NONE = 2,
    };

    struct loopback_state {
        bool is_header;
        cdpi_appif_header header;
        cdpi_id_dir id_dir;
        std::set<cdpi_id> streams;

        loopback_state() : is_header(true) {
        }
    };

    struct stream_info {
        ptr_ifrule m_ifrule;
        timeval    m_create_time;
        uint64_t   m_dsize1, m_dsize2;
        bool       m_is_created;         // sent created event?
        bool       m_is_buf1, m_is_buf2; // recv data?
        std::deque<cdpi_bytes> m_buf1, m_buf2;
        match_dir  m_match_dir[2];
        bool       m_is_giveup;
        cdpi_appif_header m_header;

        stream_info(const cdpi_id &id);
    };

    typedef boost::shared_ptr<uxpeer>         ptr_uxpeer;
    typedef boost::shared_ptr<boost::thread>  ptr_thread;
    typedef boost::shared_ptr<loopback_state> ptr_loopback_state;
    typedef boost::shared_ptr<stream_info>    ptr_info;

    struct appif_event {
        cdpi_stream_event st_event;
        cdpi_id_dir       id_dir;
        cdpi_bytes        bytes;
    };

    int m_fd7;
    int m_fd3;

    std::map<int, ptr_loopback_state> m_lb7_state;
    ifformat m_lb7_format;

    std::map<cdpi_id, ptr_info> m_info;

    std::list<ptr_ifrule>     m_ifrule;
    std::map<int, ptr_ifrule> m_fd2ifrule; // listen socket
    std::map<int, ptr_uxpeer> m_fd2uxpeer; // accepted socket
    std::map<std::string, std::set<int> > m_name2uxpeer;

    std::deque<appif_event> m_ev_queue;

    boost::shared_mutex m_rw_mutex;
    boost::mutex        m_mutex;
    boost::condition    m_condition;

    ptr_thread          m_thread_stream;
    ptr_thread          m_thread_listen;

    cdpi_callback &m_callback;
    cdpi_tcp      &m_tcp;

    event_base *m_ev_base;
    ptr_path    m_home;

    void in_stream_event(cdpi_stream_event st_event,
                         const cdpi_id_dir &id_dir, cdpi_bytes bytes);
    void makedir(boost::filesystem::path path);
    bool send_data(ptr_info p_info, cdpi_id_dir id_dir);
    void write_head(int fd, const cdpi_id_dir &id_dir, ifformat format,
                    cdpi_stream_event event, match_dir match, int bodylen,
                    cdpi_appif_header *header = NULL);

    friend void ux_accept(int fd, short events, void *arg);
    friend void ux_read(int fd, short events, void *arg);
    friend void ux_close(int fd, cdpi_appif *appif);
    friend bool read_loopback7(int fd, cdpi_appif *appif);
    friend bool read_loopback3(int fd, cdpi_appif *appif);
};

typedef boost::shared_ptr<cdpi_appif> ptr_cdpi_appif;

#endif // CDPI_APPIF_HPP
