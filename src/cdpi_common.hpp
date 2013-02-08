#ifndef CDPI_COMMON_HPP
#define CDPI_COMMON_HPP

#define PERROR() do {                                           \
    char s[256];                                                \
    snprintf(s, sizeof(s), "%s:%d", __FILE__, __LINE__);        \
    perror(s);                                                  \
} while (false)

#endif // CDPI_COMMON_HPP
