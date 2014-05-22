#ifndef CDPI_APPIF_HPP
#define CDPI_APPIF_HPP

#include "cdpi_id.hpp"

#include <event.h>

#include <sys/time.h>

#include <map>
#include <string>
#include <deque>

#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

enum cdpi_stream_event {
    // abstraction events
    STREAM_DESTROYED,
    STREAM_DATA,
    STREAM_CREATED,

    // primitive event
    STREAM_SYN,
    STREAM_FIN,
    STREAM_TIMEOUT,
    STREAM_RST,
};

class cdpi_appif {
public:
    cdpi_appif();
    virtual ~cdpi_appif();

    void read_conf(std::string conf);
    void ux_listen();
    void run();

    void in_stream_event(cdpi_stream_event st_event,
                         const cdpi_id_dir &id_dir, cdpi_bytes bytes);

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
        std::list<std::pair<uint16_t, uint16_t> > m_port;

        ifrule() : m_proto(IF_OTHER), m_format(IF_BINARY) { }
    };

    struct uxpeer {
        int          m_fd;
        event       *m_ev;
        std::string  m_name;
    };

    typedef boost::shared_ptr<ifrule>        ptr_ifrule;

    enum match_dir {
        MATCH_UP,
        MATCH_DOWN,
        MATCH_NONE,
    };

    struct stream_info {
        ptr_ifrule m_ifrule;
        timeval    m_create_time;
        uint64_t   m_dsize1, m_dsize2;
        bool       m_is_created;         // sent created event?
        bool       m_is_buf1, m_is_buf2; // recv data?
        std::deque<cdpi_bytes> m_buf1, m_buf2;
        match_dir  m_buf1_dir, m_buf2_dir;
        bool       m_is_giveup;

        stream_info() : m_dsize1(0), m_dsize2(0), m_is_created(false),
                        m_buf1_dir(MATCH_NONE), m_buf2_dir(MATCH_NONE),
                        m_is_giveup(false) {
            gettimeofday(&m_create_time, NULL);
        }
    };

    typedef boost::shared_ptr<uxpeer>        ptr_uxpeer;
    typedef boost::shared_ptr<boost::thread> ptr_thread;
    typedef boost::shared_ptr<stream_info>   ptr_info;

    std::map<cdpi_id, ptr_info> m_info;

    std::list<ptr_ifrule>     m_ifrule;
    std::map<int, ptr_ifrule> m_fd2ifrule; // listen socket
    std::map<int, ptr_uxpeer> m_fd2uxpeer; // accepted socket
    std::map<std::string, std::set<int> > m_name2uxpeer;

    boost::mutex     m_mutex;
    boost::condition m_condition;
    ptr_thread       m_thread_listen;
    ptr_thread       m_thread_send;

    event_base      *m_ev_base;

    ptr_path m_home;

    void makedir(boost::filesystem::path path);
    void send_data(ptr_info p_info, cdpi_id_dir id_dir);
    void write_head(int fd, const cdpi_id_dir &id_dir, ifformat format,
                    cdpi_stream_event event, int bodylen = 0);

    friend void ux_accept(int fd, short events, void *arg);
    friend void ux_read(int fd, short events, void *arg);
};

typedef boost::shared_ptr<cdpi_appif> ptr_cdpi_appif;

#endif // CDPI_APPIF_HPP
