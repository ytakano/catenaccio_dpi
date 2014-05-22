#include "cdpi_bencode.hpp"

#include <stdlib.h>

#include <openssl/evp.h>

#include <sstream>

#include <boost/foreach.hpp>
#include <boost/scoped_array.hpp>

using namespace std;

cdpi_bencode::cdpi_bencode() : cdpi_proto(PROTO_BENCODE)
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
        char c;

        in.get(c);

        if (in.good()) {
            return false;
        } else {
            return true;
        }
    } else {
        clear();
        return false;
    }
}

bool
cdpi_bencode::decode(istream &in, ptr_ben_data &data)
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
cdpi_bencode::dec_dict(istream &in, ptr_ben_data &data)
{
    istream::pos_type pos;
    ptr_ben_dict      dict;
    char              c;

    data = ptr_ben_data(new bencode_dict);
    dict = BEN_TO_DICT(data);

    in.ignore();

    while (! in.eof()) {
        c = in.peek();

        if (c == 'e') {
            in.get();
            return true;
        }

        if (c < '0' || c > '9') {
            return false;
        }


        ptr_ben_data key, value;

        if (! dec_str(in, key)) {
            return false;
        }

        pos = in.tellg();

        if (! decode(in, value))
            return false;


        ptr_ben_str str;

        str = BEN_TO_STR(key);

        dict->m_dict[*str] = value; 


        // for info_hash
        if (memcmp(str->get_head(), "info", 4) == 0) {
            m_dict_info_begin = pos;
            m_dict_info_end   = in.tellg();
            calc_info_hash(in);
        }
    }

    return false;
}

bool
cdpi_bencode::dec_list(istream &in, ptr_ben_data &data)
{
    ptr_ben_list ben_list;
    char c;

    in.ignore();

    data = ptr_ben_data(new bencode_list);

    ben_list = BEN_TO_LIST(data);

    for (;;) {
        c = in.peek();
        if (c == 'e') {
            in.get();
            return true;
        }

        ptr_ben_data value;
        if (! decode(in, value))
            return false;

        ben_list->m_list.push_back(value);
    }

    return false;
}

bool
cdpi_bencode::dec_int(istream &in, ptr_ben_data &data)
{
    ptr_ben_int ben_int;
    stringbuf   buf;
    char c;

    in.ignore();

    data = ptr_ben_data(new bencode_int);

    ben_int = BEN_TO_INT(data);

    in.get(buf, 'e');

    in.get(c);
    if (c != 'e') {
        return false;
    }

    ben_int->m_int = atoi(buf.str().c_str());

    return true;
}

bool
cdpi_bencode::dec_str(istream &in, ptr_ben_data &data)
{
    ptr_ben_str ben_str;

    data = ptr_ben_data(new bencode_str);

    ben_str = BEN_TO_STR(data);

    return read_str(in, ben_str);
}

bool
cdpi_bencode::read_str(istream &in, ptr_ben_str ben_str)
{
    stringbuf num;
    char c;
    int  len;

    in.get(num, ':');
    in.get(c);

    if (c != ':')
        return false;

    len = (size_t)atoi(num.str().c_str());

    p_char buf(new char[len + 1]);
    in.read(buf.get(), len);
    if (in.bad())
        return false;

    buf[len] = '\0';

    ben_str->set_buf(buf, len);

    return true;
}

void
cdpi_bencode::encode(ostream &out)
{
    return encode(out, m_data);
}

void
cdpi_bencode::encode(ostream &out, ptr_ben_data data)
{
    switch (data->get_type()) {
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
cdpi_bencode::encode_dict(ostream &out, ptr_ben_data data)
{
    ptr_ben_dict ben_dict = BEN_TO_DICT(data);

    out << "d";

    pair<bencode_str, ptr_ben_data> p;
    BOOST_FOREACH(p, ben_dict->m_dict) {
        out << p.first.get_len() << ":";
        out.write(p.first.get_head(), p.first.get_len());
        encode(out, p.second);
    }

    out << "e";
}

void
cdpi_bencode::encode_list(ostream &out, ptr_ben_data data)
{
    ptr_ben_list ben_list = BEN_TO_LIST(data);

    out << "l";

    BOOST_FOREACH(ptr_ben_data p, ben_list->m_list) {
        encode(out, p);
    }

    out << "e";
}

void
cdpi_bencode::encode_integer(ostream &out, ptr_ben_data data)
{
    ptr_ben_int ben_int = BEN_TO_INT(data);

    out << "i" << ben_int->m_int << "e";
}

void
cdpi_bencode::encode_str(ostream &out, ptr_ben_data data)
{
    ptr_ben_str ben_str = BEN_TO_STR(data);

    out << ben_str->get_len() << ":";
    out.write(ben_str->get_head(), ben_str->get_len());
}

void
cdpi_bencode::encode(evbuffer *buf)
{
    encode(buf, m_data);
}
void
cdpi_bencode::encode(evbuffer *buf, ptr_ben_data data)
{
    switch (data->get_type()) {
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
cdpi_bencode::encode_dict(evbuffer *buf, ptr_ben_data data)
{
    ptr_ben_dict ben_dict = BEN_TO_DICT(data);

    evbuffer_add(buf, "d", 1);

    pair<bencode_str, ptr_ben_data> p;
    BOOST_FOREACH(p, ben_dict->m_dict) {
        evbuffer_add_printf(buf, "%d:", (int)p.first.get_len());
        evbuffer_add(buf, p.first.get_head(), p.first.get_len());
        encode(buf, p.second);
    }

    evbuffer_add(buf, "e", 1);
}
void
cdpi_bencode::encode_list(evbuffer *buf, ptr_ben_data data)
{
    ptr_ben_list ben_list = BEN_TO_LIST(data);

    evbuffer_add(buf, "l", 1);

    BOOST_FOREACH(ptr_ben_data p, ben_list->m_list) {
        encode(buf, p);
    }

    evbuffer_add(buf, "e", 1);
}

void
cdpi_bencode::encode_integer(evbuffer *buf, ptr_ben_data data)
{
    ptr_ben_int ben_int = BEN_TO_INT(data);

    evbuffer_add_printf(buf, "i%de", ben_int->m_int);
}

void
cdpi_bencode::encode_str(evbuffer *buf, ptr_ben_data data)
{
    ptr_ben_str ben_str = BEN_TO_STR(data);

    evbuffer_add_printf(buf, "%d:", ben_str->get_len());
    evbuffer_add(buf, ben_str->get_head(), ben_str->get_len());
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

    m_info_hash = boost::shared_array<char>(new char[md_value.get_len()]);
    memcpy(m_info_hash.get(), md_value.get_head(), md_value.get_len());

    in.seekg(pos);
}
