#if !defined(_MOD_CGI_H)
#define _MOD_CGI_H 1
#include "mod_include.h"
typedef enum {RUN_AS_SSI, RUN_AS_CGI} prog_types;
typedef struct {
apr_int32_t in_pipe;
apr_int32_t out_pipe;
apr_int32_t err_pipe;
int process_cgi;
apr_cmdtype_e cmd_type;
apr_int32_t detached;
prog_types prog_type;
apr_bucket_brigade **bb;
include_ctx_t *ctx;
ap_filter_t *next;
apr_int32_t addrspace;
} cgi_exec_info_t;
APR_DECLARE_OPTIONAL_FN(apr_status_t, ap_cgi_build_command,
(const char **cmd, const char ***argv,
request_rec *r, apr_pool_t *p,
cgi_exec_info_t *e_info));
#endif
