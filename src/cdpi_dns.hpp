#ifndef CDPI_DNS_HPP
#define CDPI_DNS_HPP

/*
 * http://www.ietf.org/rfc/rfc1035.txt
 * http://www.ietf.org/rfc/rfc3596.txt
 *
 * Format Overview
 *  +---------------------+
 *  |        Header       |
 *  +---------------------+
 *  |       Question      | the question for the name server
 *  +---------------------+
 *  |        Answer       | RRs answering the question
 *  +---------------------+
 *  |      Authority      | RRs pointing toward an authority
 *  +---------------------+
 *  |      Additional     | RRs holding additional information
 *  +---------------------+
 *
 * Header section format
 *                                  1  1  1  1  1  1
 *    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                      ID                       |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    QDCOUNT                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    ANCOUNT                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    NSCOUNT                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    ARCOUNT                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * Question section format
 *                                  1  1  1  1  1  1
 *    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                                               |
 *  /                     QNAME                     /
 *  /                                               /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                     QTYPE                     |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                     QCLASS                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * DNS Resource Record Format
 *                                  1  1  1  1  1  1
 *    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                                               |
 *  /                                               /
 *  /                      NAME                     /
 *  |                                               |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                      TYPE                     |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                     CLASS                     |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                      TTL                      |
 *  |                                               |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                   RDLENGTH                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
 *  /                     RDATA                     /
 *  /                                               /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * CNAME RDATA format
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  /                     CNAME                     /
 *  /                                               /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * HINFO RDATA format
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  /                      CPU                      /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  /                       OS                      /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * MX RDATA format
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                  PREFERENCE                   |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  /                   EXCHANGE                    /
 *  /                                               /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * NS RDATA format
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  /                   NSDNAME                     /
 *  /                                               /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * PTR RDATA format
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  /                   PTRDNAME                    /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * SOA RDATA format
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  /                     MNAME                     /
 *  /                                               /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  /                     RNAME                     /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    SERIAL                     |
 *  |                                               |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    REFRESH                    |
 *  |                                               |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                     RETRY                     |
 *  |                                               |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    EXPIRE                     |
 *  |                                               |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    MINIMUM                    |
 *  |                                               |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * TXT RDATA format
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  /                   TXT-DATA                    /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * A RDATA format
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    ADDRESS                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * AAAA RDATA format
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                                               |
 *  |                                               |
 *  |                                               |
 *  |                    ADDRESS                    |
 *  |                                               |
 *  |                                               |
 *  |                                               |
 *  |                                               |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 * WKS RDATA format
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    ADDRESS                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |       PROTOCOL        |                       |
 *  +--+--+--+--+--+--+--+--+                       |
 *  |                                               |
 *  /                   <BIT MAP>                   /
 *  /                                               /
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 */

#include <stdint.h>

#include <list>
#include <string>

#include <boost/shared_ptr.hpp>

#include "cdpi_proto.hpp"

#define SIZEOF_DNS_HEADER 12

enum cdpi_dns_class {
    DNS_CLASS_IN = 1, // the Internet
    DNS_CLASS_CH = 3, // CHAOS
    DNS_CLASS_HS = 4, // Hesiod
};

enum cdpi_dns_type {
    DNS_TYPE_A     = 1,
    DNS_TYPE_NS    = 2,
    DNS_TYPE_MD    = 3,
    DNS_TYPE_MF    = 4,
    DNS_TYPE_CNAME = 5,
    DNS_TYPE_SOA   = 6,
    DNS_TYPE_MB    = 7,
    DNS_TYPE_MG    = 8,
    DNS_TYPE_MR    = 9,
    DNS_TYPE_NULL  = 10,
    DNS_TYPE_WKS   = 11,
    DSN_TYPE_PTR   = 12,
    DNS_TYPE_HINFO = 13,
    DNS_TYPE_MINFO = 14,
    DNS_TYPE_MX    = 15,
    DNS_TYPE_TXT   = 16,
    DNS_TYPE_AAAA  = 28
};

struct cdpi_dns_header {
    uint16_t m_id;
    uint16_t m_flag;
    uint16_t m_qd_count;
    uint16_t m_an_count;
    uint16_t m_ns_count;
    uint16_t m_ar_count;
    uint32_t m_padding; // for alignment
};

struct cdpi_dns_question {
    std::string m_qname;
    uint16_t    m_qtype;
    uint16_t    m_qclass;
};

struct cdpi_dns_rdata { };

typedef boost::shared_ptr<cdpi_dns_rdata> ptr_cdpi_dns_rdata;

// resource record header
struct cdpi_dns_rr {
    std::string m_name;
    uint16_t    m_type;
    uint16_t    m_class;
    uint32_t    m_ttl;
    ptr_cdpi_dns_rdata m_rdata;
};

// data of resource record
struct cdpi_dns_cname : public cdpi_dns_rdata {
    std::string m_cname;
};

struct cdpi_dns_hinfo : public cdpi_dns_rdata {
    std::string m_cpu;
    std::string m_os;
};

struct cdpi_dns_mx : public cdpi_dns_rdata {
    uint16_t    m_preference;
    std::string m_exchange;
};

struct cdpi_dns_ns : public cdpi_dns_rdata {
    std::string m_ns;
};

struct cdpi_dns_ptr : public cdpi_dns_rdata {
    std::string m_ptr;
};

struct cdpi_dns_soa : public cdpi_dns_rdata {
    std::string m_mname;
    std::string m_rname;
    uint32_t    m_serial;
    uint32_t    m_refresh;
    uint32_t    m_retry;
    uint32_t    m_expire;
    uint32_t    m_minumum;
};

struct cdpi_dns_txt : public cdpi_dns_rdata {
    std::string m_txt;
};

struct cdpi_dns_a : public cdpi_dns_rdata {
    uint32_t m_a;
};

struct cdpi_dns_aaaa : public cdpi_dns_rdata {
    char m_aaaa[16];
};

class cdpi_dns : public cdpi_proto {
public:
    cdpi_dns();
    virtual ~cdpi_dns();

    bool decode(char *buf, int len);

private:
    cdpi_dns_header               m_header;
    std::list<cdpi_dns_question>  m_quesiton;
    std::list<cdpi_dns_rr>        m_answer;
    std::list<cdpi_dns_rr>        m_authority;
    std::list<cdpi_dns_rr>        m_additional;

    int decode_question(char *head, int total_len, char *buf, int buf_len,
                        int num);
    int decode_rr(char *head, int total_len, char *buf, int buf_len, int num,
                  std::list<cdpi_dns_rr> &rr_list);
    int read_domain(char *head, int total_len, char* buf, int buf_len,
                    std::string &domain);
};

#endif // CDPI_DNS_HPP
