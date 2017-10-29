#if !defined(AJP_H)
#define AJP_H
#include "apr_version.h"
#include "apr.h"
#include "apr_hooks.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "apr_md5.h"
#include "apr_network_io.h"
#include "apr_poll.h"
#include "apr_pools.h"
#include "apr_strings.h"
#include "apr_uri.h"
#include "apr_date.h"
#include "apr_fnmatch.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#if APR_HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if APR_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#define AJP13_DEF_HOST "127.0.0.1"
#if defined(NETWARE)
#define AJP13_DEF_PORT 9009
#else
#define AJP13_DEF_PORT 8009
#endif
#define AJP13_HTTPS_INDICATOR "HTTPS"
#define AJP13_SSL_PROTOCOL_INDICATOR "SSL_PROTOCOL"
#define AJP13_SSL_CLIENT_CERT_INDICATOR "SSL_CLIENT_CERT"
#define AJP13_SSL_CIPHER_INDICATOR "SSL_CIPHER"
#define AJP13_SSL_SESSION_INDICATOR "SSL_SESSION_ID"
#define AJP13_SSL_KEY_SIZE_INDICATOR "SSL_CIPHER_USEKEYSIZE"
#if defined(AJP_USE_HTTPD_WRAP)
#include "httpd_wrap.h"
#else
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#endif
#include "mod_proxy.h"
#include "util_ebcdic.h"
#define AJP_EOVERFLOW (APR_OS_START_USERERR + 1)
#define AJP_ETOSMALL (APR_OS_START_USERERR + 2)
#define AJP_EINVAL (APR_OS_START_USERERR + 3)
#define AJP_EBAD_SIGNATURE (APR_OS_START_USERERR + 4)
#define AJP_ETOBIG (APR_OS_START_USERERR + 5)
#define AJP_ENO_HEADER (APR_OS_START_USERERR + 6)
#define AJP_EBAD_HEADER (APR_OS_START_USERERR + 7)
#define AJP_EBAD_MESSAGE (APR_OS_START_USERERR + 8)
#define AJP_ELOGFAIL (APR_OS_START_USERERR + 9)
#define AJP_EBAD_METHOD (APR_OS_START_USERERR + 10)
typedef struct ajp_msg ajp_msg_t;
struct ajp_msg {
apr_byte_t *buf;
apr_size_t header_len;
apr_size_t len;
apr_size_t pos;
int server_side;
apr_size_t max_size;
};
#define AJP13_WS_HEADER 0x1234
#define AJP_HEADER_LEN 4
#define AJP_HEADER_SZ_LEN 2
#define AJP_HEADER_SZ 6
#define AJP_MSG_BUFFER_SZ 8192
#define AJP_MAX_BUFFER_SZ 65536
#define AJP13_MAX_SEND_BODY_SZ (AJP_MAX_BUFFER_SZ - AJP_HEADER_SZ)
#define AJP_PING_PONG_SZ 128
#define CMD_AJP13_FORWARD_REQUEST (unsigned char)2
#define CMD_AJP13_SEND_BODY_CHUNK (unsigned char)3
#define CMD_AJP13_SEND_HEADERS (unsigned char)4
#define CMD_AJP13_END_RESPONSE (unsigned char)5
#define CMD_AJP13_GET_BODY_CHUNK (unsigned char)6
#define CMD_AJP13_SHUTDOWN (unsigned char)7
#define CMD_AJP13_PING (unsigned char)8
#define CMD_AJP13_CPONG (unsigned char)9
#define CMD_AJP13_CPING (unsigned char)10
apr_status_t ajp_msg_check_header(ajp_msg_t *msg, apr_size_t *len);
apr_status_t ajp_msg_reset(ajp_msg_t *msg);
apr_status_t ajp_msg_reuse(ajp_msg_t *msg);
apr_status_t ajp_msg_end(ajp_msg_t *msg);
apr_status_t ajp_msg_append_uint32(ajp_msg_t *msg, apr_uint32_t value);
apr_status_t ajp_msg_append_uint16(ajp_msg_t *msg, apr_uint16_t value);
apr_status_t ajp_msg_append_uint8(ajp_msg_t *msg, apr_byte_t value);
apr_status_t ajp_msg_append_string_ex(ajp_msg_t *msg, const char *value,
int convert);
#define ajp_msg_append_string(m, v) ajp_msg_append_string_ex(m, v, 1)
#define ajp_msg_append_string_ascii(m, v) ajp_msg_append_string_ex(m, v, 0)
apr_status_t ajp_msg_append_bytes(ajp_msg_t *msg, const apr_byte_t *value,
apr_size_t valuelen);
apr_status_t ajp_msg_get_uint32(ajp_msg_t *msg, apr_uint32_t *rvalue);
apr_status_t ajp_msg_get_uint16(ajp_msg_t *msg, apr_uint16_t *rvalue);
apr_status_t ajp_msg_peek_uint16(ajp_msg_t *msg, apr_uint16_t *rvalue);
apr_status_t ajp_msg_get_uint8(ajp_msg_t *msg, apr_byte_t *rvalue);
apr_status_t ajp_msg_peek_uint8(ajp_msg_t *msg, apr_byte_t *rvalue);
apr_status_t ajp_msg_get_string(ajp_msg_t *msg, const char **rvalue);
apr_status_t ajp_msg_get_bytes(ajp_msg_t *msg, apr_byte_t **rvalue,
apr_size_t *rvalue_len);
apr_status_t ajp_msg_create(apr_pool_t *pool, apr_size_t size, ajp_msg_t **rmsg);
apr_status_t ajp_msg_copy(ajp_msg_t *smsg, ajp_msg_t *dmsg);
apr_status_t ajp_msg_serialize_ping(ajp_msg_t *msg);
apr_status_t ajp_msg_serialize_cping(ajp_msg_t *msg);
apr_status_t ajp_msg_dump(apr_pool_t *pool, ajp_msg_t *msg, char *err,
apr_size_t count, char **buf);
apr_status_t ajp_msg_log(request_rec *r, ajp_msg_t *msg, char *err);
apr_status_t ajp_ilink_send(apr_socket_t *sock, ajp_msg_t *msg);
apr_status_t ajp_ilink_receive(apr_socket_t *sock, ajp_msg_t *msg);
apr_status_t ajp_send_header(apr_socket_t *sock, request_rec *r,
apr_size_t buffsize,
apr_uri_t *uri);
apr_status_t ajp_read_header(apr_socket_t *sock,
request_rec *r,
apr_size_t buffsize,
ajp_msg_t **msg);
apr_status_t ajp_alloc_data_msg(apr_pool_t *pool, char **ptr,
apr_size_t *len, ajp_msg_t **msg);
apr_status_t ajp_send_data_msg(apr_socket_t *sock,
ajp_msg_t *msg, apr_size_t len);
int ajp_parse_type(request_rec *r, ajp_msg_t *msg);
apr_status_t ajp_parse_header(request_rec *r, proxy_dir_conf *conf,
ajp_msg_t *msg);
apr_status_t ajp_parse_data(request_rec *r, ajp_msg_t *msg,
apr_uint16_t *len, char **ptr);
apr_status_t ajp_parse_reuse(request_rec *r, ajp_msg_t *msg,
apr_byte_t *reuse);
apr_status_t ajp_handle_cping_cpong(apr_socket_t *sock,
request_rec *r,
apr_interval_time_t timeout);
const char *ajp_type_str(int type);
#endif
