#ifndef CDPI_BENCODE_HPP
#define CDPI_BENCODE_HPP

#include "cdpi_bytes.hpp"

#include <event.h>

#include <iostream>
#include <list>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#define HASH_TYPE "SHA1"

class cdpi_bencode {
public:
    cdpi_bencode();
    virtual ~cdpi_bencode();

    bool decode(std::istream &in);
    void encode(std::ostream &out);
    void encode(evbuffer *buf);

    void clear() { m_data.reset(); }

    boost::shared_array<char> get_info_hash() {
        return m_info_hash;
    }

    enum ben_type {
        DICT,
        LIST,
        INT,
        STR
    };

    class bencode_data {
    public:
        bencode_data(ben_type type) { m_type = type; }
        virtual ~bencode_data() = 0;

        ben_type m_type;
    };

    typedef boost::shared_ptr<bencode_data> p_data;
    typedef boost::shared_array<char> p_char;


    class bencode_str : public bencode_data, public cdpi_bytes {
    public:
        bencode_str() : bencode_data(STR) { }
        virtual ~bencode_str() { }
    };
    
    class bencode_int : public bencode_data {
    public:
        bencode_int() : bencode_data(INT) { }
        virtual ~bencode_int() { }

        int m_int;
    };

    class bencode_dict : public bencode_data {
    public:
        bencode_dict() : bencode_data(DICT) { }
        virtual ~bencode_dict() { }

        void add_int(const char *k, int v) {
            bencode_str  key;
            p_data       val(new bencode_int);
            cdpi_bytes  *str = &key;
            bencode_int *num = dynamic_cast<bencode_int*>(val.get());

            *str = k;
            num->m_int = v;

            m_dict[key] = val;
        }

        void add_str(const char *k, const char *v, int len) {
            bencode_str key;
            p_data      val(new bencode_str);
            cdpi_bytes *str1 = &key;
            cdpi_bytes *str2 = dynamic_cast<bencode_str*>(val.get());
            
            *str1 = k;

            str2->alloc(len);
            memcpy(str2->m_ptr.get(), v, len);

            m_dict[key] = val;
        }

        void add_data(const char *k, p_data v) {
            bencode_str  key;
            cdpi_bytes  *str1 = &key;

            *str1 = k;

            m_dict[key] = v;
        }

        void add_data(const cdpi_bytes &k, p_data v) {
            bencode_str  key;
            cdpi_bytes  *str1 = &key;

            *str1 = k;

            m_dict[key] = v;
        }

        p_data get_data(const char *key, int keylen = -1) {
            std::map<bencode_str, p_data>::iterator it;
            bencode_str  str;
            cdpi_bytes  *p;

            p = (cdpi_bytes*)&str;

            if (keylen >= 0) {
                p->alloc(keylen);
                memcpy(p->m_ptr.get(), key, keylen);
            } else {
                *p = key;
            }

            it = m_dict.find(str);
            if (it == m_dict.end()) {
                return p_data();
            } else {
                return it->second;
            }
        }

        std::map<bencode_str, p_data> m_dict;
    };
    
    class bencode_list : public bencode_data {
    public:
        bencode_list() : bencode_data(LIST) { }
        virtual ~bencode_list() { }

        void push_back(p_data data) {
            m_list.push_back(data);
        }

        std::list<p_data> m_list;
    };

    p_data m_data;
    
private:
    bool decode(std::istream &in,   p_data &data);
    bool dec_dict(std::istream &in, p_data &data);
    bool dec_list(std::istream &in, p_data &data);
    bool dec_int(std::istream &in,  p_data &data);
    bool dec_str(std::istream &in,  p_data &data);

    bool read_str(std::istream &in, p_char str, int &len);

    void encode(std::ostream &out, p_data data);
    void encode_dict(std::ostream &out, p_data data);
    void encode_list(std::ostream &out, p_data data);
    void encode_str(std::ostream &out, p_data data);
    void encode_integer(std::ostream &out, p_data data);

    void encode(evbuffer *buf, p_data data);
    void encode_dict(evbuffer *buf, p_data data);
    void encode_list(evbuffer *buf, p_data data);
    void encode_str(evbuffer *buf, p_data data);
    void encode_integer(evbuffer *buf, p_data data);

    void calc_info_hash(std::istream &in);
    
    std::istream::pos_type m_dict_info_begin;
    std::istream::pos_type m_dict_info_end;

    boost::shared_array<char> m_info_hash;
};

#endif // CDPI_BENCODE_HPP
