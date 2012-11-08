#ifndef CDPI_BYTES_HPP
#define CDPI_BYTES_HPP

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

#endif // CDPI_BYTES_HPP
