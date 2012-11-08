#include "cdpi_divert.hpp"
#include "cdpi_tcp.hpp"

#include <iostream>

using namespace std;

class cb_ipv4 : public cdpi_callback {
public:
    cb_ipv4() { }
    virtual ~cb_ipv4() { }

    virtual void operator() (char *bytes, size_t len) {
        cdpi_direction dir;
        cdpi_id        id;

        dir = id.set_iph(bytes, IPPROTO_IPV4);

        switch (id.get_l4_proto()) {
        case IPPROTO_TCP:
            m_tcp.input_tcp(id, dir, bytes, len);
            break;
        default:
            ;
        }
    }

private:
    cdpi_tcp m_tcp;

};

int
main(int argc, char *argv[])
{
    event_base *ev_base = event_base_new();
    cdpi_divert dvt;

    if (!ev_base) {
        cerr << "could'n new event_base" << endl;
        return -1;
    }

    dvt.set_ev_base(ev_base);
    dvt.set_callback_ipv4(boost::shared_ptr<cdpi_callback>(new cb_ipv4));
    dvt.run(100, 200);

    event_base_dispatch(ev_base);

    return 0;
}
