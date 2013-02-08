#include "cdpi_bencode.hpp"

#include <stdlib.h>

#include <openssl/evp.h>

#include <sstream>

#include <boost/foreach.hpp>
#include <boost/scoped_array.hpp>

using namespace std;

cdpi_bencode::cdpi_bencode()
{

}

cdpi_bencode::~cdpi_bencode()
{

}

cdpi_bencode::bencode_data::~bencode_data()
{

}

bool
cdpi_bencode::decode(istream &in)
{
    if (decode(in, m_data)) {
        return true;
    } else {
        clear();
        return false;
    }
}

bool
cdpi_bencode::decode(istream &in, p_data &data)
{
    char c;

    c = in.peek();

    if (c == 'd') {
        return dec_dict(in, data);
    } else if (c == 'l') {
        return dec_list(in, data);
    } else if (c == 'i') {
        return dec_int(in, data);
    } else if ('0' <= c && c <= '9') {
        return dec_str(in, data);
    }

    return false;
}

bool
cdpi_bencode::dec_dict(istream &in, p_data &data)
{
    istream::pos_type  pos;
    bencode_dict      *dict;
    char               c;

    data = p_data(new bencode_dict);
    dict = dynamic_cast<bencode_dict*>(data.get());

    in.ignore();

    while (! in.eof()) {
        c = in.peek();

        if (c == 'e') {
            in.get();
            return true;
        }

        if (c < '0' || c > '9') {
            cerr << "dec_dict(): key of dictionary isn't string" << endl;
            return false;
        }


        p_data key, value;

        if (! dec_str(in, key)) {
            cerr << "dec_dict(): invalid string format" << endl;
            return false;
        }

        pos = in.tellg();

        if (! decode(in, value))
            return false;


        bencode_str *str;

        str = dynamic_cast<bencode_str*>(key.get());
        dict->m_dict[*str] = value;


        // for info_hash
        if (memcmp(str->m_ptr.get(), "info", 4) == 0) {
            m_dict_info_begin = pos;
            m_dict_info_end   = in.tellg();
            calc_info_hash(in);
        }
    }

    return false;
}

bool
cdpi_bencode::dec_list(istream &in, p_data &data)
{
    bencode_list *ben_list;
    char c;

    in.ignore();

    data = p_data(new bencode_list);

    ben_list = dynamic_cast<bencode_list*>(data.get());

    for (;;) {
        c = in.peek();
        if (c == 'e') {
            in.get();
            return true;
        }

        p_data value;
        if (! decode(in, value))
            return false;

        ben_list->m_list.push_back(value);
    }

    return false;
}

bool
cdpi_bencode::dec_int(istream &in, p_data &data)
{
    bencode_int *ben_int;
    stringbuf    buf;
    char c;

    in.ignore();

    data = p_data(new bencode_int);

    ben_int = dynamic_cast<bencode_int*>(data.get());

    in.get(buf, 'e');

    in.get(c);
    if (c != 'e') {
        cerr << "dec_int(): invalid integer format" << endl;
        return false;
    }

    ben_int->m_int = atoi(buf.str().c_str());

    return true;
}

bool
cdpi_bencode::dec_str(istream &in, p_data &data)
{
    bencode_str *ben_str;

    data = p_data(new bencode_str);

    ben_str = dynamic_cast<bencode_str*>(data.get());

    return read_str(in, ben_str->m_ptr, ben_str->m_len);
}

bool
cdpi_bencode::read_str(istream &in, p_char str, int &len)
{
    stringbuf num;
    char c;

    in.get(num, ':');
    in.get(c);

    if (c != ':')
        return false;

    len = (size_t)atoi(num.str().c_str());


    p_char buf(new char[len + 1]);
    in.read(buf.get(), len);
    if (in.bad())
        return false;

    str = buf;
    str[len] = '\0';

    return true;
}

void
cdpi_bencode::encode(ostream &out)
{
    return encode(out, m_data);
}

