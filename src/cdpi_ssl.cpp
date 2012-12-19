#include "cdpi_ssl.hpp"
#include "cdpi_stream.hpp"

#include <arpa/inet.h>

#include <boost/regex.hpp>

using namespace std;

#define SSL20_VER 0x0200
#define SSL30_VER 0x0300
#define TLS10_VER 0x0301

/*
 * Content Type: Handshake(0x16)
 * Version: SSL2.0(0x0200), SSL3.0(0x0300), TLS1.0(0x0301)
 * Length: 2 bytes
 * Handshake Type: Cleint Hello(0x01), Server Hello(0x02)
 * Length: 3 bytes
 * Version: SSL2.0(0x0200), SSL3.0(0x0300), TLS1.0(0x0301)
 */

static boost::regex regex_ssl_client_hello("^\\x16(\\x02\\x00|\\x03\\x00|\\x03\\x01)..\\x01...\\1.*");
static boost::regex regex_ssl_server_hello("^\\x16(\\x02\\x00|\\x03\\x00|\\x03\\x01)..\\x02...\\1.*");

struct cdpi_ssl_compression_method {
    uint16_t    m_idx;
    const char *m_method;
};

struct cdpi_ssl_cipher_suite {
    uint16_t    m_idx;
    const char *m_cipher;
};

// cf. http://www.iana.org/assignments/comp-meth-ids/comp-meth-ids.xml
cdpi_ssl_compression_method compression_methods[] = {
    { 0, "NULL"},
    { 1, "DEFLATE"},
    {64, "LZS"}
};

