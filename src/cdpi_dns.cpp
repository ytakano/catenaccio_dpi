#include "cdpi_dns.hpp"

#include <string.h>

#include <arpa/inet.h>

using namespace std;

cdpi_dns::cdpi_dns()
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

    memcpy(&m_header, buf, SIZEOF_DNS_HEADER);

    buf += SIZEOF_DNS_HEADER;
    len -= SIZEOF_DNS_HEADER;

    readlen = decode_question(head, total_len,
                              buf, len, ntohs(m_header.m_qd_count));

    if (readlen < 0)
        return false;

    return true;
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

        if (buf_len < 1)
            return -1;

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

                if (pos >= total_len)
                    return -1;

                if (read_domain(head, total_len,
                                &head[pos], total_len - pos, domain)) {
                    return -1;
                }

                break;
            } else {
                return -1;
            }
        } else {
            if (dlen == 0)
                break;

            buf++;
            buf_len--;
            readlen++;

            if (buf_len < (int)dlen)
                return -1;

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
