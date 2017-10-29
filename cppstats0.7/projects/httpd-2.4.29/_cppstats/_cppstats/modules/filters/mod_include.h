#if !defined(_MOD_INCLUDE_H)
#define _MOD_INCLUDE_H 1
#include "apr_pools.h"
#include "apr_optional.h"
#define SSI_VALUE_DECODED 1
#define SSI_VALUE_RAW 0
#define SSI_EXPAND_LEAVE_NAME 1
#define SSI_EXPAND_DROP_NAME 0
#define SSI_CREATE_ERROR_BUCKET(ctx, f, bb) APR_BRIGADE_INSERT_TAIL((bb), apr_bucket_pool_create(apr_pstrdup((ctx)->pool, (ctx)->error_str), strlen((ctx)->error_str), (ctx)->pool, (f)->c->bucket_alloc))
#define SSI_FLAG_PRINTING (1<<0)
#define SSI_FLAG_COND_TRUE (1<<1)
#define SSI_FLAG_SIZE_IN_BYTES (1<<2)
#define SSI_FLAG_NO_EXEC (1<<3)
#define SSI_FLAG_SIZE_ABBREV (~(SSI_FLAG_SIZE_IN_BYTES))
#define SSI_FLAG_CLEAR_PRINT_COND (~((SSI_FLAG_PRINTING) | (SSI_FLAG_COND_TRUE)))
#define SSI_FLAG_CLEAR_PRINTING (~(SSI_FLAG_PRINTING))
typedef struct {
apr_pool_t *pool;
apr_pool_t *dpool;
int flags;
int if_nesting_level;
int flush_now;
unsigned argc;
const char *error_str;
const char *time_str;
request_rec *r;
struct ssi_internal_ctx *intern;
} include_ctx_t;
typedef apr_status_t (include_handler_fn_t)(include_ctx_t *, ap_filter_t *,
apr_bucket_brigade *);
APR_DECLARE_OPTIONAL_FN(void, ap_ssi_get_tag_and_value,
(include_ctx_t *ctx, char **tag, char **tag_val,
int dodecode));
APR_DECLARE_OPTIONAL_FN(char*, ap_ssi_parse_string,
(include_ctx_t *ctx, const char *in, char *out,
apr_size_t length, int leave_name));
APR_DECLARE_OPTIONAL_FN(void, ap_register_include_handler,
(char *tag, include_handler_fn_t *func));
#endif