#ifndef CDPI_APPIF_HPP
#define CDPI_APPIF_HPP

#include "cdpi_id.hpp"

#include <event.h>

#include <string>

#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

enum cdpi_stream_event {
    STREAM_OPEN, // SYN
    STREAM_DATA, // data
    STREAM_FIN,  // FIN
    STREAM_RST,  // RST
    STREAM_TIMEOUT,   // timeout
    STREAM_DESTROYED, // close, RST, timeout
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

    struct ifrule {
        ptr_regex   m_up, m_down;
        std::string m_name;
        ifproto     m_proto;
        ptr_path    m_ux;
        std::list<std::pair<uint16_t, uint16_t> > m_port;

        ifrule() : m_proto(IF_OTHER) { }
    };

    struct uxpeer {
        int          m_fd;
        event       *m_ev;
        std::string  m_name;
    };

    typedef boost::shared_ptr<ifrule>        ptr_ifrule;
    typedef boost::shared_ptr<uxpeer>        ptr_uxpeer;
    typedef boost::shared_ptr<boost::thread> ptr_thread;

    std::list<ptr_ifrule>     m_ifrule;
    std::map<int, ptr_ifrule> m_fd2ifrule;
    std::map<int, ptr_uxpeer> m_fd2uxpeer;
    std::map<std::string, std::set<int> > m_name2uxpeer;

    boost::mutex     m_mutex;
    boost::condition m_condition;
    ptr_thread       m_thread_listen;
    ptr_thread       m_thread_send;

    event_base      *m_ev_base;

    ptr_path m_home;

    void makedir(boost::filesystem::path path);

    friend void ux_accept(int fd, short events, void *arg);
    friend void ux_read(int fd, short events, void *arg);
};

typedef boost::shared_ptr<cdpi_appif> ptr_cdpi_appif;

#endif // CDPI_APPIF_HPP
