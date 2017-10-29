#include <apr_pools.h>
#include "svn_client.h"
#include "svn_error.h"
static void
call_notify_func(void *baton, const svn_wc_notify_t *n, apr_pool_t *pool) {
const svn_client_ctx_t *ctx = baton;
if (ctx->notify_func)
ctx->notify_func(ctx->notify_baton, n->path, n->action, n->kind,
n->mime_type, n->content_state, n->prop_state,
n->revision);
}
svn_error_t *
svn_client_create_context(svn_client_ctx_t **ctx,
apr_pool_t *pool) {
*ctx = apr_pcalloc(pool, sizeof(svn_client_ctx_t));
(*ctx)->notify_func2 = call_notify_func;
(*ctx)->notify_baton2 = *ctx;
return SVN_NO_ERROR;
}