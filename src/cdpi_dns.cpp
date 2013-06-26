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

        buf     += dlen;
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

            if (decode_txt(buf, p_txt, rdlen) < 0)
                return -1;

            rr.m_rdata = DNS_TO_RDATA(p_txt);

            break;
        }
        case DNS_TYPE_A:
        {
            ptr_cdpi_dns_a p_a(new cdpi_dns_a);

            if (decode_a(buf, p_a, rdlen) < 0)
                return -1;

            rr.m_rdata = DNS_TO_RDATA(p_a);

            break;
        }
        case DNS_TYPE_AAAA:
        {
            ptr_cdpi_dns_aaaa p_aaaa(new cdpi_dns_aaaa);

            if (decode_aaaa(buf, p_aaaa, rdlen) < 0)
                return -1;

            rr.m_rdata = DNS_TO_RDATA(p_aaaa);

            break;
        }
        case DNS_TYPE_NS:
        {
            ptr_cdpi_dns_ns p_ns(new cdpi_dns_ns);
            int dlen;

            dlen = read_domain(head, total_len, buf, buf_len, p_ns->m_ns);

            if (dlen < 0)
                return -1;

            rr.m_rdata = DNS_TO_RDATA(p_ns);

            break;
        }
        case DNS_TYPE_CNAME:
        {
            ptr_cdpi_dns_cname p_cname(new cdpi_dns_cname);
            int dlen;

            dlen = read_domain(head, total_len, buf, buf_len, p_cname->m_cname);

            if (dlen < 0)
                return -1;

            rr.m_rdata = DNS_TO_RDATA(p_cname);

            break;
        }
        case DNS_TYPE_MX:
        {
            ptr_cdpi_dns_mx p_mx(new cdpi_dns_mx);

            if (decode_mx(head, total_len, buf, buf_len, p_mx, rdlen) < 0)
                return -1;

            rr.m_rdata = DNS_TO_RDATA(p_mx);

            break;
        }
        case DSN_TYPE_PTR:
        {
            ptr_cdpi_dns_ptr p_ptr(new cdpi_dns_ptr);
            int dlen;

            dlen = read_domain(head, total_len, buf, buf_len, p_ptr->m_ptr);

            if (dlen < 0)
                return -1;

            rr.m_rdata = DNS_TO_RDATA(p_ptr);

            break;
        }
        case DNS_TYPE_MD:
        case DNS_TYPE_MF:
        case DNS_TYPE_MB:
        case DNS_TYPE_MG:
        case DNS_TYPE_MR:
        case DNS_TYPE_NULL:
        case DNS_TYPE_WKS:
        case DNS_TYPE_HINFO:
        case DNS_TYPE_MINFO:
        default:
            ;
        }

        rr_list.push_back(rr);

        buf     += rdlen;
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
cdpi_dns::decode_a(char *buf, ptr_cdpi_dns_a p_a, uint16_t rdlen)
{
    if (rdlen < sizeof(cdpi_dns_a))
        return rdlen;

    memcpy(&p_a->m_a, buf, 4);

    return rdlen;
}

int
cdpi_dns::decode_aaaa(char *buf, ptr_cdpi_dns_aaaa p_aaaa, uint16_t rdlen)
{
    if (rdlen < sizeof(cdpi_dns_aaaa))
        return rdlen;

    memcpy(&p_aaaa->m_aaaa, buf, 16);

    return rdlen;
}

int
cdpi_dns::decode_hinfo(char *buf, ptr_cdpi_dns_hinfo p_hinfo, uint16_t rdlen)
{
    int readlen = 0;
    uint8_t n;

    // read cpu
    if (rdlen < 1)
        return -1;

    n = (uint8_t)buf[0];

    if (n > rdlen - 1)
        return -1;

    p_hinfo->m_cpu = string(buf + 1, n);

    n++;

    buf     += n;
    rdlen   -= n;
    readlen += n;


    // read os
    if (rdlen < 1)
        return -1;

    n = (uint8_t)buf[0];

    if (n != rdlen - 1)
        return -1;

    p_hinfo->m_os = string(buf + 1, n);

    return readlen + n + 1;
}

int
cdpi_dns::decode_txt(char *buf, ptr_cdpi_dns_txt p_txt, uint16_t rdlen)
{
    uint8_t n;

    if (rdlen <= 1)
        return rdlen;

    n = (uint8_t)buf[0];

    if (n != rdlen - 1)
        return -1;

    p_txt->m_txt = string(buf + 1, n);

    return rdlen;
}

int
cdpi_dns::decode_mx(char *head, int total_len, char *buf, int buf_len,
                    ptr_cdpi_dns_mx p_mx, uint16_t rdlen)
{
    if (rdlen < 2)
        return rdlen;

    memcpy(&p_mx->m_preference, buf, 2);

    int dlen = read_domain(head, total_len, buf, buf_len, p_mx->m_exchange);

    if (dlen < 0)
        return -1;

    return dlen + 2;
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

    buf     += dlen;
    buf_len -= dlen;
    readlen += dlen;

    dlen = read_domain(head, total_len, buf, buf_len, p_soa->m_rname);

    if (dlen < 0)
        return -1;

    buf     += dlen;
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

int
cdpi_dns::get_rcode()
{
    return ntohs(m_header.m_flag) & 0x000f;
}

int
cdpi_dns::get_opcode()
{
    return (ntohs(m_header.m_flag) & (0x000f << 11)) >> 11;
}

bool
cdpi_dns::is_qr()
{
    return (ntohs(m_header.m_flag) & (1 << 15)) != 0 ? true : false;
}

bool
cdpi_dns::is_aa()
{
    return (ntohs(m_header.m_flag) & (1 << 10)) != 0 ? true : false;
}

bool
cdpi_dns::is_tc()
{
    return (ntohs(m_header.m_flag) & (1 << 9)) != 0 ? true : false;
}

bool
cdpi_dns::is_rd()
{
    return (ntohs(m_header.m_flag) & (1 << 8)) != 0 ? true : false;
}

bool
cdpi_dns::is_ra()
{
    return (ntohs(m_header.m_flag) & (1 << 7)) != 0 ? true : false;
}

bool
cdpi_dns::is_ad()
{
    return (ntohs(m_header.m_flag) & (1 << 5)) != 0 ? true : false;
}

bool
cdpi_dns::is_cd()
{
    return (ntohs(m_header.m_flag) & (1 << 4)) != 0 ? true : false;
}
