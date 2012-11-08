#ifndef CDPI_ID_HPP
#define CDPI_ID_HPP

#include <boost/shared_ptr.hpp>

struct cdpi_peer {
    union {
        uint32_t b32; // big endian
        uint8_t  b128[16];
    } l3_addr;

    uint32_t l4_port; // big endian

    bool operator< (const cdpi_peer &rhs) const {
        return memcmp(this, &rhs, sizeof(cdpi_peer)) < 0 ? true : false;
    };

    bool operator> (const cdpi_peer &rhs) const {
        return rhs < *this;
    }

    bool operator== (const cdpi_peer &rhs) const {
        return memcmp(this, &rhs, sizeof(cdpi_peer)) == 0 ? true : false;
    }
};

enum cdpi_direction {
    FROM_ADDR1,
    FROM_ADDR2,
    FROM_NONE,
};

class cdpi_id {
public:
    cdpi_id() { }
    virtual ~cdpi_id(){ };

    cdpi_direction set_iph(char *iph, int protocol);

    bool operator< (const cdpi_id &rhs) const {
        if (m_l3_proto == rhs.m_l3_proto) {
            if (m_l4_proto == rhs.m_l4_proto) {
                m_addr1 < rhs.m_addr2;
            }

            return m_l4_proto < rhs.m_l4_proto;
        }

        return m_l3_proto < rhs.m_l3_proto;
    }

    bool operator> (const cdpi_id &rhs) const {
        return rhs < *this;
    }

    bool operator== (const cdpi_id &rhs) const {
        return (m_l3_proto == rhs.m_l3_proto &&
                m_l4_proto == rhs.m_l4_proto &&
                *m_addr1 == *m_addr2);
    }

    uint8_t get_l3_proto() { return m_l3_proto; }
    uint8_t get_l4_proto() { return m_l4_proto; }

private:
    boost::shared_ptr<cdpi_peer> m_addr1, m_addr2;
    uint8_t m_l3_proto;
    uint8_t m_l4_proto;
};

#endif // CDPI_ID_HPP
