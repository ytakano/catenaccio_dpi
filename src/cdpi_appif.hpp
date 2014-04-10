#ifndef CDPI_APPIF_HPP
#define CDPI_APPIF_HPP

#include <event.h>

#include <string>

#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>

class cdpi_appif {
public:
    cdpi_appif();
    virtual ~cdpi_appif();

    void read_conf(std::string conf);
    void ux_listen();

private:
    typedef boost::shared_ptr<boost::regex> ptr_regex;

    struct ifrule {
        ptr_regex   m_up, m_down;
        std::string m_name;
        std::string m_ux;
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

    void run();


    friend void ux_accept(int fd, short events, void *arg);
    friend void ux_read(int fd, short events, void *arg);
};

#endif // CDPI_APPIF_HPP
