#include "cdpi_bytes.hpp"

#include <ctype.h>

using namespace std;

int
read_bytes_ec(const list<cdpi_bytes> &bytes, char *buf, int len, char c)
{
    list<cdpi_bytes>::const_iterator it;
    int read_len = 0;

    for (it = bytes.begin(); it != bytes.end(); ++it) {
        const char *p = it->m_ptr.get() + it->m_pos;

        if (! it->m_ptr)
            continue;

        for (int i = 0; i < it->m_len; i++) {
            if (read_len >= len)
                return read_len;

            buf[read_len] = p[i];

            read_len++;

            if (p[i] == c)
                return read_len;
        }
    }

    return read_len;
}

int
skip_bytes(list<cdpi_bytes> &bytes, int len)
{
    list<cdpi_bytes>::iterator it;
    int skip_len = 0;

    for (it = bytes.begin(); it != bytes.end();) {
        int remain = len - skip_len;

        if (! it->m_ptr)
            continue;

        if (remain < it->m_len) {
            it->m_len -= remain;
            it->m_pos += remain;
            skip_len  += remain;
            break;
        }

        skip_len += it->m_len;

        bytes.erase(it++);
    }

    return skip_len;
}

int
find_char(char *buf, int len, char c)
{
    int n = 0;
    char *end = buf + len;

    for (; buf < end; buf++) {
        if (*buf == c)
            return n;

        n++;
    }

    return -1;
}

int
lower_case(int c)
{
    return tolower(c);
}

void
to_lower_str(string &str)
{
    transform(str.begin(), str.end(), str.begin(), lower_case);
}

