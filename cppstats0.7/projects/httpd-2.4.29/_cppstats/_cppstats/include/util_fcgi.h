#if !defined(APACHE_UTIL_FCGI_H)
#define APACHE_UTIL_FCGI_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "httpd.h"
typedef struct {
unsigned char version;
unsigned char type;
unsigned char requestIdB1;
unsigned char requestIdB0;
unsigned char contentLengthB1;
unsigned char contentLengthB0;
unsigned char paddingLength;
unsigned char reserved;
} ap_fcgi_header;
#define AP_FCGI_HEADER_LEN 8
#define AP_FCGI_MAX_CONTENT_LEN 65535
#define AP_FCGI_VERSION_1 1
#define AP_FCGI_BEGIN_REQUEST 1
#define AP_FCGI_ABORT_REQUEST 2
#define AP_FCGI_END_REQUEST 3
#define AP_FCGI_PARAMS 4
#define AP_FCGI_STDIN 5
#define AP_FCGI_STDOUT 6
#define AP_FCGI_STDERR 7
#define AP_FCGI_DATA 8
#define AP_FCGI_GET_VALUES 9
#define AP_FCGI_GET_VALUES_RESULT 10
#define AP_FCGI_UNKNOWN_TYPE 11
#define AP_FCGI_MAXTYPE (AP_FCGI_UNKNOWN_TYPE)
#define AP_FCGI_HDR_VERSION_OFFSET 0
#define AP_FCGI_HDR_TYPE_OFFSET 1
#define AP_FCGI_HDR_REQUEST_ID_B1_OFFSET 2
#define AP_FCGI_HDR_REQUEST_ID_B0_OFFSET 3
#define AP_FCGI_HDR_CONTENT_LEN_B1_OFFSET 4
#define AP_FCGI_HDR_CONTENT_LEN_B0_OFFSET 5
#define AP_FCGI_HDR_PADDING_LEN_OFFSET 6
#define AP_FCGI_HDR_RESERVED_OFFSET 7
typedef struct {
unsigned char roleB1;
unsigned char roleB0;
unsigned char flags;
unsigned char reserved[5];
} ap_fcgi_begin_request_body;
#define AP_FCGI_RESPONDER 1
#define AP_FCGI_AUTHORIZER 2
#define AP_FCGI_FILTER 3
#define AP_FCGI_KEEP_CONN 1
#define AP_FCGI_BRB_ROLEB1_OFFSET 0
#define AP_FCGI_BRB_ROLEB0_OFFSET 1
#define AP_FCGI_BRB_FLAGS_OFFSET 2
#define AP_FCGI_BRB_RESERVED0_OFFSET 3
#define AP_FCGI_BRB_RESERVED1_OFFSET 4
#define AP_FCGI_BRB_RESERVED2_OFFSET 5
#define AP_FCGI_BRB_RESERVED3_OFFSET 6
#define AP_FCGI_BRB_RESERVED4_OFFSET 7
AP_DECLARE(void) ap_fcgi_header_to_array(ap_fcgi_header *h,
unsigned char a[]);
AP_DECLARE(void) ap_fcgi_header_from_array(ap_fcgi_header *h,
unsigned char a[]);
AP_DECLARE(void) ap_fcgi_header_fields_from_array(unsigned char *version,
unsigned char *type,
apr_uint16_t *request_id,
apr_uint16_t *content_len,
unsigned char *padding_len,
unsigned char a[]);
AP_DECLARE(void) ap_fcgi_begin_request_body_to_array(ap_fcgi_begin_request_body *h,
unsigned char a[]);
AP_DECLARE(void) ap_fcgi_fill_in_header(ap_fcgi_header *header,
unsigned char type,
apr_uint16_t request_id,
apr_uint16_t content_len,
unsigned char padding_len);
AP_DECLARE(void) ap_fcgi_fill_in_request_body(ap_fcgi_begin_request_body *brb,
int role,
unsigned char flags);
AP_DECLARE(apr_size_t) ap_fcgi_encoded_env_len(apr_table_t *env,
apr_size_t maxlen,
int *starting_elem);
AP_DECLARE(apr_status_t) ap_fcgi_encode_env(request_rec *r,
apr_table_t *env,
void *buffer,
apr_size_t buflen,
int *starting_elem);
#define AP_FCGI_RESPONDER_STR "RESPONDER"
#define AP_FCGI_AUTHORIZER_STR "AUTHORIZER"
#define AP_FCGI_FILTER_STR "FILTER"
#define AP_FCGI_APACHE_ROLE_AUTHENTICATOR_STR "AUTHENTICATOR"
#define AP_FCGI_APACHE_ROLE_AUTHORIZER_STR "AUTHORIZER"
#define AP_FCGI_APACHE_ROLE_ACCESS_CHECKER_STR "ACCESS_CHECKER"
#if defined(__cplusplus)
}
#endif
#endif