// cf. http://www.iana.org/assignments/tls-parameters/tls-parameters.xml
cdpi_ssl_cipher_suite cipher_suites[] = {
    {0x0000, "TLS_NULL_WITH_NULL_NULL"},
    {0x0001, "TLS_RSA_WITH_NULL_MD5"},
    {0x0002, "TLS_RSA_WITH_NULL_SHA"},
    {0x0003, "TLS_RSA_EXPORT_WITH_RC4_40_MD5"},
    {0x0004, "TLS_RSA_WITH_RC4_128_MD5"},
    {0x0005, "TLS_RSA_WITH_RC4_128_SHA"},
    {0x0006, "TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5"},
    {0x0007, "TLS_RSA_WITH_IDEA_CBC_SHA"},
    {0x0008, "TLS_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {0x0009, "TLS_RSA_WITH_DES_CBC_SHA"},
    {0x000a, "TLS_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0x000b, "TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA"},
    {0x000c, "TLS_DH_DSS_WITH_DES_CBC_SHA"},
    {0x000d, "TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA"},
    {0x000e, "TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {0x000f, "TLS_DH_RSA_WITH_DES_CBC_SHA"},
    {0x0010, "TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0x0011, "TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA"},
    {0x0012, "TLS_DHE_DSS_WITH_DES_CBC_SHA"},
    {0x0013, "TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA"},
    {0x0014, "TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA"},
    {0x0015, "TLS_DHE_RSA_WITH_DES_CBC_SHA"},
    {0x0016, "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0x0017, "TLS_DH_anon_EXPORT_WITH_RC4_40_MD5"},
    {0x0018, "TLS_DH_anon_WITH_RC4_128_MD5"},
    {0x0019, "TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA"},
    {0x001a, "TLS_DH_anon_WITH_DES_CBC_SHA"},
    {0x001b, "TLS_DH_anon_WITH_3DES_EDE_CBC_SHA"},
    // 0x001c-1d Reserved to avoid conflicts with SSLv3
    {0x001e, "TLS_KRB5_WITH_DES_CBC_SHA"},
    {0x001f, "TLS_KRB5_WITH_3DES_EDE_CBC_SHA"},
    {0x0020, "TLS_KRB5_WITH_RC4_128_SHA"},
    {0x0021, "TLS_KRB5_WITH_IDEA_CBC_SHA"},
    {0x0022, "TLS_KRB5_WITH_DES_CBC_MD5"},
    {0x0023, "TLS_KRB5_WITH_3DES_EDE_CBC_MD5"},
    {0x0024, "TLS_KRB5_WITH_RC4_128_MD5"},
    {0x0025, "TLS_KRB5_WITH_IDEA_CBC_MD5"},
    {0x0026, "TLS_KRB5_EXPORT_WITH_DES_CBC_40_SHA"},
    {0x0027, "TLS_KRB5_EXPORT_WITH_RC2_CBC_40_SHA"},
    {0x0028, "TLS_KRB5_EXPORT_WITH_RC4_40_SHA"},
    {0x0029, "TLS_KRB5_EXPORT_WITH_DES_CBC_40_MD5"},
    {0x002a, "TLS_KRB5_EXPORT_WITH_RC2_CBC_40_MD5"},
    {0x002b, "TLS_KRB5_EXPORT_WITH_RC4_40_MD5"},
    {0x002c, "TLS_PSK_WITH_NULL_SHA"},
    {0x002d, "TLS_DHE_PSK_WITH_NULL_SHA"},
    {0x002e, "TLS_RSA_PSK_WITH_NULL_SHA"},
    {0x002f, "TLS_RSA_WITH_AES_128_CBC_SHA"},
    {0x0030, "TLS_DH_DSS_WITH_AES_128_CBC_SHA"},
    {0x0031, "TLS_DH_RSA_WITH_AES_128_CBC_SHA"},
    {0x0032, "TLS_DHE_DSS_WITH_AES_128_CBC_SHA"},
    {0x0033, "TLS_DHE_RSA_WITH_AES_128_CBC_SHA"},
    {0x0034, "TLS_DH_anon_WITH_AES_128_CBC_SHA"},
    {0x0035, "TLS_RSA_WITH_AES_256_CBC_SHA"},
    {0x0036, "TLS_DH_DSS_WITH_AES_256_CBC_SHA"},
    {0x0037, "TLS_DH_RSA_WITH_AES_256_CBC_SHA"},
    {0x0038, "TLS_DHE_DSS_WITH_AES_256_CBC_SHA"},
    {0x0039, "TLS_DHE_RSA_WITH_AES_256_CBC_SHA"},
    {0x003a, "TLS_DH_anon_WITH_AES_256_CBC_SHA"},
    {0x003b, "TLS_RSA_WITH_NULL_SHA256"},
    {0x003c, "TLS_RSA_WITH_AES_128_CBC_SHA256"},
    {0x003d, "TLS_RSA_WITH_AES_256_CBC_SHA256"},
    {0x003e, "TLS_DH_DSS_WITH_AES_128_CBC_SHA256"},
    {0x003f, "TLS_DH_RSA_WITH_AES_128_CBC_SHA256"},
    {0x0040, "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256"},
    {0x0041, "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0042, "TLS_DH_DSS_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0043, "TLS_DH_RSA_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0044, "TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0045, "TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA"},
    {0x0046, "TLS_DH_anon_WITH_CAMELLIA_128_CBC_SHA"},
    // 0x0047-4f Reserved to avoid conflicts with deployed implementations
    // 0x0050-58 Reserved to avoid conflicts
    // 0x0059-5c Reserved to avoid conflicts with deployed implementations
    // 0x005d-5f Unassigned
    // 0x0060-66 Reserved to avoid conflicts with widely deployed implementations
    {0x0067, "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256"},
    {0x0068, "TLS_DH_DSS_WITH_AES_256_CBC_SHA256"},
    {0x0069, "TLS_DH_RSA_WITH_AES_256_CBC_SHA256"},
    {0x006a, "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256"},
    {0x006b, "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256"},
    {0x006c, "TLS_DH_anon_WITH_AES_128_CBC_SHA256"},
    {0x006d, "TLS_DH_anon_WITH_AES_256_CBC_SHA256"},
    // 0x6e-83 Unassigned
    {0x0084, "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA"},
    {0x0085, "TLS_DH_DSS_WITH_CAMELLIA_256_CBC_SHA"},
    {0x0086, "TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA"},
    {0x0087, "TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA"},
    {0x0088, "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA"},
    {0x0089, "TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA"},
    {0x008a, "TLS_PSK_WITH_RC4_128_SHA"},
    {0x008b, "TLS_PSK_WITH_3DES_EDE_CBC_SHA"},
    {0x008c, "TLS_PSK_WITH_AES_128_CBC_SHA"},
    {0x008d, "TLS_PSK_WITH_AES_256_CBC_SHA"},
    {0x008e, "TLS_DHE_PSK_WITH_RC4_128_SHA"},
    {0x008f, "TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA"},
    {0x0090, "TLS_DHE_PSK_WITH_AES_128_CBC_SHA"},
    {0x0091, "TLS_DHE_PSK_WITH_AES_256_CBC_SHA"},
    {0x0092, "TLS_RSA_PSK_WITH_RC4_128_SHA"},
    {0x0093, "TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA"},
    {0x0094, "TLS_RSA_PSK_WITH_AES_128_CBC_SHA"},
    {0x0095, "TLS_RSA_PSK_WITH_AES_256_CBC_SHA"},
    {0x0096, "TLS_RSA_WITH_SEED_CBC_SHA"},
    {0x0097, "TLS_DH_DSS_WITH_SEED_CBC_SHA"},
    {0x0098, "TLS_DH_RSA_WITH_SEED_CBC_SHA"},
    {0x0099, "TLS_DHE_DSS_WITH_SEED_CBC_SHA"},
    {0x009a, "TLS_DHE_RSA_WITH_SEED_CBC_SHA"},
    {0x009b, "TLS_DH_anon_WITH_SEED_CBC_SHA"},
    {0x009c, "TLS_RSA_WITH_AES_128_GCM_SHA256"},
    {0x009d, "TLS_RSA_WITH_AES_256_GCM_SHA384"},
    {0x009e, "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256"},
    {0x009f, "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384"},
    {0x00a0, "TLS_DH_RSA_WITH_AES_128_GCM_SHA256"},
    {0x00a1, "TLS_DH_RSA_WITH_AES_256_GCM_SHA384"},
    {0x00a2, "TLS_DHE_DSS_WITH_AES_128_GCM_SHA256"},
    {0x00a3, "TLS_DHE_DSS_WITH_AES_256_GCM_SHA384"},
    {0x00a4, "TLS_DH_DSS_WITH_AES_128_GCM_SHA256"},
    {0x00a5, "TLS_DH_DSS_WITH_AES_256_GCM_SHA384"},
    {0x00a6, "TLS_DH_anon_WITH_AES_128_GCM_SHA256"},
    {0x00a7, "TLS_DH_anon_WITH_AES_256_GCM_SHA384"},
    {0x00a8, "TLS_PSK_WITH_AES_128_GCM_SHA256"},
    {0x00a9, "TLS_PSK_WITH_AES_256_GCM_SHA384"},
    {0x00aa, "TLS_DHE_PSK_WITH_AES_128_GCM_SHA256"},
    {0x00ab, "TLS_DHE_PSK_WITH_AES_256_GCM_SHA384"},
    {0x00ac, "TLS_RSA_PSK_WITH_AES_128_GCM_SHA256"},
    {0x00ad, "TLS_RSA_PSK_WITH_AES_256_GCM_SHA384"},
    {0x00ae, "TLS_PSK_WITH_AES_128_CBC_SHA256"},
    {0x00af, "TLS_PSK_WITH_AES_256_CBC_SHA384"},
    {0x00b0, "TLS_PSK_WITH_NULL_SHA256"},
    {0x00b1, "TLS_PSK_WITH_NULL_SHA384"},
    {0x00b2, "TLS_DHE_PSK_WITH_AES_128_CBC_SHA256"},
    {0x00b3, "TLS_DHE_PSK_WITH_AES_256_CBC_SHA384"},
    {0x00b4, "TLS_DHE_PSK_WITH_NULL_SHA256"},
    {0x00b5, "TLS_DHE_PSK_WITH_NULL_SHA384"},
    {0x00b6, "TLS_RSA_PSK_WITH_AES_128_CBC_SHA256"},
    {0x00b7, "TLS_RSA_PSK_WITH_AES_256_CBC_SHA384"},
    {0x00b8, "TLS_RSA_PSK_WITH_NULL_SHA256"},
    {0x00b9, "TLS_RSA_PSK_WITH_NULL_SHA384"},
    {0x00ba, "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00bb, "TLS_DH_DSS_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00bc, "TLS_DH_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00bd, "TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00be, "TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00bf, "TLS_DH_anon_WITH_CAMELLIA_128_CBC_SHA256"},
    {0x00c0, "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00c1, "TLS_DH_DSS_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00c2, "TLS_DH_RSA_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00c3, "TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00c4, "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256"},
    {0x00c5, "TLS_DH_anon_WITH_CAMELLIA_256_CBC_SHA256"},
    // 0x00c6-fe Unassigned
    {0x00ff, "TLS_EMPTY_RENEGOTIATION_INFO_SCSV"},
    // 0x01-bfff Unassigned
    {0xc001, "TLS_ECDH_ECDSA_WITH_NULL_SHA"},
    {0xc002, "TLS_ECDH_ECDSA_WITH_RC4_128_SHA"},
    {0xc003, "TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA"},
    {0xc004, "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA"},
    {0xc005, "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA"},
    {0xc006, "TLS_ECDHE_ECDSA_WITH_NULL_SHA"},
    {0xc007, "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA"},
    {0xc008, "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA"},
    {0xc009, "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA"},
    {0xc00a, "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA"},
    {0xc00b, "TLS_ECDH_RSA_WITH_NULL_SHA"},
    {0xc00c, "TLS_ECDH_RSA_WITH_RC4_128_SHA"},
    {0xc00d, "TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0xc00e, "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA"},
    {0xc00f, "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA"},
    {0xc010, "TLS_ECDHE_RSA_WITH_NULL_SHA"},
    {0xc011, "TLS_ECDHE_RSA_WITH_RC4_128_SHA"},
    {0xc012, "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0xc013, "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA"},
    {0xc014, "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA"},
    {0xc015, "TLS_ECDH_anon_WITH_NULL_SHA"},
    {0xc016, "TLS_ECDH_anon_WITH_RC4_128_SHA"},
    {0xc017, "TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA"},
    {0xc018, "TLS_ECDH_anon_WITH_AES_128_CBC_SHA"},
    {0xc019, "TLS_ECDH_anon_WITH_AES_256_CBC_SHA"},
    {0xc01a, "TLS_SRP_SHA_WITH_3DES_EDE_CBC_SHA"},
    {0xc01b, "TLS_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA"},
    {0xc01c, "TLS_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA"},
    {0xc01d, "TLS_SRP_SHA_WITH_AES_128_CBC_SHA"},
    {0xc01e, "TLS_SRP_SHA_RSA_WITH_AES_128_CBC_SHA"},
    {0xc01f, "TLS_SRP_SHA_DSS_WITH_AES_128_CBC_SHA"},
    {0xc020, "TLS_SRP_SHA_WITH_AES_256_CBC_SHA"},
    {0xc021, "TLS_SRP_SHA_RSA_WITH_AES_256_CBC_SHA"},
    {0xc022, "TLS_SRP_SHA_DSS_WITH_AES_256_CBC_SHA"},
    {0xc023, "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256"},
    {0xc024, "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384"},
    {0xc025, "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256"},
    {0xc026, "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384"},
    {0xc027, "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256"},
    {0xc028, "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384"},
    {0xc029, "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256"},
    {0xc02a, "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384"},
    {0xc02b, "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256"},
    {0xc02c, "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384"},
    {0xc02d, "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256"},
    {0xc02e, "TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384"},
    {0xc02f, "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256"},
    {0xc030, "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384"},
    {0xc031, "TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256"},
    {0xc032, "TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384"},
    {0xc033, "TLS_ECDHE_PSK_WITH_RC4_128_SHA"},
    {0xc034, "TLS_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA"},
    {0xc035, "TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA"},
    {0xc036, "TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA"},
    {0xc037, "TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256"},
    {0xc038, "TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384"},
    {0xc039, "TLS_ECDHE_PSK_WITH_NULL_SHA"},
    {0xc03a, "TLS_ECDHE_PSK_WITH_NULL_SHA256"},
    {0xc03b, "TLS_ECDHE_PSK_WITH_NULL_SHA384"},
    {0xc03c, "TLS_RSA_WITH_ARIA_128_CBC_SHA256"},
    {0xc03d, "TLS_RSA_WITH_ARIA_256_CBC_SHA384"},
    {0xc03e, "TLS_DH_DSS_WITH_ARIA_128_CBC_SHA256"},
    {0xc03f, "TLS_DH_DSS_WITH_ARIA_256_CBC_SHA384"},
    {0xc040, "TLS_DH_RSA_WITH_ARIA_128_CBC_SHA256"},
    {0xc041, "TLS_DH_RSA_WITH_ARIA_256_CBC_SHA384"},
    {0xc042, "TLS_DHE_DSS_WITH_ARIA_128_CBC_SHA256"},
    {0xc043, "TLS_DHE_DSS_WITH_ARIA_256_CBC_SHA384"},
    {0xc044, "TLS_DHE_RSA_WITH_ARIA_128_CBC_SHA256"},
    {0xc045, "TLS_DHE_RSA_WITH_ARIA_256_CBC_SHA384"},
    {0xc046, "TLS_DH_anon_WITH_ARIA_128_CBC_SHA256"},
    {0xc047, "TLS_DH_anon_WITH_ARIA_256_CBC_SHA384"},
    {0xc048, "TLS_ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256"},
    {0xc049, "TLS_ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384"},
    {0xc04a, "TLS_ECDH_ECDSA_WITH_ARIA_128_CBC_SHA256"},
    {0xc04b, "TLS_ECDH_ECDSA_WITH_ARIA_256_CBC_SHA384"},
    {0xc04c, "TLS_ECDHE_RSA_WITH_ARIA_128_CBC_SHA256"},
    {0xc04d, "TLS_ECDHE_RSA_WITH_ARIA_256_CBC_SHA384"},
    {0xc04e, "TLS_ECDH_RSA_WITH_ARIA_128_CBC_SHA256"},
    {0xc04f, "TLS_ECDH_RSA_WITH_ARIA_256_CBC_SHA384"},
    {0xc050, "TLS_RSA_WITH_ARIA_128_GCM_SHA256"},
    {0xc051, "TLS_RSA_WITH_ARIA_256_GCM_SHA384"},
    {0xc052, "TLS_DHE_RSA_WITH_ARIA_128_GCM_SHA256"},
    {0xc053, "TLS_DHE_RSA_WITH_ARIA_256_GCM_SHA384"},
    {0xc054, "TLS_DH_RSA_WITH_ARIA_128_GCM_SHA256"},
    {0xc055, "TLS_DH_RSA_WITH_ARIA_256_GCM_SHA384"},
    {0xc056, "TLS_DHE_DSS_WITH_ARIA_128_GCM_SHA256"},
    {0xc057, "TLS_DHE_DSS_WITH_ARIA_256_GCM_SHA384"},
    {0xc058, "TLS_DH_DSS_WITH_ARIA_128_GCM_SHA256"},
    {0xc059, "TLS_DH_DSS_WITH_ARIA_256_GCM_SHA384"},
    {0xc05a, "TLS_DH_anon_WITH_ARIA_128_GCM_SHA256"},
    {0xc05b, "TLS_DH_anon_WITH_ARIA_256_GCM_SHA384"},
    {0xc05c, "TLS_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256"},
    {0xc05d, "TLS_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384"},
    {0xc05e, "TLS_ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256"},
    {0xc05f, "TLS_ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384"},
    {0xc060, "TLS_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256"},
    {0xc061, "TLS_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384"},
    {0xc062, "TLS_ECDH_RSA_WITH_ARIA_128_GCM_SHA256"},
    {0xc063, "TLS_ECDH_RSA_WITH_ARIA_256_GCM_SHA384"},
    {0xc064, "TLS_PSK_WITH_ARIA_128_CBC_SHA256"},
    {0xc065, "TLS_PSK_WITH_ARIA_256_CBC_SHA384"},
    {0xc066, "TLS_DHE_PSK_WITH_ARIA_128_CBC_SHA256"},
    {0xc067, "TLS_DHE_PSK_WITH_ARIA_256_CBC_SHA384"},
    {0xc068, "TLS_RSA_PSK_WITH_ARIA_128_CBC_SHA256"},
    {0xc069, "TLS_RSA_PSK_WITH_ARIA_256_CBC_SHA384"},
    {0xc06a, "TLS_PSK_WITH_ARIA_128_GCM_SHA256"},
    {0xc06b, "TLS_PSK_WITH_ARIA_256_GCM_SHA384"},
    {0xc06c, "TLS_DHE_PSK_WITH_ARIA_128_GCM_SHA256"},
    {0xc06d, "TLS_DHE_PSK_WITH_ARIA_256_GCM_SHA384"},
    {0xc06e, "TLS_RSA_PSK_WITH_ARIA_128_GCM_SHA256"},
    {0xc06f, "TLS_RSA_PSK_WITH_ARIA_256_GCM_SHA384"},
    {0xc070, "TLS_ECDHE_PSK_WITH_ARIA_128_CBC_SHA256"},
    {0xc071, "TLS_ECDHE_PSK_WITH_ARIA_256_CBC_SHA384"},
    {0xc072, "TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256"},
    {0xc073, "TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384"},
    {0xc074, "TLS_ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256"},
    {0xc075, "TLS_ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384"},
    {0xc076, "TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
    {0xc077, "TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384"},
    {0xc078, "TLS_ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
    {0xc079, "TLS_ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384"},
    {0xc07a, "TLS_RSA_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc07b, "TLS_RSA_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc07c, "TLS_DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc07d, "TLS_DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc07e, "TLS_DH_RSA_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc07f, "TLS_DH_RSA_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc080, "TLS_DHE_DSS_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc081, "TLS_DHE_DSS_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc082, "TLS_DH_DSS_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc083, "TLS_DH_DSS_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc084, "TLS_DH_anon_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc085, "TLS_DH_anon_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc086, "TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc087, "TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc088, "TLS_ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc089, "TLS_ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc08a, "TLS_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc08b, "TLS_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc08c, "TLS_ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc08d, "TLS_ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc08e, "TLS_PSK_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc08f, "TLS_PSK_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc090, "TLS_DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc091, "TLS_DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc092, "TLS_RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256"},
    {0xc093, "TLS_RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384"},
    {0xc094, "TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256"},
    {0xc095, "TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384"},
    {0xc096, "TLS_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256"},
    {0xc097, "TLS_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384"},
    {0xc098, "TLS_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256"},
    {0xc099, "TLS_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384"},
    {0xc09a, "TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256"},
    {0xc09b, "TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384"},
    {0xc09c, "TLS_RSA_WITH_AES_128_CCM"},
    {0xc09d, "TLS_RSA_WITH_AES_256_CCM"},
    {0xc09e, "TLS_DHE_RSA_WITH_AES_128_CCM"},
    {0xc09f, "TLS_DHE_RSA_WITH_AES_256_CCM"},
    {0xc0a0, "TLS_RSA_WITH_AES_128_CCM_8"},
    {0xc0a1, "TLS_RSA_WITH_AES_256_CCM_8"},
    {0xc0a2, "TLS_DHE_RSA_WITH_AES_128_CCM_8"},
    {0xc0a3, "TLS_DHE_RSA_WITH_AES_256_CCM_8"},
    {0xc0a4, "TLS_PSK_WITH_AES_128_CCM"},
    {0xc0a5, "TLS_PSK_WITH_AES_256_CCM"},
    {0xc0a6, "TLS_DHE_PSK_WITH_AES_128_CCM"},
    {0xc0a7, "TLS_DHE_PSK_WITH_AES_256_CCM"},
    {0xc0a8, "TLS_PSK_WITH_AES_128_CCM_8"},
    {0xc0a9, "TLS_PSK_WITH_AES_256_CCM_8"},
    {0xc0aa, "TLS_PSK_DHE_WITH_AES_128_CCM_8"},
    {0xc0ab, "TLS_PSK_DHE_WITH_AES_256_CCM_8"}
};

cdpi_ssl::cdpi_ssl(cdpi_proto_type type) : m_type(type), m_ver(0)
{

}

cdpi_ssl::~cdpi_ssl()
{

}

bool
cdpi_ssl::is_ssl_client(list<cdpi_bytes> &bytes)
{
    if (bytes.size() == 0)
        return false;

    cdpi_bytes buf = bytes.front();
    string     data(buf.m_ptr.get() + buf.m_pos,
                    buf.m_ptr.get() + buf.m_pos + buf.m_len);

    return boost::regex_match(data, regex_ssl_client_hello);
}

bool
cdpi_ssl::is_ssl_server(list<cdpi_bytes> &bytes)
{
    if (bytes.size() == 0)
        return false;

    cdpi_bytes buf = bytes.front();
    string     data(buf.m_ptr.get() + buf.m_pos,
                    buf.m_ptr.get() + buf.m_pos + buf.m_len);

    return boost::regex_match(data, regex_ssl_server_hello);
}

void
cdpi_ssl::parse(list<cdpi_bytes> &bytes)
{
    uint16_t ver;
    uint16_t len;
    uint8_t  type;
    char     buf[5];
    int      read_len;

    for (;;) {
        read_len = read_bytes(bytes, buf, sizeof(buf));

        if (read_len < (int)sizeof(buf))
            return;

        type = (uint8_t)buf[0];

        memcpy(&ver, &buf[1], sizeof(ver));
        memcpy(&len, &buf[3], sizeof(len));

        ver = ntohs(ver);
        len = ntohs(len);

        m_ver = ver;

        boost::shared_array<char> data(new char[len]);

        read_len = read_bytes(bytes, data.get(), len);

        if (read_len < len)
            return;

        skip_bytes(bytes, read_len + sizeof(buf));

        switch (type) {
        case SSL_HANDSHAKE:
            parse_handshake(data.get(), len);
            break;
        case SSL_CHANGE_CIPHER_SPEC:
        case SSL_ALERT:
        case SSL_APPLICATION_DATA:
        case SSL_HEARTBEAT:
            break;
        default:
            throw cdpi_parse_error(__FILE__, __LINE__);
        }
    }
}

void
cdpi_ssl::parse_handshake(char *data, int len)
{
    uint32_t msg_len;
    uint16_t ver;
    uint8_t  type = data[0];

    memcpy(&msg_len, &data[0], sizeof(len));
    memcpy(&ver, &data[4], sizeof(ver));

    msg_len = ntohl(msg_len) & 0x00ffffff;

    m_ver = ver;

    switch (type) {
    case SSL_CLIENT_HELLO:
        parse_client_hello(data + 6, msg_len);
        break;
    case SSL_SERVER_HELLO:
        parse_server_hello(data + 6, msg_len);
        break;
    case SSL_CERTIFICATE:
        break;
    default:
        ;
    }
}

void
cdpi_ssl::parse_server_hello(char *data, int len)
{
    switch (m_ver) {
    case SSL30_VER:
    case TLS10_VER:
    {
        char    *end_of_data = data + len;
        char    *p;
        uint8_t  session_id_len;
        uint8_t  compression_method;
        uint16_t cipher_suite;

        // read GMT UNIX Time
        p     = data;
        data += sizeof(m_gmt_unix_time);

        if (data > end_of_data)
            return;

        memcpy(&m_gmt_unix_time, p, sizeof(m_gmt_unix_time));
        m_gmt_unix_time = ntohl(m_gmt_unix_time);


        // read random
        p     = data;
        data += sizeof(m_random);

        if (data > end_of_data)
            return;

        memcpy(m_random, p, sizeof(m_random));


        // read session ID
        p     = data;
        data += sizeof(session_id_len);

        if (data > end_of_data)
            return;

        memcpy(&session_id_len, p, sizeof(session_id_len));

        if (session_id_len > 0) {
            p     = data;
            data += session_id_len;

            if (data > end_of_data)
                return;

            m_session_id.set_buf(p, session_id_len);
        }


        // read cipher suite
        p     = data;
        data += sizeof(cipher_suite);

        if (data > end_of_data)
            return;

        memcpy(&cipher_suite, p, sizeof(cipher_suite));
        cipher_suite = ntohs(cipher_suite);

        m_cipher_suites.push_back(cipher_suite);


        // read compression method
        p     = data;
        data += sizeof(compression_method);

        if (data > end_of_data)
            return;

        memcpy(&compression_method, p, sizeof(compression_method));


        // TODO: read extensions

        break;
    }
    case SSL20_VER:
        break;
    default:
        ;
    }
}

void
cdpi_ssl::parse_client_hello(char *data, int len)
{
    switch (m_ver) {
    case SSL30_VER:
    case TLS10_VER:
    {
        char    *end_of_data = data + len;
        char    *p;
        uint8_t  session_id_len;
        uint8_t  compression_methods_len;
        uint16_t cipher_suites_len;

        // read GMT UNIX Time
        p     = data;
        data += sizeof(m_gmt_unix_time);

        if (data > end_of_data)
            return;

        memcpy(&m_gmt_unix_time, p, sizeof(m_gmt_unix_time));
        m_gmt_unix_time = ntohl(m_gmt_unix_time);


        // read random
        p     = data;
        data += sizeof(m_random);

        if (data > end_of_data)
            return;

        memcpy(m_random, p, sizeof(m_random));


        // read session ID
        p     = data;
        data += sizeof(session_id_len);

        if (data > end_of_data)
            return;

        memcpy(&session_id_len, data, sizeof(session_id_len));


        if (session_id_len > 0) {
            p     = data;
            data += session_id_len;

            if (data > end_of_data)
                return;

            m_session_id.set_buf(p, session_id_len);
        }


        // read cipher suites
        p     = data;
        data += sizeof(cipher_suites_len);

        if (data > end_of_data)
            return;

        memcpy(&cipher_suites_len, p, sizeof(cipher_suites_len));
        cipher_suites_len = ntohs(cipher_suites_len);


        if (data + cipher_suites_len * 2 > end_of_data)
            return;

        for (uint16_t i = 0; i < cipher_suites_len; i++) {
            uint16_t buf;
            memcpy(&buf, &data[i * sizeof(buf)], sizeof(buf));

            m_cipher_suites.push_back(buf);
        }

        data += cipher_suites_len * 2;


        // read compression methods
        p     = data;
        data += sizeof(compression_methods_len);

        memcpy(&compression_methods_len, p, sizeof(compression_methods_len));

        if (data + compression_methods_len > end_of_data)
            return;

        for (uint8_t i = 0; i < compression_methods_len; i++) {
            uint8_t buf;
            memcpy(&buf, &data[i], sizeof(buf));

            m_compression_methods.push_back(buf);
        }

        data += compression_methods_len;


        // TODO: read extensions

        break;
    }
    case SSL20_VER:
        break;
    default:
        ;
    }
}