void
cdpi_bencode::encode(ostream &out, p_data data)
{
    switch (data->m_type) {
    case DICT:
        encode_dict(out, data);
        break;
    case LIST:
        encode_list(out, data);
        break;
    case INT:
        encode_integer(out, data);
        break;
    case STR:
        encode_str(out, data);
        break;
    }
}

void
cdpi_bencode::encode_dict(ostream &out, p_data data)
{
    bencode_dict *ben_dict = dynamic_cast<bencode_dict*>(data.get());

    out << "d";

    pair<bencode_str, p_data> p;
    BOOST_FOREACH(p, ben_dict->m_dict) {
        out << p.first.m_len << ":";
        out.write(p.first.m_ptr.get(), p.first.m_len);
        encode(out, p.second);
    }

    out << "e";
}

void
cdpi_bencode::encode_list(ostream &out, p_data data)
{
    bencode_list *ben_list = dynamic_cast<bencode_list*>(data.get());

    out << "l";

    BOOST_FOREACH(p_data p, ben_list->m_list) {
        encode(out, p);
    }

    out << "e";
}

void
cdpi_bencode::encode_integer(ostream &out, p_data data)
{
    bencode_int *ben_int = dynamic_cast<bencode_int*>(data.get());

    out << "i" << ben_int->m_int << "e";
}

void
cdpi_bencode::encode_str(ostream &out, p_data data)
{
    bencode_str *ben_str = dynamic_cast<bencode_str*>(data.get());

    out << ben_str->m_len << ":";
    out.write(ben_str->m_ptr.get(), ben_str->m_len);
}

void
cdpi_bencode::encode(evbuffer *buf)
{
    encode(buf, m_data);
}
void
cdpi_bencode::encode(evbuffer *buf, p_data data)
{
    switch (data->m_type) {
    case DICT:
        encode_dict(buf, data);
        break;
    case LIST:
        encode_list(buf, data);
        break;
    case INT:
        encode_integer(buf, data);
        break;
    case STR:
        encode_str(buf, data);
        break;
    }
}

void
cdpi_bencode::encode_dict(evbuffer *buf, p_data data)
{
    bencode_dict *ben_dict = dynamic_cast<bencode_dict*>(data.get());

    evbuffer_add(buf, "d", 1);

    pair<bencode_str, p_data> p;
    BOOST_FOREACH(p, ben_dict->m_dict) {
        evbuffer_add_printf(buf, "%d:", (int)p.first.m_len);
        evbuffer_add(buf, p.first.m_ptr.get(), p.first.m_len);
        encode(buf, p.second);
    }

    evbuffer_add(buf, "e", 1);
}
void
cdpi_bencode::encode_list(evbuffer *buf, p_data data)
{
    bencode_list *ben_list = dynamic_cast<bencode_list*>(data.get());

    evbuffer_add(buf, "l", 1);

    BOOST_FOREACH(p_data p, ben_list->m_list) {
        encode(buf, p);
    }

    evbuffer_add(buf, "e", 1);
}

void
cdpi_bencode::encode_integer(evbuffer *buf, p_data data)
{
    bencode_int *ben_int = dynamic_cast<bencode_int*>(data.get());

    evbuffer_add_printf(buf, "i%de", ben_int->m_int);
}

void
cdpi_bencode::encode_str(evbuffer *buf, p_data data)
{
    bencode_str *ben_str = dynamic_cast<bencode_str*>(data.get());

    evbuffer_add_printf(buf, "%d:", (int)ben_str->m_len);
    evbuffer_add(buf, ben_str->m_ptr.get(), ben_str->m_len);
}

void
cdpi_bencode::calc_info_hash(istream &in)
{
    istream::pos_type pos = in.tellg();
    istream::pos_type len = m_dict_info_end - m_dict_info_begin;
    boost::scoped_array<char> info(new char[len]);
    cdpi_bytes md_value;

    in.seekg(m_dict_info_begin);
    in.read(info.get(), len);

    get_digest(md_value, HASH_TYPE, info.get(), len);

    m_info_hash = boost::shared_array<char>(new char[md_value.m_len]);
    memcpy(m_info_hash.get(), md_value.m_ptr.get(), md_value.m_len);

    in.seekg(pos);
}
