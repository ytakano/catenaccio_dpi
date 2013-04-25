#include "cdpi_dns.hpp"

#include "cdpi_bytes.hpp"

#include <string.h>

#include <arpa/inet.h>

using namespace std;

cdpi_dns::cdpi_dns() : cdpi_proto(PROTO_DNS)
{

}

cdpi_dns::~cdpi_dns()
{

}

bool
cdpi_dns::decode(char *buf, int len)
{
    int   readlen;
    int   total_len = len;
    char *head = buf;

    if (len < SIZEOF_DNS_HEADER)
        return false;

    // read header section
    memcpy(&m_header, buf, SIZEOF_DNS_HEADER);

    buf += SIZEOF_DNS_HEADER;
    len -= SIZEOF_DNS_HEADER;


    // read question section
    readlen = decode_question(head, total_len, buf, len,
                              ntohs(m_header.m_qd_count));

    if (readlen < 0)
        return false;

    buf += readlen;
    len -= readlen;


    // read answer section
    readlen = decode_rr(head, total_len, buf, len, ntohs(m_header.m_an_count),
                        m_answer);

    if (readlen < 0)
        return false;

    buf += readlen;
    len -= readlen;


    // read authority section
    readlen = decode_rr(head, total_len, buf, len, ntohs(m_header.m_ns_count),
                        m_authority);

    if (readlen < 0)
        return false;

    buf += readlen;
    len -= readlen;


    // read additional section
    readlen = decode_rr(head, total_len, buf, len, ntohs(m_header.m_ar_count),
                        m_additional);

    if (readlen < 0)
        return false;

    buf += readlen;
    len -= readlen;


    if (len == 0)
        return true;
    else
        return false;
}

int
cdpi_dns::decode_rr(char *head, int total_len, char *buf, int buf_len, int num,
                    list<cdpi_dns_rr> &rr_list)
{
    int readlen = 0;

    for (int i = 0; i < num; i++) {
        cdpi_dns_rr rr;
        int dlen = read_domain(head, total_len, buf, buf_len, rr.m_name);
        uint16_t rdlen;
        uint16_t type;

        if (dlen < 0)
            return -1;

        buf += dlen;
        buf_len -= dlen;
        readlen += dlen;

        if (buf_len < 10)
            return -1;

        memcpy(&rr.m_type, buf, 2);
        buf +=2;

        memcpy(&rr.m_class, buf, 2);
        buf +=2;

        memcpy(&rr.m_ttl, buf, 4);
        buf += 4;

        memcpy(&rdlen, buf, 2);
        buf += 2;

        buf_len -= 10;
        readlen += 10;

        type  = ntohs(rr.m_type);
        rdlen = ntohs(rdlen);

        if (buf_len < rdlen)
            return -1;

        switch (type) {
        case DNS_TYPE_SOA:
        {
            ptr_cdpi_dns_soa p_soa(new cdpi_dns_soa);
            if(decode_soa(head, total_len, buf, buf_len, p_soa) < 0)
                return -1;

            rr.m_rdata = DNS_TO_RDATA(p_soa);

            break;
        }
        case DNS_TYPE_TXT:
        {
            ptr_cdpi_dns_txt p_txt(new cdpi_dns_txt);

            if (decode_txt(head, total_len, buf, buf_len, p_txt, rdlen) < 0)
                return -1;

            rr.m_rdata = DNS_TO_RDATA(p_txt);

            break;
        }
        case DNS_TYPE_A:
        case DNS_TYPE_NS:
        case DNS_TYPE_MD:
        case DNS_TYPE_MF:
        case DNS_TYPE_CNAME:
        case DNS_TYPE_MB:
        case DNS_TYPE_MG:
        case DNS_TYPE_MR:
        case DNS_TYPE_NULL:
        case DNS_TYPE_WKS:
        case DSN_TYPE_PTR:
        case DNS_TYPE_HINFO:
        case DNS_TYPE_MINFO:
        case DNS_TYPE_MX:
        case DNS_TYPE_AAAA:
        default:
            ;
        }

        rr_list.push_back(rr);

        buf += rdlen;
        buf_len -= rdlen;
        readlen += rdlen;
    }

    return readlen;
}

