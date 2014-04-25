#ifndef CDPI_CONF_HPP
#define CDPI_CONF_HPP

#include <map>
#include <string>

class cdpi_conf {
public:
    cdpi_conf();
    virtual ~cdpi_conf();

    bool read_conf(std::string conf);

    std::map<std::string, std::map<std::string, std::string> > m_conf;

};

#endif // CDPI_CONF_HPP
