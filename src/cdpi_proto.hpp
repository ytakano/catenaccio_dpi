#ifndef CDPI_HTTP_PROTO
#define CDPI_HTTP_PROTO

enum cdpi_proto_type {
    PROTO_HTTP_1_0,
    PROTO_HTTP_1_1,
    PROTO_HTTP_PROXY,
    PROTO_NONE,
};

class cdpi_proto {
public:
    cdpi_proto() { }
    virtual ~cdpi_proto() = 0;

};

#endif // CDPI_HTTP_PROTO
