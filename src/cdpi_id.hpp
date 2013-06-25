#ifndef CDPI_ID_HPP
#define CDPI_ID_HPP

#include "cdpi_bytes.hpp"

#include <arpa/inet.h>

#include <stdint.h>
#include <string.h>

#include <boost/shared_ptr.hpp>

struct cdpi_peer {
    union {
        uint32_t b32; // big endian
        uint8_t  b128[16];
    } l3_addr;

    uint16_t l4_port; // big endian
    uint16_t padding;

    cdpi_peer() { memset(this, 0, sizeof(*this)); }
    cdpi_peer(const cdpi_peer &rhs) { *this = rhs; }

    bool operator< (const cdpi_peer &rhs) const {
        return memcmp(this, &rhs, sizeof(cdpi_peer)) < 0 ? true : false;
    };

    bool operator> (const cdpi_peer &rhs) const {
        return rhs < *this;
    }

    bool operator== (const cdpi_peer &rhs) const {
        return memcmp(this, &rhs, sizeof(cdpi_peer)) == 0 ? true : false;
    }

    cdpi_peer& operator= (const cdpi_peer &rhs) {
        memcpy(this, &rhs, sizeof(cdpi_peer));
        return *this;
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

    cdpi_direction set_iph(char *iph, int protocol, char **l4hdr);
    void print_id() const;

    bool operator< (const cdpi_id &rhs) const {
        if (m_l3_proto == rhs.m_l3_proto) {
            if (m_l4_proto == rhs.m_l4_proto) {
                int n = memcmp(m_addr1.get(), rhs.m_addr1.get(),
                               sizeof(cdpi_peer));

                if (n == 0)
                    return *m_addr2 < *rhs.m_addr2;

                return n < 0 ? true : false;
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
                *m_addr1 == *rhs.m_addr1 &&
                *m_addr2 == *rhs.m_addr2);
    }

    std::string to_str() const {
        std::string addr1, addr2;

        addr1 = bin2str((char*)m_addr1.get(), sizeof(cdpi_peer));
        addr2 = bin2str((char*)m_addr2.get(), sizeof(cdpi_peer));

        return addr1 + ":" + addr2;
    }

    uint8_t get_l3_proto() const { return m_l3_proto; }
    uint8_t get_l4_proto() const { return m_l4_proto; }

    boost::shared_ptr<cdpi_peer> m_addr1, m_addr2;

private:
    uint8_t m_l3_proto;
    uint8_t m_l4_proto;
};

struct cdpi_id_dir {
    cdpi_id        m_id;
    cdpi_direction m_dir;

    bool operator< (const cdpi_id_dir &rhs) const {
        if (m_dir == rhs.m_dir)
            return m_id < rhs.m_id;

        return m_dir < rhs.m_dir;
    }

    bool operator> (const cdpi_id_dir &rhs) const {
        return rhs < *this;
    }

    bool operator== (const cdpi_id_dir &rhs) const {
        return m_dir == rhs.m_dir && m_id == rhs.m_id;
    }


    void get_addr_src(char *buf, int len) const {
        boost::shared_ptr<cdpi_peer> addr;

        addr = (m_dir == FROM_ADDR1) ? m_id.m_addr1 : m_id.m_addr2;

        get_addr(addr, buf, len);
    }

    void get_addr_dst(char *buf, int len) const {
        boost::shared_ptr<cdpi_peer> addr;

        addr = (m_dir == FROM_ADDR1) ? m_id.m_addr2 : m_id.m_addr1;

        get_addr(addr, buf, len);
    }

    uint32_t get_ipv4_addr_src() const {
        return m_dir == FROM_ADDR1 ?
            m_id.m_addr1->l3_addr.b32 :
            m_id.m_addr2->l3_addr.b32;
    }

    uint32_t get_ipv4_addr_dst() const {
        return m_dir == FROM_ADDR1 ?
            m_id.m_addr2->l3_addr.b32 :
            m_id.m_addr1->l3_addr.b32;
    }

    uint16_t get_port_src() const {
        return m_dir == FROM_ADDR1 ?
            m_id.m_addr1->l4_port :
            m_id.m_addr2->l4_port;
    }

    uint16_t get_port_dst() const {
        return m_dir == FROM_ADDR1 ?
            m_id.m_addr2->l4_port :
            m_id.m_addr1->l4_port;
    }

    uint8_t get_l3_proto() const { return m_id.get_l3_proto(); }
    uint8_t get_l4_proto() const { return m_id.get_l4_proto(); }

private:
    void get_addr(boost::shared_ptr<cdpi_peer> addr, char *buf, int len) const {
        if (m_id.get_l3_proto() == IPPROTO_IP) {
            inet_ntop(AF_INET, &addr->l3_addr.b32, buf, len);
        } else if (m_id.get_l3_proto() == IPPROTO_IPV6) {
            inet_ntop(AF_INET6, addr->l3_addr.b128, buf, len);
        }
    }
};

#endif // CDPI_ID_HPP
