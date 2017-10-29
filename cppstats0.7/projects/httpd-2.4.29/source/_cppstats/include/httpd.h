#if !defined(APACHE_HTTPD_H)
#define APACHE_HTTPD_H
#include "ap_config.h"
#include "ap_mmn.h"
#include "ap_release.h"
#include "apr.h"
#include "apr_general.h"
#include "apr_tables.h"
#include "apr_pools.h"
#include "apr_time.h"
#include "apr_network_io.h"
#include "apr_buckets.h"
#include "apr_poll.h"
#include "apr_thread_proc.h"
#include "os.h"
#include "ap_regex.h"
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif
#if !defined(HTTPD_ROOT)
#if defined(OS2)
#define HTTPD_ROOT "/os2httpd"
#elif defined(WIN32)
#define HTTPD_ROOT "/apache"
#elif defined (NETWARE)
#define HTTPD_ROOT "/apache"
#else
#define HTTPD_ROOT "/usr/local/apache"
#endif
#endif
#if !defined(DOCUMENT_LOCATION)
#if defined(OS2)
#define DOCUMENT_LOCATION HTTPD_ROOT "/docs"
#else
#define DOCUMENT_LOCATION HTTPD_ROOT "/htdocs"
#endif
#endif
#if !defined(DYNAMIC_MODULE_LIMIT)
#define DYNAMIC_MODULE_LIMIT 256
#endif
#define DEFAULT_ADMIN "[no address given]"
#if !defined(DEFAULT_ERRORLOG)
#if defined(OS2) || defined(WIN32)
#define DEFAULT_ERRORLOG "logs/error.log"
#else
#define DEFAULT_ERRORLOG "logs/error_log"
#endif
#endif
#if !defined(DEFAULT_ACCESS_FNAME)
#if defined(OS2)
#define DEFAULT_ACCESS_FNAME "htaccess"
#else
#define DEFAULT_ACCESS_FNAME ".htaccess"
#endif
#endif
#if !defined(SERVER_CONFIG_FILE)
#define SERVER_CONFIG_FILE "conf/httpd.conf"
#endif
#if !defined(DEFAULT_PATH)
#define DEFAULT_PATH "/bin:/usr/bin:/usr/ucb:/usr/bsd:/usr/local/bin"
#endif
#if !defined(SUEXEC_BIN)
#define SUEXEC_BIN HTTPD_ROOT "/bin/suexec"
#endif
#if !defined(DEFAULT_TIMEOUT)
#define DEFAULT_TIMEOUT 60
#endif
#if !defined(DEFAULT_KEEPALIVE_TIMEOUT)
#define DEFAULT_KEEPALIVE_TIMEOUT 5
#endif
#if !defined(DEFAULT_KEEPALIVE)
#define DEFAULT_KEEPALIVE 100
#endif
#if !defined(DEFAULT_LIMIT_REQUEST_LINE)
#define DEFAULT_LIMIT_REQUEST_LINE 8190
#endif
#if !defined(DEFAULT_LIMIT_REQUEST_FIELDSIZE)
#define DEFAULT_LIMIT_REQUEST_FIELDSIZE 8190
#endif
#if !defined(DEFAULT_LIMIT_REQUEST_FIELDS)
#define DEFAULT_LIMIT_REQUEST_FIELDS 100
#endif
#if !defined(DEFAULT_LIMIT_BLANK_LINES)
#define DEFAULT_LIMIT_BLANK_LINES 10
#endif
#define DEFAULT_ADD_DEFAULT_CHARSET_NAME "iso-8859-1"
#define AP_SERVER_PROTOCOL "HTTP/1.1"
#if !defined(AP_DEFAULT_INDEX)
#define AP_DEFAULT_INDEX "index.html"
#endif
#if !defined(AP_TYPES_CONFIG_FILE)
#define AP_TYPES_CONFIG_FILE "conf/mime.types"
#endif
#define DOCTYPE_HTML_2_0 "<!DOCTYPE HTML PUBLIC \"-//IETF//" "DTD HTML 2.0//EN\">\n"
#define DOCTYPE_HTML_3_2 "<!DOCTYPE HTML PUBLIC \"-//W3C//" "DTD HTML 3.2 Final//EN\">\n"
#define DOCTYPE_HTML_4_0S "<!DOCTYPE HTML PUBLIC \"-//W3C//" "DTD HTML 4.0//EN\"\n" "\"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
#define DOCTYPE_HTML_4_0T "<!DOCTYPE HTML PUBLIC \"-//W3C//" "DTD HTML 4.0 Transitional//EN\"\n" "\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n"
#define DOCTYPE_HTML_4_0F "<!DOCTYPE HTML PUBLIC \"-//W3C//" "DTD HTML 4.0 Frameset//EN\"\n" "\"http://www.w3.org/TR/REC-html40/frameset.dtd\">\n"
#define DOCTYPE_XHTML_1_0S "<!DOCTYPE html PUBLIC \"-//W3C//" "DTD XHTML 1.0 Strict//EN\"\n" "\"http://www.w3.org/TR/xhtml1/DTD/" "xhtml1-strict.dtd\">\n"
#define DOCTYPE_XHTML_1_0T "<!DOCTYPE html PUBLIC \"-//W3C//" "DTD XHTML 1.0 Transitional//EN\"\n" "\"http://www.w3.org/TR/xhtml1/DTD/" "xhtml1-transitional.dtd\">\n"
#define DOCTYPE_XHTML_1_0F "<!DOCTYPE html PUBLIC \"-//W3C//" "DTD XHTML 1.0 Frameset//EN\"\n" "\"http://www.w3.org/TR/xhtml1/DTD/" "xhtml1-frameset.dtd\">"
#define HTTP_VERSION(major,minor) (1000*(major)+(minor))
#define HTTP_VERSION_MAJOR(number) ((number)/1000)
#define HTTP_VERSION_MINOR(number) ((number)%1000)
#define DEFAULT_HTTP_PORT 80
#define DEFAULT_HTTPS_PORT 443
#define ap_is_default_port(port,r) ((port) == ap_default_port(r))
#define ap_default_port(r) ap_run_default_port(r)
#define ap_http_scheme(r) ap_run_http_scheme(r)
#define MAX_STRING_LEN HUGE_STRING_LEN
#define HUGE_STRING_LEN 8192
#define AP_IOBUFSIZE 8192
#define AP_MAX_REG_MATCH 10
#define AP_MAX_SENDFILE 16777216
#define APEXIT_OK 0x0
#define APEXIT_INIT 0x2
#define APEXIT_CHILDINIT 0x3
#define APEXIT_CHILDSICK 0x7
#define APEXIT_CHILDFATAL 0xf
#if !defined(AP_DECLARE)
#define AP_DECLARE(type) type
#endif
#if !defined(AP_DECLARE_NONSTD)
#define AP_DECLARE_NONSTD(type) type
#endif
#if !defined(AP_DECLARE_DATA)
#define AP_DECLARE_DATA
#endif
#if !defined(AP_MODULE_DECLARE)
#define AP_MODULE_DECLARE(type) type
#endif
#if !defined(AP_MODULE_DECLARE_NONSTD)
#define AP_MODULE_DECLARE_NONSTD(type) type
#endif
#if !defined(AP_MODULE_DECLARE_DATA)
#define AP_MODULE_DECLARE_DATA
#endif
#if !defined(AP_CORE_DECLARE)
#define AP_CORE_DECLARE AP_DECLARE
#endif
#if !defined(AP_CORE_DECLARE_NONSTD)
#define AP_CORE_DECLARE_NONSTD AP_DECLARE_NONSTD
#endif
#define AP_START_USERERR (APR_OS_START_USERERR + 2000)
#define AP_USERERR_LEN 1000
#define AP_DECLINED (AP_START_USERERR + 0)
typedef struct {
int major;
int minor;
int patch;
const char *add_string;
} ap_version_t;
AP_DECLARE(void) ap_get_server_revision(ap_version_t *version);
AP_DECLARE(const char *) ap_get_server_banner(void);
AP_DECLARE(const char *) ap_get_server_description(void);
AP_DECLARE(void) ap_add_version_component(apr_pool_t *pconf, const char *component);
AP_DECLARE(const char *) ap_get_server_built(void);
#define OK 0
#define DECLINED -1
#define DONE -2
#define SUSPENDED -3
#define AP_NOBODY_WROTE -100
#define AP_NOBODY_READ -101
#define AP_FILTER_ERROR -102
#define RESPONSE_CODES 103
#define HTTP_CONTINUE 100
#define HTTP_SWITCHING_PROTOCOLS 101
#define HTTP_PROCESSING 102
#define HTTP_OK 200
#define HTTP_CREATED 201
#define HTTP_ACCEPTED 202
#define HTTP_NON_AUTHORITATIVE 203
#define HTTP_NO_CONTENT 204
#define HTTP_RESET_CONTENT 205
#define HTTP_PARTIAL_CONTENT 206
#define HTTP_MULTI_STATUS 207
#define HTTP_ALREADY_REPORTED 208
#define HTTP_IM_USED 226
#define HTTP_MULTIPLE_CHOICES 300
#define HTTP_MOVED_PERMANENTLY 301
#define HTTP_MOVED_TEMPORARILY 302
#define HTTP_SEE_OTHER 303
#define HTTP_NOT_MODIFIED 304
#define HTTP_USE_PROXY 305
#define HTTP_TEMPORARY_REDIRECT 307
#define HTTP_PERMANENT_REDIRECT 308
#define HTTP_BAD_REQUEST 400
#define HTTP_UNAUTHORIZED 401
#define HTTP_PAYMENT_REQUIRED 402
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_NOT_ACCEPTABLE 406
#define HTTP_PROXY_AUTHENTICATION_REQUIRED 407
#define HTTP_REQUEST_TIME_OUT 408
#define HTTP_CONFLICT 409
#define HTTP_GONE 410
#define HTTP_LENGTH_REQUIRED 411
#define HTTP_PRECONDITION_FAILED 412
#define HTTP_REQUEST_ENTITY_TOO_LARGE 413
#define HTTP_REQUEST_URI_TOO_LARGE 414
#define HTTP_UNSUPPORTED_MEDIA_TYPE 415
#define HTTP_RANGE_NOT_SATISFIABLE 416
#define HTTP_EXPECTATION_FAILED 417
#define HTTP_MISDIRECTED_REQUEST 421
#define HTTP_UNPROCESSABLE_ENTITY 422
#define HTTP_LOCKED 423
#define HTTP_FAILED_DEPENDENCY 424
#define HTTP_UPGRADE_REQUIRED 426
#define HTTP_PRECONDITION_REQUIRED 428
#define HTTP_TOO_MANY_REQUESTS 429
#define HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE 431
#define HTTP_UNAVAILABLE_FOR_LEGAL_REASONS 451
#define HTTP_INTERNAL_SERVER_ERROR 500
#define HTTP_NOT_IMPLEMENTED 501
#define HTTP_BAD_GATEWAY 502
#define HTTP_SERVICE_UNAVAILABLE 503
#define HTTP_GATEWAY_TIME_OUT 504
#define HTTP_VERSION_NOT_SUPPORTED 505
#define HTTP_VARIANT_ALSO_VARIES 506
#define HTTP_INSUFFICIENT_STORAGE 507
#define HTTP_LOOP_DETECTED 508
#define HTTP_NOT_EXTENDED 510
#define HTTP_NETWORK_AUTHENTICATION_REQUIRED 511
#define ap_is_HTTP_INFO(x) (((x) >= 100)&&((x) < 200))
#define ap_is_HTTP_SUCCESS(x) (((x) >= 200)&&((x) < 300))
#define ap_is_HTTP_REDIRECT(x) (((x) >= 300)&&((x) < 400))
#define ap_is_HTTP_ERROR(x) (((x) >= 400)&&((x) < 600))
#define ap_is_HTTP_CLIENT_ERROR(x) (((x) >= 400)&&((x) < 500))
#define ap_is_HTTP_SERVER_ERROR(x) (((x) >= 500)&&((x) < 600))
#define ap_is_HTTP_VALID_RESPONSE(x) (((x) >= 100)&&((x) < 600))
#define ap_status_drops_connection(x) (((x) == HTTP_BAD_REQUEST) || ((x) == HTTP_REQUEST_TIME_OUT) || ((x) == HTTP_LENGTH_REQUIRED) || ((x) == HTTP_REQUEST_ENTITY_TOO_LARGE) || ((x) == HTTP_REQUEST_URI_TOO_LARGE) || ((x) == HTTP_INTERNAL_SERVER_ERROR) || ((x) == HTTP_SERVICE_UNAVAILABLE) || ((x) == HTTP_NOT_IMPLEMENTED))
#define M_GET 0
#define M_PUT 1
#define M_POST 2
#define M_DELETE 3
#define M_CONNECT 4
#define M_OPTIONS 5
#define M_TRACE 6
#define M_PATCH 7
#define M_PROPFIND 8
#define M_PROPPATCH 9
#define M_MKCOL 10
#define M_COPY 11
#define M_MOVE 12
#define M_LOCK 13
#define M_UNLOCK 14
#define M_VERSION_CONTROL 15
#define M_CHECKOUT 16
#define M_UNCHECKOUT 17
#define M_CHECKIN 18
#define M_UPDATE 19
#define M_LABEL 20
#define M_REPORT 21
#define M_MKWORKSPACE 22
#define M_MKACTIVITY 23
#define M_BASELINE_CONTROL 24
#define M_MERGE 25
#define M_INVALID 26
#define METHODS 64
#define AP_METHOD_BIT ((apr_int64_t)1)
typedef struct ap_method_list_t ap_method_list_t;
struct ap_method_list_t {
apr_int64_t method_mask;
apr_array_header_t *method_list;
};
#define CGI_MAGIC_TYPE "application/x-httpd-cgi"
#define INCLUDES_MAGIC_TYPE "text/x-server-parsed-html"
#define INCLUDES_MAGIC_TYPE3 "text/x-server-parsed-html3"
#define DIR_MAGIC_TYPE "httpd/unix-directory"
#define AP_DEFAULT_HANDLER_NAME ""
#define AP_IS_DEFAULT_HANDLER_NAME(x) (*x == '\0')
#if !APR_CHARSET_EBCDIC
#define LF 10
#define CR 13
#define CRLF "\015\012"
#else
#define CR '\r'
#define LF '\n'
#define CRLF "\r\n"
#endif
#define CRLF_ASCII "\015\012"
#define REQUEST_NO_BODY 0
#define REQUEST_CHUNKED_ERROR 1
#define REQUEST_CHUNKED_DECHUNK 2
#define AP_REQ_ACCEPT_PATH_INFO 0
#define AP_REQ_REJECT_PATH_INFO 1
#define AP_REQ_DEFAULT_PATH_INFO 2
struct htaccess_result {
const char *dir;
int override;
int override_opts;
apr_table_t *override_list;
struct ap_conf_vector_t *htaccess;
const struct htaccess_result *next;
};
typedef struct process_rec process_rec;
typedef struct server_rec server_rec;
typedef struct conn_rec conn_rec;
typedef struct request_rec request_rec;
typedef struct conn_state_t conn_state_t;
#include "apr_uri.h"
struct process_rec {
apr_pool_t *pool;
apr_pool_t *pconf;
const char *short_name;
const char * const *argv;
int argc;
};
struct request_rec {
apr_pool_t *pool;
conn_rec *connection;
server_rec *server;
request_rec *next;
request_rec *prev;
request_rec *main;
char *the_request;
int assbackwards;
int proxyreq;
int header_only;
int proto_num;
char *protocol;
const char *hostname;
apr_time_t request_time;
const char *status_line;
int status;
int method_number;
const char *method;
apr_int64_t allowed;
apr_array_header_t *allowed_xmethods;
ap_method_list_t *allowed_methods;
apr_off_t sent_bodyct;
apr_off_t bytes_sent;
apr_time_t mtime;
const char *range;
apr_off_t clength;
int chunked;
int read_body;
int read_chunked;
unsigned expecting_100;
apr_bucket_brigade *kept_body;
apr_table_t *body_table;
apr_off_t remaining;
apr_off_t read_length;
apr_table_t *headers_in;
apr_table_t *headers_out;
apr_table_t *err_headers_out;
apr_table_t *subprocess_env;
apr_table_t *notes;
const char *content_type;
const char *handler;
const char *content_encoding;
apr_array_header_t *content_languages;
char *vlist_validator;
char *user;
char *ap_auth_type;
char *unparsed_uri;
char *uri;
char *filename;
char *canonical_filename;
char *path_info;
char *args;
int used_path_info;
int eos_sent;
struct ap_conf_vector_t *per_dir_config;
struct ap_conf_vector_t *request_config;
const struct ap_logconf *log;
const char *log_id;
const struct htaccess_result *htaccess;
struct ap_filter_t *output_filters;
struct ap_filter_t *input_filters;
struct ap_filter_t *proto_output_filters;
struct ap_filter_t *proto_input_filters;
int no_cache;
int no_local_copy;
apr_thread_mutex_t *invoke_mtx;
apr_uri_t parsed_uri;
apr_finfo_t finfo;
apr_sockaddr_t *useragent_addr;
char *useragent_ip;
apr_table_t *trailers_in;
apr_table_t *trailers_out;
char *useragent_host;
int double_reverse;
};
#define PROXYREQ_NONE 0
#define PROXYREQ_PROXY 1
#define PROXYREQ_REVERSE 2
#define PROXYREQ_RESPONSE 3
typedef enum {
AP_CONN_UNKNOWN,
AP_CONN_CLOSE,
AP_CONN_KEEPALIVE
} ap_conn_keepalive_e;
struct conn_rec {
apr_pool_t *pool;
server_rec *base_server;
void *vhost_lookup_data;
apr_sockaddr_t *local_addr;
apr_sockaddr_t *client_addr;
char *client_ip;
char *remote_host;
char *remote_logname;
char *local_ip;
char *local_host;
long id;
struct ap_conf_vector_t *conn_config;
apr_table_t *notes;
struct ap_filter_t *input_filters;
struct ap_filter_t *output_filters;
void *sbh;
struct apr_bucket_alloc_t *bucket_alloc;
conn_state_t *cs;
int data_in_input_filters;
int data_in_output_filters;
unsigned int clogging_input_filters:1;
signed int double_reverse:2;
unsigned aborted;
ap_conn_keepalive_e keepalive;
int keepalives;
const struct ap_logconf *log;
const char *log_id;
#if APR_HAS_THREADS
apr_thread_t *current_thread;
#endif
conn_rec *master;
};
typedef enum {
CONN_STATE_CHECK_REQUEST_LINE_READABLE,
CONN_STATE_READ_REQUEST_LINE,
CONN_STATE_HANDLER,
CONN_STATE_WRITE_COMPLETION,
CONN_STATE_SUSPENDED,
CONN_STATE_LINGER,
CONN_STATE_LINGER_NORMAL,
CONN_STATE_LINGER_SHORT
} conn_state_e;
typedef enum {
CONN_SENSE_DEFAULT,
CONN_SENSE_WANT_READ,
CONN_SENSE_WANT_WRITE
} conn_sense_e;
struct conn_state_t {
conn_state_e state;
conn_sense_e sense;
};
#define DEFAULT_VHOST_ADDR 0xfffffffful
typedef struct server_addr_rec server_addr_rec;
struct server_addr_rec {
server_addr_rec *next;
char *virthost;
apr_sockaddr_t *host_addr;
apr_port_t host_port;
};
struct ap_logconf {
signed char *module_levels;
int level;
};
struct server_rec {
process_rec *process;
server_rec *next;
char *error_fname;
apr_file_t *error_log;
struct ap_logconf log;
struct ap_conf_vector_t *module_config;
struct ap_conf_vector_t *lookup_defaults;
const char *defn_name;
unsigned defn_line_number;
char is_virtual;
apr_port_t port;
const char *server_scheme;
char *server_admin;
char *server_hostname;
server_addr_rec *addrs;
apr_interval_time_t timeout;
apr_interval_time_t keep_alive_timeout;
int keep_alive_max;
int keep_alive;
apr_array_header_t *names;
apr_array_header_t *wild_names;
const char *path;
int pathlen;
int limit_req_line;
int limit_req_fieldsize;
int limit_req_fields;
void *context;
unsigned int keep_alive_timeout_set:1;
};
typedef struct ap_sload_t ap_sload_t;
struct ap_sload_t {
int idle;
int busy;
apr_off_t bytes_served;
unsigned long access_count;
};
typedef struct ap_loadavg_t ap_loadavg_t;
struct ap_loadavg_t {
float loadavg;
float loadavg5;
float loadavg15;
};
AP_DECLARE(const char *) ap_context_document_root(request_rec *r);
AP_DECLARE(const char *) ap_context_prefix(request_rec *r);
AP_DECLARE(void) ap_set_context_info(request_rec *r, const char *prefix,
const char *document_root);
AP_DECLARE(void) ap_set_document_root(request_rec *r, const char *document_root);
AP_DECLARE(char *) ap_field_noparam(apr_pool_t *p, const char *intype);
AP_DECLARE(char *) ap_ht_time(apr_pool_t *p, apr_time_t t, const char *fmt, int gmt);
AP_DECLARE(char *) ap_getword(apr_pool_t *p, const char **line, char stop);
AP_DECLARE(char *) ap_getword_nc(apr_pool_t *p, char **line, char stop);
AP_DECLARE(char *) ap_getword_white(apr_pool_t *p, const char **line);
AP_DECLARE(char *) ap_getword_white_nc(apr_pool_t *p, char **line);
AP_DECLARE(char *) ap_getword_nulls(apr_pool_t *p, const char **line,
char stop);
AP_DECLARE(char *) ap_getword_nulls_nc(apr_pool_t *p, char **line, char stop);
AP_DECLARE(char *) ap_getword_conf(apr_pool_t *p, const char **line);
AP_DECLARE(char *) ap_getword_conf_nc(apr_pool_t *p, char **line);
AP_DECLARE(char *) ap_getword_conf2(apr_pool_t *p, const char **line);
AP_DECLARE(char *) ap_getword_conf2_nc(apr_pool_t *p, char **line);
AP_DECLARE(const char *) ap_resolve_env(apr_pool_t *p, const char * word);
AP_DECLARE(const char *) ap_size_list_item(const char **field, int *len);
AP_DECLARE(char *) ap_get_list_item(apr_pool_t *p, const char **field);
AP_DECLARE(int) ap_find_list_item(apr_pool_t *p, const char *line, const char *tok);
AP_DECLARE(int) ap_find_etag_weak(apr_pool_t *p, const char *line, const char *tok);
AP_DECLARE(int) ap_find_etag_strong(apr_pool_t *p, const char *line, const char *tok);
AP_DECLARE(const char *) ap_scan_http_field_content(const char *ptr);
AP_DECLARE(const char *) ap_scan_http_token(const char *ptr);
AP_DECLARE(const char *) ap_scan_vchar_obstext(const char *ptr);
AP_DECLARE(const char *) ap_parse_token_list_strict(apr_pool_t *p, const char *tok,
apr_array_header_t **tokens,
int skip_invalid);
AP_DECLARE(char *) ap_get_token(apr_pool_t *p, const char **accept_line, int accept_white);
AP_DECLARE(int) ap_find_token(apr_pool_t *p, const char *line, const char *tok);
AP_DECLARE(int) ap_find_last_token(apr_pool_t *p, const char *line, const char *tok);
AP_DECLARE(int) ap_is_url(const char *u);
AP_DECLARE(int) ap_unescape_all(char *url);
AP_DECLARE(int) ap_unescape_url(char *url);
AP_DECLARE(int) ap_unescape_url_keep2f(char *url, int decode_slashes);
AP_DECLARE(int) ap_unescape_urlencoded(char *query);
AP_DECLARE(void) ap_no2slash(char *name);
AP_DECLARE(void) ap_getparents(char *name);
AP_DECLARE(char *) ap_escape_path_segment(apr_pool_t *p, const char *s);
AP_DECLARE(char *) ap_escape_path_segment_buffer(char *c, const char *s);
AP_DECLARE(char *) ap_os_escape_path(apr_pool_t *p, const char *path, int partial);
#define ap_escape_uri(ppool,path) ap_os_escape_path(ppool,path,1)
AP_DECLARE(char *) ap_escape_urlencoded(apr_pool_t *p, const char *s);
AP_DECLARE(char *) ap_escape_urlencoded_buffer(char *c, const char *s);
#define ap_escape_html(p,s) ap_escape_html2(p,s,0)
AP_DECLARE(char *) ap_escape_html2(apr_pool_t *p, const char *s, int toasc);
AP_DECLARE(char *) ap_escape_logitem(apr_pool_t *p, const char *str);
AP_DECLARE(apr_size_t) ap_escape_errorlog_item(char *dest, const char *source,
apr_size_t buflen);
AP_DECLARE(char *) ap_construct_server(apr_pool_t *p, const char *hostname,
apr_port_t port, const request_rec *r);
AP_DECLARE(char *) ap_escape_shell_cmd(apr_pool_t *p, const char *s);
AP_DECLARE(int) ap_count_dirs(const char *path);
AP_DECLARE(char *) ap_make_dirstr_prefix(char *d, const char *s, int n);
AP_DECLARE(char *) ap_make_dirstr_parent(apr_pool_t *p, const char *s);
AP_DECLARE(char *) ap_make_full_path(apr_pool_t *a, const char *dir, const char *f);
AP_DECLARE(int) ap_os_is_path_absolute(apr_pool_t *p, const char *dir);
AP_DECLARE(int) ap_is_matchexp(const char *str);
AP_DECLARE(int) ap_strcmp_match(const char *str, const char *expected);
AP_DECLARE(int) ap_strcasecmp_match(const char *str, const char *expected);
AP_DECLARE(char *) ap_strcasestr(const char *s1, const char *s2);
AP_DECLARE(const char *) ap_stripprefix(const char *bigstring,
const char *prefix);
AP_DECLARE(char *) ap_pbase64decode(apr_pool_t *p, const char *bufcoded);
AP_DECLARE(char *) ap_pbase64encode(apr_pool_t *p, char *string);
AP_DECLARE(ap_regex_t *) ap_pregcomp(apr_pool_t *p, const char *pattern,
int cflags);
AP_DECLARE(void) ap_pregfree(apr_pool_t *p, ap_regex_t *reg);
AP_DECLARE(char *) ap_pregsub(apr_pool_t *p, const char *input,
const char *source, apr_size_t nmatch,
ap_regmatch_t pmatch[]);
AP_DECLARE(apr_status_t) ap_pregsub_ex(apr_pool_t *p, char **result,
const char *input, const char *source,
apr_size_t nmatch,
ap_regmatch_t pmatch[],
apr_size_t maxlen);
AP_DECLARE(void) ap_content_type_tolower(char *s);
AP_DECLARE(void) ap_str_tolower(char *s);
AP_DECLARE(void) ap_str_toupper(char *s);
AP_DECLARE(int) ap_ind(const char *str, char c);
AP_DECLARE(int) ap_rind(const char *str, char c);
AP_DECLARE(char *) ap_escape_quotes(apr_pool_t *p, const char *instring);
AP_DECLARE(char *) ap_append_pid(apr_pool_t *p, const char *string,
const char *delim);
AP_DECLARE(apr_status_t) ap_timeout_parameter_parse(
const char *timeout_parameter,
apr_interval_time_t *timeout,
const char *default_time_unit);
AP_DECLARE(int) ap_request_has_body(request_rec *r);
AP_DECLARE(apr_status_t) ap_pstr2_alnum(apr_pool_t *p, const char *src,
const char **dest);
AP_DECLARE(apr_status_t) ap_str2_alnum(const char *src, char *dest);
typedef struct {
const char *name;
apr_bucket_brigade *value;
} ap_form_pair_t;
AP_DECLARE(int) ap_parse_form_data(request_rec *r, struct ap_filter_t *f,
apr_array_header_t **ptr,
apr_size_t num, apr_size_t size);
AP_DECLARE(int) ap_is_rdirectory(apr_pool_t *p, const char *name);
AP_DECLARE(int) ap_is_directory(apr_pool_t *p, const char *name);
#if defined(_OSD_POSIX)
extern int os_init_job_environment(server_rec *s, const char *user_name, int one_process);
#endif
char *ap_get_local_host(apr_pool_t *p);
AP_DECLARE(void) ap_log_assert(const char *szExp, const char *szFile, int nLine)
__attribute__((noreturn));
#define ap_assert(exp) ((exp) ? (void)0 : ap_log_assert(#exp,__FILE__,__LINE__))
#if defined(AP_DEBUG)
#define AP_DEBUG_ASSERT(exp) ap_assert(exp)
#else
#define AP_DEBUG_ASSERT(exp) ((void)0)
#endif
#define SIGSTOP_DETACH 1
#define SIGSTOP_MAKE_CHILD 2
#define SIGSTOP_SPAWN_CHILD 4
#define SIGSTOP_PIPED_LOG_SPAWN 8
#define SIGSTOP_CGI_CHILD 16
#if defined(DEBUG_SIGSTOP)
extern int raise_sigstop_flags;
#define RAISE_SIGSTOP(x) do { if (raise_sigstop_flags & SIGSTOP_##x) raise(SIGSTOP);} while (0)
#else
#define RAISE_SIGSTOP(x)
#endif
AP_DECLARE(const char *) ap_psignature(const char *prefix, request_rec *r);
#include <string.h>
AP_DECLARE(char *) ap_strchr(char *s, int c);
AP_DECLARE(const char *) ap_strchr_c(const char *s, int c);
AP_DECLARE(char *) ap_strrchr(char *s, int c);
AP_DECLARE(const char *) ap_strrchr_c(const char *s, int c);
AP_DECLARE(char *) ap_strstr(char *s, const char *c);
AP_DECLARE(const char *) ap_strstr_c(const char *s, const char *c);
#if defined(AP_DEBUG)
#undef strchr
#define strchr(s, c) ap_strchr(s,c)
#undef strrchr
#define strrchr(s, c) ap_strrchr(s,c)
#undef strstr
#define strstr(s, c) ap_strstr(s,c)
#else
#define ap_strchr(s, c) strchr(s, c)
#define ap_strchr_c(s, c) strchr(s, c)
#define ap_strrchr(s, c) strrchr(s, c)
#define ap_strrchr_c(s, c) strrchr(s, c)
#define ap_strstr(s, c) strstr(s, c)
#define ap_strstr_c(s, c) strstr(s, c)
#endif
AP_DECLARE(void) ap_random_insecure_bytes(void *buf, apr_size_t size);
AP_DECLARE(apr_uint32_t) ap_random_pick(apr_uint32_t min, apr_uint32_t max);
AP_DECLARE(void) ap_abort_on_oom(void) __attribute__((noreturn));
AP_DECLARE(void *) ap_malloc(size_t size)
__attribute__((malloc))
AP_FN_ATTR_ALLOC_SIZE(1);
AP_DECLARE(void *) ap_calloc(size_t nelem, size_t size)
__attribute__((malloc))
AP_FN_ATTR_ALLOC_SIZE2(1,2);
AP_DECLARE(void *) ap_realloc(void *ptr, size_t size)
AP_FN_ATTR_WARN_UNUSED_RESULT
AP_FN_ATTR_ALLOC_SIZE(2);
AP_DECLARE(void) ap_get_sload(ap_sload_t *ld);
AP_DECLARE(void) ap_get_loadavg(ap_loadavg_t *ld);
AP_DECLARE(void) ap_bin2hex(const void *src, apr_size_t srclen, char *dest);
AP_DECLARE(char *) ap_get_exec_line(apr_pool_t *p,
const char *cmd,
const char * const *argv);
#define AP_NORESTART APR_OS_START_USEERR + 1
AP_DECLARE(int) ap_array_str_index(const apr_array_header_t *array,
const char *s,
int start);
AP_DECLARE(int) ap_array_str_contains(const apr_array_header_t *array,
const char *s);
AP_DECLARE(int) ap_cstr_casecmp(const char *s1, const char *s2);
AP_DECLARE(int) ap_cstr_casecmpn(const char *s1, const char *s2, apr_size_t n);
#if defined(__cplusplus)
}
#endif
#endif