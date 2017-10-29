#if !defined(APACHE_HTTP_CORE_H)
#define APACHE_HTTP_CORE_H
#include "apr.h"
#include "apr_hash.h"
#include "apr_optional.h"
#include "util_filter.h"
#include "ap_expr.h"
#include "apr_tables.h"
#include "http_config.h"
#if APR_HAVE_STRUCT_RLIMIT
#include <sys/time.h>
#include <sys/resource.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif
#define OPT_NONE 0
#define OPT_INDEXES 1
#define OPT_INCLUDES 2
#define OPT_SYM_LINKS 4
#define OPT_EXECCGI 8
#define OPT_UNSET 16
#define OPT_INC_WITH_EXEC 32
#define OPT_SYM_OWNER 64
#define OPT_MULTI 128
#define OPT_ALL (OPT_INDEXES|OPT_INCLUDES|OPT_INC_WITH_EXEC|OPT_SYM_LINKS|OPT_EXECCGI)
#define REMOTE_HOST (0)
#define REMOTE_NAME (1)
#define REMOTE_NOLOOKUP (2)
#define REMOTE_DOUBLE_REV (3)
#define SATISFY_ALL 0
#define SATISFY_ANY 1
#define SATISFY_NOSPEC 2
#define AP_MIN_BYTES_TO_WRITE 8000
#define AP_DEFAULT_MAX_INTERNAL_REDIRECTS 10
#define AP_DEFAULT_MAX_SUBREQ_DEPTH 10
AP_DECLARE(int) ap_allow_options(request_rec *r);
AP_DECLARE(int) ap_allow_overrides(request_rec *r);
AP_DECLARE(const char *) ap_document_root(request_rec *r);
AP_DECLARE(const char *) ap_get_useragent_host(request_rec *req, int type,
int *str_is_ip);
AP_DECLARE(const char *) ap_get_remote_host(conn_rec *conn, void *dir_config, int type, int *str_is_ip);
AP_DECLARE(const char *) ap_get_remote_logname(request_rec *r);
AP_DECLARE(char *) ap_construct_url(apr_pool_t *p, const char *uri, request_rec *r);
AP_DECLARE(const char *) ap_get_server_name(request_rec *r);
AP_DECLARE(const char *) ap_get_server_name_for_url(request_rec *r);
AP_DECLARE(apr_port_t) ap_get_server_port(const request_rec *r);
AP_DECLARE(apr_off_t) ap_get_limit_req_body(const request_rec *r);
AP_DECLARE(apr_size_t) ap_get_limit_xml_body(const request_rec *r);
AP_DECLARE(void) ap_custom_response(request_rec *r, int status, const char *string);
AP_DECLARE(int) ap_is_recursion_limit_exceeded(const request_rec *r);
AP_DECLARE(int) ap_exists_config_define(const char *name);
AP_DECLARE_NONSTD(int) ap_core_translate(request_rec *r);
typedef struct require_line require_line;
struct require_line {
apr_int64_t method_mask;
char *requirement;
};
AP_DECLARE(const char *) ap_auth_type(request_rec *r);
AP_DECLARE(const char *) ap_auth_name(request_rec *r);
AP_DECLARE(int) ap_satisfies(request_rec *r);
AP_DECLARE_DATA extern module core_module;
AP_DECLARE(void *) ap_get_core_module_config(const ap_conf_vector_t *cv);
AP_DECLARE(void) ap_set_core_module_config(ap_conf_vector_t *cv, void *val);
AP_DECLARE(apr_socket_t *) ap_get_conn_socket(conn_rec *c);
#if !defined(AP_DEBUG)
#define AP_CORE_MODULE_INDEX 0
#define ap_get_core_module_config(v) (((void **)(v))[AP_CORE_MODULE_INDEX])
#define ap_set_core_module_config(v, val) ((((void **)(v))[AP_CORE_MODULE_INDEX]) = (val))
#else
#define AP_CORE_MODULE_INDEX (AP_DEBUG_ASSERT(core_module.module_index == 0), 0)
#endif
typedef struct {
struct apr_bucket_brigade *bb;
void **notes;
char **response_code_strings;
const char *document_root;
const char *context_document_root;
const char *context_prefix;
int deliver_script;
int suppress_charset;
} core_request_config;
#define AP_NOTE_DIRECTORY_WALK 0
#define AP_NOTE_LOCATION_WALK 1
#define AP_NOTE_FILE_WALK 2
#define AP_NOTE_IF_WALK 3
#define AP_NUM_STD_NOTES 4
AP_DECLARE(apr_size_t) ap_register_request_note(void);
AP_DECLARE(void **) ap_get_request_note(request_rec *r, apr_size_t note_num);
typedef unsigned char allow_options_t;
typedef unsigned int overrides_t;
typedef unsigned long etag_components_t;
#define ETAG_UNSET 0
#define ETAG_NONE (1 << 0)
#define ETAG_MTIME (1 << 1)
#define ETAG_INODE (1 << 2)
#define ETAG_SIZE (1 << 3)
#define ETAG_ALL (ETAG_MTIME | ETAG_INODE | ETAG_SIZE)
#define ETAG_BACKWARD (ETAG_MTIME | ETAG_SIZE)
#define AP_CORE_CONFIG_OFF (0)
#define AP_CORE_CONFIG_ON (1)
#define AP_CORE_CONFIG_UNSET (2)
#define AP_CORE_MERGE_FLAG(field, to, base, over) to->field = over->field != AP_CORE_CONFIG_UNSET ? over->field : base->field
typedef enum {
srv_sig_unset,
srv_sig_off,
srv_sig_on,
srv_sig_withmail
} server_signature_e;
typedef struct {
char *d;
unsigned d_components;
allow_options_t opts;
allow_options_t opts_add;
allow_options_t opts_remove;
overrides_t override;
allow_options_t override_opts;
char **response_code_strings;
#define HOSTNAME_LOOKUP_OFF 0
#define HOSTNAME_LOOKUP_ON 1
#define HOSTNAME_LOOKUP_DOUBLE 2
#define HOSTNAME_LOOKUP_UNSET 3
unsigned int hostname_lookups : 4;
unsigned int content_md5 : 2;
#define USE_CANONICAL_NAME_OFF (0)
#define USE_CANONICAL_NAME_ON (1)
#define USE_CANONICAL_NAME_DNS (2)
#define USE_CANONICAL_NAME_UNSET (3)
unsigned use_canonical_name : 2;
unsigned d_is_fnmatch : 1;
#define ADD_DEFAULT_CHARSET_OFF (0)
#define ADD_DEFAULT_CHARSET_ON (1)
#define ADD_DEFAULT_CHARSET_UNSET (2)
unsigned add_default_charset : 2;
const char *add_default_charset_name;
#if defined(RLIMIT_CPU)
struct rlimit *limit_cpu;
#endif
#if defined (RLIMIT_DATA) || defined (RLIMIT_VMEM) || defined(RLIMIT_AS)
struct rlimit *limit_mem;
#endif
#if defined(RLIMIT_NPROC)
struct rlimit *limit_nproc;
#endif
apr_off_t limit_req_body;
long limit_xml_body;
server_signature_e server_signature;
apr_array_header_t *sec_file;
apr_array_header_t *sec_if;
ap_regex_t *r;
const char *mime_type;
const char *handler;
const char *output_filters;
const char *input_filters;
int accept_path_info;
etag_components_t etag_bits;
etag_components_t etag_add;
etag_components_t etag_remove;
#define ENABLE_MMAP_OFF (0)
#define ENABLE_MMAP_ON (1)
#define ENABLE_MMAP_UNSET (2)
unsigned int enable_mmap : 2;
#define ENABLE_SENDFILE_OFF (0)
#define ENABLE_SENDFILE_ON (1)
#define ENABLE_SENDFILE_UNSET (2)
unsigned int enable_sendfile : 2;
#define USE_CANONICAL_PHYS_PORT_OFF (0)
#define USE_CANONICAL_PHYS_PORT_ON (1)
#define USE_CANONICAL_PHYS_PORT_UNSET (2)
unsigned int use_canonical_phys_port : 2;
unsigned int allow_encoded_slashes : 1;
unsigned int decode_encoded_slashes : 1;
#define AP_CONDITION_IF 1
#define AP_CONDITION_ELSE 2
#define AP_CONDITION_ELSEIF (AP_CONDITION_ELSE|AP_CONDITION_IF)
unsigned int condition_ifelse : 2;
ap_expr_info_t *condition;
struct ap_logconf *log;
apr_table_t *override_list;
#define AP_MAXRANGES_UNSET -1
#define AP_MAXRANGES_DEFAULT -2
#define AP_MAXRANGES_UNLIMITED -3
#define AP_MAXRANGES_NORANGES 0
int max_ranges;
int max_overlaps;
int max_reversals;
apr_array_header_t *refs;
apr_hash_t *response_code_exprs;
#define AP_CGI_PASS_AUTH_OFF (0)
#define AP_CGI_PASS_AUTH_ON (1)
#define AP_CGI_PASS_AUTH_UNSET (2)
unsigned int cgi_pass_auth : 2;
unsigned int qualify_redirect_url :2;
ap_expr_info_t *expr_handler;
apr_hash_t *cgi_var_rules;
} core_dir_config;
#define AP_SENDFILE_ENABLED(x) ((x) == ENABLE_SENDFILE_ON ? APR_SENDFILE_ENABLED : 0)
typedef struct {
char *gprof_dir;
const char *ap_document_root;
char *access_name;
apr_array_header_t *sec_dir;
apr_array_header_t *sec_url;
int redirect_limit;
int subreq_limit;
const char *protocol;
apr_table_t *accf_map;
apr_array_header_t *error_log_format;
apr_array_header_t *error_log_conn;
apr_array_header_t *error_log_req;
#define AP_TRACE_UNSET -1
#define AP_TRACE_DISABLE 0
#define AP_TRACE_ENABLE 1
#define AP_TRACE_EXTENDED 2
int trace_enable;
#define AP_MERGE_TRAILERS_UNSET 0
#define AP_MERGE_TRAILERS_ENABLE 1
#define AP_MERGE_TRAILERS_DISABLE 2
int merge_trailers;
apr_array_header_t *protocols;
int protocols_honor_order;
#define AP_HTTP09_UNSET 0
#define AP_HTTP09_ENABLE 1
#define AP_HTTP09_DISABLE 2
char http09_enable;
#define AP_HTTP_CONFORMANCE_UNSET 0
#define AP_HTTP_CONFORMANCE_UNSAFE 1
#define AP_HTTP_CONFORMANCE_STRICT 2
char http_conformance;
#define AP_HTTP_METHODS_UNSET 0
#define AP_HTTP_METHODS_LENIENT 1
#define AP_HTTP_METHODS_REGISTERED 2
char http_methods;
} core_server_config;
void ap_add_output_filters_by_type(request_rec *r);
void ap_core_reorder_directories(apr_pool_t *, server_rec *);
AP_CORE_DECLARE(void) ap_add_per_dir_conf(server_rec *s, void *dir_config);
AP_CORE_DECLARE(void) ap_add_per_url_conf(server_rec *s, void *url_config);
AP_CORE_DECLARE(void) ap_add_file_conf(apr_pool_t *p, core_dir_config *conf, void *url_config);
AP_CORE_DECLARE(const char *) ap_add_if_conf(apr_pool_t *p, core_dir_config *conf, void *url_config);
AP_CORE_DECLARE_NONSTD(const char *) ap_limit_section(cmd_parms *cmd, void *dummy, const char *arg);
apr_status_t ap_core_input_filter(ap_filter_t *f, apr_bucket_brigade *b,
ap_input_mode_t mode, apr_read_type_e block,
apr_off_t readbytes);
apr_status_t ap_core_output_filter(ap_filter_t *f, apr_bucket_brigade *b);
AP_DECLARE(const char*) ap_get_server_protocol(server_rec* s);
AP_DECLARE(void) ap_set_server_protocol(server_rec* s, const char* proto);
typedef struct core_output_filter_ctx core_output_filter_ctx_t;
typedef struct core_filter_ctx core_ctx_t;
typedef struct core_net_rec {
apr_socket_t *client_socket;
conn_rec *c;
core_output_filter_ctx_t *out_ctx;
core_ctx_t *in_ctx;
} core_net_rec;
AP_DECLARE_HOOK(apr_status_t, insert_network_bucket,
(conn_rec *c, apr_bucket_brigade *bb, apr_socket_t *socket))
typedef enum {
ap_mgmt_type_string,
ap_mgmt_type_long,
ap_mgmt_type_hash
} ap_mgmt_type_e;
typedef union {
const char *s_value;
long i_value;
apr_hash_t *h_value;
} ap_mgmt_value;
typedef struct {
const char *description;
const char *name;
ap_mgmt_type_e vtype;
ap_mgmt_value v;
} ap_mgmt_item_t;
AP_DECLARE_DATA extern ap_filter_rec_t *ap_subreq_core_filter_handle;
AP_DECLARE_DATA extern ap_filter_rec_t *ap_core_output_filter_handle;
AP_DECLARE_DATA extern ap_filter_rec_t *ap_content_length_filter_handle;
AP_DECLARE_DATA extern ap_filter_rec_t *ap_core_input_filter_handle;
AP_DECLARE_HOOK(int, get_mgmt_items,
(apr_pool_t *p, const char * val, apr_hash_t *ht))
APR_DECLARE_OPTIONAL_FN(void, ap_logio_add_bytes_out,
(conn_rec *c, apr_off_t bytes));
APR_DECLARE_OPTIONAL_FN(void, ap_logio_add_bytes_in,
(conn_rec *c, apr_off_t bytes));
APR_DECLARE_OPTIONAL_FN(apr_off_t, ap_logio_get_last_bytes, (conn_rec *c));
typedef struct ap_errorlog_info {
const server_rec *s;
const conn_rec *c;
const request_rec *r;
const request_rec *rmain;
apr_pool_t *pool;
const char *file;
int line;
int module_index;
int level;
apr_status_t status;
int using_syslog;
int startup;
const char *format;
} ap_errorlog_info;
typedef int ap_errorlog_handler_fn_t(const ap_errorlog_info *info,
const char *arg, char *buf, int buflen);
AP_DECLARE(void) ap_register_errorlog_handler(apr_pool_t *p, char *tag,
ap_errorlog_handler_fn_t *handler,
int flags);
typedef struct ap_errorlog_handler {
ap_errorlog_handler_fn_t *func;
int flags;
} ap_errorlog_handler;
#define AP_ERRORLOG_FLAG_FIELD_SEP 1
#define AP_ERRORLOG_FLAG_MESSAGE 2
#define AP_ERRORLOG_FLAG_REQUIRED 4
#define AP_ERRORLOG_FLAG_NULL_AS_HYPHEN 8
typedef struct {
ap_errorlog_handler_fn_t *func;
const char *arg;
unsigned int flags;
unsigned int min_loglevel;
} ap_errorlog_format_item;
AP_DECLARE_HOOK(void, error_log, (const ap_errorlog_info *info,
const char *errstr))
AP_CORE_DECLARE(void) ap_register_log_hooks(apr_pool_t *p);
AP_CORE_DECLARE(void) ap_register_config_hooks(apr_pool_t *p);
APR_DECLARE_OPTIONAL_FN(const char *, ap_ident_lookup,
(request_rec *r));
APR_DECLARE_OPTIONAL_FN(int, authz_some_auth_required, (request_rec *r));
APR_DECLARE_OPTIONAL_FN(const char *, authn_ap_auth_type, (request_rec *r));
APR_DECLARE_OPTIONAL_FN(const char *, authn_ap_auth_name, (request_rec *r));
APR_DECLARE_OPTIONAL_FN(int, access_compat_ap_satisfies, (request_rec *r));
AP_DECLARE(int) ap_state_query(int query_code);
#define AP_SQ_MAIN_STATE 0
#define AP_SQ_RUN_MODE 1
#define AP_SQ_CONFIG_GEN 2
#define AP_SQ_NOT_SUPPORTED -1
#define AP_SQ_MS_INITIAL_STARTUP 1
#define AP_SQ_MS_CREATE_PRE_CONFIG 2
#define AP_SQ_MS_DESTROY_CONFIG 3
#define AP_SQ_MS_CREATE_CONFIG 4
#define AP_SQ_MS_RUN_MPM 5
#define AP_SQ_MS_EXITING 6
#define AP_SQ_RM_UNKNOWN 1
#define AP_SQ_RM_NORMAL 2
#define AP_SQ_RM_CONFIG_TEST 3
#define AP_SQ_RM_CONFIG_DUMP 4
#if defined(__cplusplus)
}
#endif
#endif