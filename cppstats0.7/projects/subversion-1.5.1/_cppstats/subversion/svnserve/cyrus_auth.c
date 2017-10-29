#include "svn_private_config.h"
#if defined(SVN_HAVE_SASL)
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_general.h>
#include <apr_strings.h>
#include "svn_types.h"
#include "svn_string.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_ra_svn.h"
#include "svn_base64.h"
#include "private/svn_atomic.h"
#include "private/ra_svn_sasl.h"
#include "server.h"
static int canonicalize_username(sasl_conn_t *conn,
void *context,
const char *in,
unsigned inlen,
unsigned flags,
const char *user_realm,
char *out,
unsigned out_max, unsigned *out_len) {
int realm_len = strlen(user_realm);
char *pos;
*out_len = inlen;
pos = memchr(in, '@', inlen);
if (pos) {
if (strncmp(pos+1, user_realm, inlen-(pos-in+1)) != 0)
return SASL_BADPROT;
} else
*out_len += realm_len + 1;
if (*out_len > out_max)
return SASL_BADPROT;
strncpy(out, in, inlen);
if (!pos) {
out[inlen] = '@';
strncpy(&out[inlen+1], user_realm, realm_len);
}
return SASL_OK;
}
static sasl_callback_t callbacks[] = {
{ SASL_CB_CANON_USER, canonicalize_username, NULL },
{ SASL_CB_LIST_END, NULL, NULL }
};
static svn_error_t *initialize(apr_pool_t *pool) {
int result;
apr_status_t status;
status = svn_ra_svn__sasl_common_init(pool);
if (status)
return svn_error_wrap_apr(status,
_("Could not initialize the SASL library"));
result = sasl_server_init(callbacks, SVN_RA_SVN_SASL_NAME);
if (result != SASL_OK) {
svn_error_t *err = svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errstring(result, NULL, NULL));
return svn_error_quick_wrap(err,
_("Could not initialize the SASL library"));
}
return SVN_NO_ERROR;
}
svn_error_t *cyrus_init(apr_pool_t *pool) {
SVN_ERR(svn_atomic__init_once(&svn_ra_svn__sasl_status, initialize, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
fail_auth(svn_ra_svn_conn_t *conn, apr_pool_t *pool, sasl_conn_t *sasl_ctx) {
const char *msg = sasl_errdetail(sasl_ctx);
SVN_ERR(svn_ra_svn_write_tuple(conn, pool, "w(c)", "failure", msg));
return svn_ra_svn_flush(conn, pool);
}
static svn_error_t *
write_failure(svn_ra_svn_conn_t *conn, apr_pool_t *pool, svn_error_t **err_p) {
svn_error_t *write_err = svn_ra_svn_write_cmd_failure(conn, pool, *err_p);
svn_error_clear(*err_p);
*err_p = SVN_NO_ERROR;
return write_err;
}
static svn_error_t *
fail_cmd(svn_ra_svn_conn_t *conn, apr_pool_t *pool, sasl_conn_t *sasl_ctx) {
svn_error_t *err = svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errdetail(sasl_ctx));
SVN_ERR(write_failure(conn, pool, &err));
return svn_ra_svn_flush(conn, pool);
}
static svn_error_t *try_auth(svn_ra_svn_conn_t *conn,
sasl_conn_t *sasl_ctx,
apr_pool_t *pool,
server_baton_t *b,
svn_boolean_t *success) {
const char *out, *mech;
const svn_string_t *arg = NULL, *in;
unsigned int outlen;
int result;
svn_boolean_t use_base64;
*success = FALSE;
SVN_ERR(svn_ra_svn_read_tuple(conn, pool, "w(?s)", &mech, &in));
if (strcmp(mech, "EXTERNAL") == 0 && !in)
in = svn_string_create(b->tunnel_user, pool);
else if (in)
in = svn_base64_decode_string(in, pool);
use_base64 = (strcmp(mech, "CRAM-MD5") != 0);
result = sasl_server_start(sasl_ctx, mech,
in ? in->data : NULL,
in ? in->len : 0, &out, &outlen);
if (result != SASL_OK && result != SASL_CONTINUE)
return fail_auth(conn, pool, sasl_ctx);
while (result == SASL_CONTINUE) {
svn_ra_svn_item_t *item;
arg = svn_string_ncreate(out, outlen, pool);
if (use_base64)
arg = svn_base64_encode_string(arg, pool);
SVN_ERR(svn_ra_svn_write_tuple(conn, pool, "w(s)", "step", arg));
SVN_ERR(svn_ra_svn_read_item(conn, pool, &item));
if (item->kind != SVN_RA_SVN_STRING)
return SVN_NO_ERROR;
in = item->u.string;
if (use_base64)
in = svn_base64_decode_string(in, pool);
result = sasl_server_step(sasl_ctx, in->data, in->len, &out, &outlen);
}
if (result != SASL_OK)
return fail_auth(conn, pool, sasl_ctx);
if (outlen)
arg = svn_base64_encode_string(svn_string_ncreate(out, outlen, pool),
pool);
else
arg = NULL;
*success = TRUE;
SVN_ERR(svn_ra_svn_write_tuple(conn, pool, "w(?s)", "success", arg));
return SVN_NO_ERROR;
}
static apr_status_t sasl_dispose_cb(void *data) {
sasl_conn_t *sasl_ctx = (sasl_conn_t*) data;
sasl_dispose(&sasl_ctx);
return APR_SUCCESS;
}
svn_error_t *cyrus_auth_request(svn_ra_svn_conn_t *conn,
apr_pool_t *pool,
server_baton_t *b,
enum access_type required,
svn_boolean_t needs_username) {
sasl_conn_t *sasl_ctx;
apr_pool_t *subpool;
apr_status_t apr_err;
const char *localaddrport = NULL, *remoteaddrport = NULL;
const char *mechlist, *val;
char hostname[APRMAXHOSTLEN + 1];
sasl_security_properties_t secprops;
svn_boolean_t success, no_anonymous;
int mech_count, result = SASL_OK;
SVN_ERR(svn_ra_svn__get_addresses(&localaddrport, &remoteaddrport,
conn, pool));
apr_err = apr_gethostname(hostname, sizeof(hostname), pool);
if (apr_err) {
svn_error_t *err = svn_error_wrap_apr(apr_err, _("Can't get hostname"));
SVN_ERR(write_failure(conn, pool, &err));
return svn_ra_svn_flush(conn, pool);
}
result = sasl_server_new(SVN_RA_SVN_SASL_NAME,
hostname, b->realm,
localaddrport, remoteaddrport,
NULL, SASL_SUCCESS_DATA,
&sasl_ctx);
if (result != SASL_OK) {
svn_error_t *err = svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errstring(result, NULL, NULL));
SVN_ERR(write_failure(conn, pool, &err));
return svn_ra_svn_flush(conn, pool);
}
apr_pool_cleanup_register(b->pool, sasl_ctx, sasl_dispose_cb,
apr_pool_cleanup_null);
svn_ra_svn__default_secprops(&secprops);
secprops.security_flags = SASL_SEC_NOPLAINTEXT;
no_anonymous = needs_username || get_access(b, UNAUTHENTICATED) < required;
if (no_anonymous)
secprops.security_flags |= SASL_SEC_NOANONYMOUS;
svn_config_get(b->cfg, &val,
SVN_CONFIG_SECTION_SASL,
SVN_CONFIG_OPTION_MIN_SSF,
"0");
secprops.min_ssf = atoi(val);
svn_config_get(b->cfg, &val,
SVN_CONFIG_SECTION_SASL,
SVN_CONFIG_OPTION_MAX_SSF,
"256");
secprops.max_ssf = atoi(val);
result = sasl_setprop(sasl_ctx, SASL_SEC_PROPS, &secprops);
if (result != SASL_OK)
return fail_cmd(conn, pool, sasl_ctx);
if (b->tunnel_user)
result = sasl_setprop(sasl_ctx, SASL_AUTH_EXTERNAL, b->tunnel_user);
if (result != SASL_OK)
return fail_cmd(conn, pool, sasl_ctx);
result = sasl_listmech(sasl_ctx, NULL, NULL, " ", NULL,
&mechlist, NULL, &mech_count);
if (result != SASL_OK)
return fail_cmd(conn, pool, sasl_ctx);
if (mech_count == 0) {
svn_error_t *err = svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
_("Could not obtain the list"
" of SASL mechanisms"));
SVN_ERR(write_failure(conn, pool, &err));
return svn_ra_svn_flush(conn, pool);
}
SVN_ERR(svn_ra_svn_write_cmd_response(conn, pool, "(w)c",
mechlist, b->realm));
subpool = svn_pool_create(pool);
do {
svn_pool_clear(subpool);
SVN_ERR(try_auth(conn, sasl_ctx, subpool, b, &success));
} while (!success);
svn_pool_destroy(subpool);
SVN_ERR(svn_ra_svn__enable_sasl_encryption(conn, sasl_ctx, pool));
if (no_anonymous) {
char *p;
const void *user;
result = sasl_getprop(sasl_ctx, SASL_USERNAME, &user);
if (result != SASL_OK)
return fail_cmd(conn, pool, sasl_ctx);
if ((p = strchr(user, '@')) != NULL)
b->user = apr_pstrndup(b->pool, user, p - (char *)user);
else {
svn_error_t *err;
err = svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
_("Couldn't obtain the authenticated"
" username"));
SVN_ERR(write_failure(conn, pool, &err));
return svn_ra_svn_flush(conn, pool);
}
}
return SVN_NO_ERROR;
}
#endif