#include "cdpi_conf.hpp"
#include "cdpi_bytes.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

cdpi_conf::cdpi_conf()
{

}

cdpi_conf::~cdpi_conf()
{

}

bool
cdpi_conf::read_conf(string conf)
{
    ifstream   ifs(conf);
    string     section;

    enum {
        SECTION,
        KEY_VALUE,
    } state = SECTION;

    while (ifs) {
        int n = 1;
        string line;
        std::getline(ifs, line);

        stringstream s1(line);
        std::getline(s1, line, '#');

        switch (state) {
        case SECTION:
        {
            stringstream s2(line);
            std::getline(s2, line, ':');

            line = trim(line);

            section = line;

            cout << "section: " << line << endl;
            state = KEY_VALUE;

            break;
        }
        case KEY_VALUE:
        {
            stringstream s3(line);
            char c;

            c = (char)s3.peek();

            if (c != ' ') {
                state = SECTION;
            } else {
                for (int i = 0; i < 4; i++) {
                    s3.get(c);

                    if (c != ' ') {
                        cerr << "An error occurred while reading config file \""
                             << conf << ":" << n
                             << "\". The indent must be 4 bytes white space."
                             << endl;
                        return false;
                    }
                }

                string key, value;
                std::getline(s3, key, '=');
                std::getline(s3, value);

                key   = trim(key);
                value = trim(value);

                cout << "key: " << key << ", value: " << value << endl;

                m_conf[section][key] = value;
            }

            break;
        }
        }
    }

    return true;
}