int
cdpi_dns::decode_question(char *head, int total_len,
                          char *buf, int buf_len, int num)
{
    int readlen = 0;

    for (int i = 0; i < num; i++) {
        cdpi_dns_question question;
        int dlen = read_domain(head, total_len, buf, buf_len, question.m_qname);

        if (dlen < 0)
            return -1;

        buf += dlen;
        buf_len -= dlen;
        readlen += dlen;

        if (buf_len < 4)
            return -1;

        memcpy(&question.m_qtype, buf, 2);
        buf += 2;

        memcpy(&question.m_qclass, buf, 2);
        buf += 2;

        buf_len -= 4;
        readlen += 4;

        m_quesiton.push_back(question);
    }

    return readlen;
}

int
cdpi_dns::read_domain(char *head, int total_len, char* buf, int buf_len,
                      string &domain)
{
    int readlen = 0;

    for (;;) {
        uint16_t pos;
        uint8_t  dlen;
        uint8_t  flag;

        if (buf_len < 1) {
            return -1;
        }

        dlen = (uint8_t)buf[0];

        flag = dlen & 0xC0;

        if (flag) {
            if (flag == 0xC0) {
                if (buf_len < 2)
                    return -1;

                memcpy(&pos, buf, 2);

                pos = ntohs(pos);
                pos &= 0x3FFF;

                readlen += 2;

                if (pos >= total_len) {
                    return -1;
                }

                if (read_domain(head, total_len,
                                &head[pos], total_len - pos, domain) < 0) {
                    return -1;
                }

                break;
            } else {
                return -1;
            }
        } else {
            if (dlen == 0) {
                readlen++;
                break;
            }

            buf++;
            buf_len--;
            readlen++;

            if ((unsigned int)buf_len < dlen) {
                return -1;
            }

            if (domain == "") {
                domain = string(buf, dlen);
            } else {
                domain += "." + string(buf, dlen);
            }

            buf += dlen;
            buf_len -= dlen;
            readlen += dlen;
        }
    }

    return readlen;
}

int
cdpi_dns::decode_txt(char *head, int total_len, char *buf, int buf_len,
                     ptr_cdpi_dns_txt p_txt, uint16_t rdlen)
{
    p_txt->m_txt = string(buf, rdlen);

    return rdlen;
}

int
cdpi_dns::decode_soa(char *head, int total_len, char *buf, int buf_len,
                     ptr_cdpi_dns_soa p_soa)
{
    int readlen = 0;
    int dlen;

    dlen = read_domain(head, total_len, buf, buf_len, p_soa->m_mname);

    if (dlen < 0)
        return -1;

    buf += dlen;
    buf_len -= dlen;
    readlen += dlen;

    dlen = read_domain(head, total_len, buf, buf_len, p_soa->m_rname);

    if (dlen < 0)
        return -1;

    buf += dlen;
    buf_len -= dlen;
    readlen += dlen;

    if ((uint32_t)buf_len < sizeof(p_soa->m_serial))
        return -1;

    memcpy(&p_soa->m_serial, buf, sizeof(p_soa->m_serial));
    buf += sizeof(p_soa->m_serial);
    buf_len -= sizeof(p_soa->m_serial);
    readlen += sizeof(p_soa->m_serial);

    memcpy(&p_soa->m_refresh, buf, sizeof(p_soa->m_refresh));
    buf += sizeof(p_soa->m_refresh);
    buf_len -= sizeof(p_soa->m_refresh);
    readlen += sizeof(p_soa->m_refresh);

    memcpy(&p_soa->m_retry, buf, sizeof(p_soa->m_retry));
    buf += sizeof(p_soa->m_retry);
    buf_len -= sizeof(p_soa->m_retry);
    readlen += sizeof(p_soa->m_retry);

    memcpy(&p_soa->m_expire, buf, sizeof(p_soa->m_expire));
    buf += sizeof(p_soa->m_expire);
    buf_len -= sizeof(p_soa->m_expire);
    readlen += sizeof(p_soa->m_expire);

    memcpy(&p_soa->m_minimum, buf, sizeof(p_soa->m_minimum));
    buf += sizeof(p_soa->m_minimum);
    buf_len -= sizeof(p_soa->m_minimum);
    readlen += sizeof(p_soa->m_minimum);

    return readlen;
}
