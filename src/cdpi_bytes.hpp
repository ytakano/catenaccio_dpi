#ifndef CDPI_BYTES_HPP
#define CDPI_BYTES_HPP

#include "cdpi_common.hpp"

#include <string.h>

#include <list>
#include <string>
#include <vector>

#include <boost/shared_array.hpp>

class cdpi_bytes {
public:
    cdpi_bytes() : m_pos(0), m_len(0) { }
    cdpi_bytes(const char *str) { *this = str; }
    cdpi_bytes(boost::shared_array<char> ptr, int len) : m_ptr(ptr),
                                                         m_pos(0),
                                                         m_len(len) { }
    cdpi_bytes(const cdpi_bytes &rhs) { *this = rhs; }

    cdpi_bytes & operator = (const char *str) {
        int len = strlen(str);
        m_ptr = boost::shared_array<char>(new char[len]);
        memcpy(m_ptr.get(), str, len);
        m_len = len;
        m_pos = 0;

        return *this;
    }

    cdpi_bytes & operator = (const cdpi_bytes &rhs) {
        m_ptr = rhs.m_ptr;
        m_len = rhs.m_len;
        m_pos = rhs.m_pos;

        return *this;
    }

    bool operator == (const cdpi_bytes &rhs) const {
        if (m_len != rhs.m_len)
            return false;

        return memcmp(m_ptr.get() + m_pos, rhs.m_ptr.get() + rhs.m_pos,
                      m_len) == 0;
    }

    bool operator < (const cdpi_bytes &rhs) const {
        if (m_len == rhs.m_len)
            return memcmp(m_ptr.get() + m_pos, rhs.m_ptr.get() + rhs.m_pos,
                          m_len) < 0;

        int len = m_len < rhs.m_len ? m_len : rhs.m_len;
        int result;

        result = memcmp(m_ptr.get() + m_pos, rhs.m_ptr.get() + rhs.m_pos, len);
        if (result < 0) {
            return true;
        } else if (result > 0) {
            return false;
        } else {
            return m_len < rhs.m_len;
        }
        
        return false;
    }

    bool operator > (const cdpi_bytes &rhs) const {
        return rhs < *this;
    }

    void fill_zero() {
        memset(m_ptr.get() + m_pos, 0, m_len);
    }

    bool is_zero() {
        boost::shared_array<char> z(new char[m_len - m_pos]);
        memset(z.get(), 0, m_len - m_pos);
        return memcmp(z.get(), m_ptr.get(), m_len - m_pos) == 0 ? true : false;
    }

    void alloc(size_t len) {
        m_ptr = boost::shared_array<char>(new char[len]);

        if (m_ptr.get() == NULL) {
            PERROR();
            exit(-1);
        }

        m_len = len;
    }

    void set_buf(char *buf, int len) {
        m_ptr = boost::shared_array<char>(new char[len]);
        memcpy(m_ptr.get(), buf, len);

        m_len = len;
        m_pos = 0;
    }

    void clear() {
        m_ptr.reset();
        m_pos = 0;
        m_len = 0;
    }

    boost::shared_array<char> m_ptr;
    int m_pos;
    int m_len;
};

int read_bytes_ec(const std::list<cdpi_bytes> &bytes, char *buf, int len,
                  char c);
int read_bytes(std::list<cdpi_bytes> &bytes, char *buf, int len);
int skip_bytes(std::list<cdpi_bytes> &bytes, int len);
int find_char(const char *buf, int len, char c);
void get_digest(cdpi_bytes &md_value, const char *alg, const char *buf,
                unsigned int len);
std::string bin2str(const char *buf, int len);
void print_binary(const char *buf, int len);
void to_lower_str(std::string &str);
void decompress_gzip(const char *buf, int len, std::string &out_buf);
void decompress_zlib(const char *buf, int len, std::string &out_buf);

#endif // CDPI_BYTES_HPP
