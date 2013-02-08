#ifndef CDPI_UDP_HPP
#define CDPI_UDP_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_id.hpp"

#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>

#include <queue>

struct cdpi_udp_data {
    cdpi_id_dir m_id_dir;
    cdpi_bytes  m_bytes;
};

class cdpi_udp {
public:
    cdpi_udp();
    virtual ~cdpi_udp();

    void input_udp(cdpi_id &id, cdpi_direction dir, char *buf, int len);
    void run();

private:
    boost::mutex     m_mutex;
    boost::condition m_condition;
    boost::thread    m_thread;

    std::queue<cdpi_udp_data> m_queue;

    void input_udp4(cdpi_id &id, cdpi_direction dir, char *buf, int len);

};

#endif // CDPI_UDP_HPP
