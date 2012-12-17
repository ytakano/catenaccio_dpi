#ifndef CDPI_BYTES_HPP
#define CDPI_BYTES_HPP

#include <string.h>

#include <list>
#include <string>

#include <boost/shared_array.hpp>

class cdpi_bytes {
public:
    void set_buf(char *buf, int len) {
        m_ptr = boost::shared_array<char>(new char[len]);
        memcpy(m_ptr.get(), buf, len);

        m_len = len;
        m_pos = 0;
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
void print_binary(const char *buf, int len);

void to_lower_str(std::string &str);

#endif // CDPI_BYTES_HPP
