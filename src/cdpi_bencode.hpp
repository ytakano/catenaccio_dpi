#ifndef CDPI_BENCODE_HPP
#define CDPI_BENCODE_HPP

#include "cdpi_bytes.hpp"
#include "cdpi_proto.hpp"

#include <event.h>

#include <iostream>
#include <list>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#define HASH_TYPE "SHA1"

#define BEN_TO_STR(PTR) boost::dynamic_pointer_cast<cdpi_bencode::bencode_str>(PTR)
#define BEN_TO_INT(PTR) boost::dynamic_pointer_cast<cdpi_bencode::bencode_int>(PTR)
#define BEN_TO_DICT(PTR) boost::dynamic_pointer_cast<cdpi_bencode::bencode_dict>(PTR)
#define BEN_TO_LIST(PTR) boost::dynamic_pointer_cast<cdpi_bencode::bencode_list>(PTR)

class cdpi_bencode : public cdpi_proto {
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

        ben_type get_type() { return m_type; }

    private:
        ben_type m_type;
    };

    typedef boost::shared_ptr<bencode_data> ptr_ben_data;
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
            ptr_ben_data val(new bencode_int);
            cdpi_bytes  *str = &key;
            bencode_int *num = dynamic_cast<bencode_int*>(val.get());

            *str = k;
            num->m_int = v;

            m_dict[key] = val;
        }

        void add_str(const char *k, const char *v, int len) {
            bencode_str  key;
            ptr_ben_data val(new bencode_str);
            cdpi_bytes  *str1 = &key;
            cdpi_bytes  *str2 = dynamic_cast<bencode_str*>(val.get());
            
            *str1 = k;

            str2->alloc(len);
            memcpy(str2->m_ptr.get(), v, len);

            m_dict[key] = val;
        }

        void add_data(const char *k, ptr_ben_data v) {
            bencode_str  key;
            cdpi_bytes  *str1 = &key;

            *str1 = k;

            m_dict[key] = v;
        }

        void add_data(const cdpi_bytes &k, ptr_ben_data v) {
            bencode_str  key;
            cdpi_bytes  *str1 = &key;

            *str1 = k;

            m_dict[key] = v;
        }

        ptr_ben_data get_data(const char *key, int keylen = -1) {
            std::map<bencode_str, ptr_ben_data>::iterator it;
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
                return ptr_ben_data();
            } else {
                return it->second;
            }
        }

        std::map<bencode_str, ptr_ben_data> m_dict;
    };
    
    class bencode_list : public bencode_data {
    public:
        bencode_list() : bencode_data(LIST) { }
        virtual ~bencode_list() { }

        void push_back(ptr_ben_data data) {
            m_list.push_back(data);
        }

        std::list<ptr_ben_data> m_list;
    };

    ptr_ben_data get_data() { return m_data; }
    void set_data(ptr_ben_data data) { m_data = data; }

    typedef boost::shared_ptr<bencode_int>  ptr_ben_int;
    typedef boost::shared_ptr<bencode_str>  ptr_ben_str;
    typedef boost::shared_ptr<bencode_dict> ptr_ben_dict;
    typedef boost::shared_ptr<bencode_list> ptr_ben_list;
    
private:
    bool decode(std::istream &in,   ptr_ben_data &data);
    bool dec_dict(std::istream &in, ptr_ben_data &data);
    bool dec_list(std::istream &in, ptr_ben_data &data);
    bool dec_int(std::istream &in,  ptr_ben_data &data);
    bool dec_str(std::istream &in,  ptr_ben_data &data);

    bool read_str(std::istream &in, ptr_ben_str ben_str);

    void encode(std::ostream &out, ptr_ben_data data);
    void encode_dict(std::ostream &out, ptr_ben_data data);
    void encode_list(std::ostream &out, ptr_ben_data data);
    void encode_str(std::ostream &out, ptr_ben_data data);
    void encode_integer(std::ostream &out, ptr_ben_data data);

    void encode(evbuffer *buf, ptr_ben_data data);
    void encode_dict(evbuffer *buf, ptr_ben_data data);
    void encode_list(evbuffer *buf, ptr_ben_data data);
    void encode_str(evbuffer *buf, ptr_ben_data data);
    void encode_integer(evbuffer *buf, ptr_ben_data data);

    void calc_info_hash(std::istream &in);
    
    std::istream::pos_type m_dict_info_begin;
    std::istream::pos_type m_dict_info_end;

    boost::shared_array<char> m_info_hash;

    ptr_ben_data m_data;

};

typedef boost::shared_ptr<cdpi_bencode> ptr_cdpi_bencode;

#endif // CDPI_BENCODE_HPP
