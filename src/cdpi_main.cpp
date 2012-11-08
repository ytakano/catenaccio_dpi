#include "cdpi_divert.hpp"

#include <iostream>

using namespace std;

class cb_ipv4 : public cdpi_callback {
public:
    cb_ipv4() { }
    virtual ~cb_ipv4() { }

    virtual void operator() (uint8_t *bytes, size_t len) {
    }
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
