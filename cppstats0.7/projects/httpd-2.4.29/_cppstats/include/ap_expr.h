#if !defined(AP_EXPR_H)
#define AP_EXPR_H
#include "httpd.h"
#include "http_config.h"
#include "ap_regex.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct ap_expr_node ap_expr_t;
typedef struct {
ap_expr_t *root_node;
const char *filename;
unsigned int line_number;
unsigned int flags;
int module_index;
} ap_expr_info_t;
#define AP_EXPR_FLAG_SSL_EXPR_COMPAT 1
#define AP_EXPR_FLAG_DONT_VARY 2
#define AP_EXPR_FLAG_RESTRICTED 4
#define AP_EXPR_FLAG_STRING_RESULT 8
AP_DECLARE(int) ap_expr_exec(request_rec *r, const ap_expr_info_t *expr,
const char **err);
AP_DECLARE(int) ap_expr_exec_re(request_rec *r, const ap_expr_info_t *expr,
apr_size_t nmatch, ap_regmatch_t *pmatch,
const char **source, const char **err);
typedef struct {
request_rec *r;
conn_rec *c;
server_rec *s;
apr_pool_t *p;
const char **err;
const ap_expr_info_t *info;
ap_regmatch_t *re_pmatch;
apr_size_t re_nmatch;
const char **re_source;
const char **vary_this;
const char **result_string;
void *data;
int reclvl;
} ap_expr_eval_ctx_t;
AP_DECLARE(int) ap_expr_exec_ctx(ap_expr_eval_ctx_t *ctx);
AP_DECLARE(const char *) ap_expr_str_exec(request_rec *r,
const ap_expr_info_t *expr,
const char **err);
AP_DECLARE(const char *) ap_expr_str_exec_re(request_rec *r,
const ap_expr_info_t *expr,
apr_size_t nmatch,
ap_regmatch_t *pmatch,
const char **source,
const char **err);
typedef int ap_expr_op_unary_t(ap_expr_eval_ctx_t *ctx, const void *data,
const char *arg);
typedef int ap_expr_op_binary_t(ap_expr_eval_ctx_t *ctx, const void *data,
const char *arg1, const char *arg2);
typedef const char *(ap_expr_string_func_t)(ap_expr_eval_ctx_t *ctx,
const void *data,
const char *arg);
typedef apr_array_header_t *(ap_expr_list_func_t)(ap_expr_eval_ctx_t *ctx,
const void *data,
const char *arg);
typedef const char *(ap_expr_var_func_t)(ap_expr_eval_ctx_t *ctx,
const void *data);
typedef struct {
int type;
#define AP_EXPR_FUNC_VAR 0
#define AP_EXPR_FUNC_STRING 1
#define AP_EXPR_FUNC_LIST 2
#define AP_EXPR_FUNC_OP_UNARY 3
#define AP_EXPR_FUNC_OP_BINARY 4
const char *name;
int flags;
apr_pool_t *pool;
apr_pool_t *ptemp;
const void **func;
const void **data;
const char **err;
const char *arg;
} ap_expr_lookup_parms;
typedef int (ap_expr_lookup_fn_t)(ap_expr_lookup_parms *parms);
AP_DECLARE_NONSTD(int) ap_expr_lookup_default(ap_expr_lookup_parms *parms);
AP_DECLARE_HOOK(int, expr_lookup, (ap_expr_lookup_parms *parms))
AP_DECLARE(const char *) ap_expr_parse(apr_pool_t *pool, apr_pool_t *ptemp,
ap_expr_info_t *info, const char *expr,
ap_expr_lookup_fn_t *lookup_fn);
AP_DECLARE(ap_expr_info_t *) ap_expr_parse_cmd_mi(const cmd_parms *cmd,
const char *expr,
unsigned int flags,
const char **err,
ap_expr_lookup_fn_t *lookup_fn,
int module_index);
#define ap_expr_parse_cmd(cmd, expr, flags, err, lookup_fn) ap_expr_parse_cmd_mi(cmd, expr, flags, err, lookup_fn, APLOG_MODULE_INDEX)
void ap_expr_init(apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif