#include "svn_private_config.h"
#if defined(SVN_HAVE_SASL)
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_general.h>
#include <apr_strings.h>
#include <apr_atomic.h>
#include <apr_thread_mutex.h>
#include <apr_version.h>
#include "svn_types.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_ra.h"
#include "svn_ra_svn.h"
#include "svn_base64.h"
#include "private/svn_atomic.h"
#include "private/ra_svn_sasl.h"
#include "ra_svn.h"
volatile svn_atomic_t svn_ra_svn__sasl_status;
static volatile svn_atomic_t sasl_ctx_count;
static apr_pool_t *sasl_pool = NULL;
static apr_status_t sasl_done_cb(void *data) {
svn_ra_svn__sasl_status = 0;
if (svn_atomic_dec(&sasl_ctx_count) == 0)
sasl_done();
return APR_SUCCESS;
}
#if APR_HAS_THREADS
static apr_array_header_t *free_mutexes = NULL;
static apr_thread_mutex_t *array_mutex = NULL;
static void *sasl_mutex_alloc_cb(void) {
apr_thread_mutex_t *mutex;
apr_status_t apr_err;
if (!svn_ra_svn__sasl_status)
return NULL;
apr_err = apr_thread_mutex_lock(array_mutex);
if (apr_err != APR_SUCCESS)
return NULL;
if (apr_is_empty_array(free_mutexes)) {
apr_err = apr_thread_mutex_create(&mutex,
APR_THREAD_MUTEX_DEFAULT,
sasl_pool);
if (apr_err != APR_SUCCESS)
mutex = NULL;
} else
mutex = *((apr_thread_mutex_t**)apr_array_pop(free_mutexes));
apr_err = apr_thread_mutex_unlock(array_mutex);
if (apr_err != APR_SUCCESS)
return NULL;
return mutex;
}
static int sasl_mutex_lock_cb(void *mutex) {
if (!svn_ra_svn__sasl_status)
return 0;
return (apr_thread_mutex_lock(mutex) == APR_SUCCESS) ? 0 : -1;
}
static int sasl_mutex_unlock_cb(void *mutex) {
if (!svn_ra_svn__sasl_status)
return 0;
return (apr_thread_mutex_unlock(mutex) == APR_SUCCESS) ? 0 : -1;
}
static void sasl_mutex_free_cb(void *mutex) {
if (svn_ra_svn__sasl_status) {
apr_status_t apr_err = apr_thread_mutex_lock(array_mutex);
if (apr_err == APR_SUCCESS) {
APR_ARRAY_PUSH(free_mutexes, apr_thread_mutex_t*) = mutex;
apr_thread_mutex_unlock(array_mutex);
}
}
}
#endif
apr_status_t svn_ra_svn__sasl_common_init(apr_pool_t *pool) {
apr_status_t apr_err = APR_SUCCESS;
sasl_pool = svn_pool_create(pool);
sasl_ctx_count = 1;
apr_pool_cleanup_register(sasl_pool, NULL, sasl_done_cb,
apr_pool_cleanup_null);
#if APR_HAS_THREADS
sasl_set_mutex(sasl_mutex_alloc_cb,
sasl_mutex_lock_cb,
sasl_mutex_unlock_cb,
sasl_mutex_free_cb);
free_mutexes = apr_array_make(sasl_pool, 0, sizeof(apr_thread_mutex_t *));
apr_err = apr_thread_mutex_create(&array_mutex,
APR_THREAD_MUTEX_DEFAULT,
sasl_pool);
#endif
return apr_err;
}
static svn_error_t *sasl_init_cb(apr_pool_t *pool) {
if (svn_ra_svn__sasl_common_init(pool) != APR_SUCCESS
|| sasl_client_init(NULL) != SASL_OK)
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
_("Could not initialize the SASL library"));
return SVN_NO_ERROR;
}
svn_error_t *svn_ra_svn__sasl_init(void) {
SVN_ERR(svn_atomic__init_once(&svn_ra_svn__sasl_status, sasl_init_cb, NULL));
return SVN_NO_ERROR;
}
static apr_status_t sasl_dispose_cb(void *data) {
sasl_conn_t *sasl_ctx = data;
sasl_dispose(&sasl_ctx);
if (svn_atomic_dec(&sasl_ctx_count) == 0)
sasl_done();
return APR_SUCCESS;
}
void svn_ra_svn__default_secprops(sasl_security_properties_t *secprops) {
secprops->min_ssf = 0;
secprops->max_ssf = 256;
secprops->maxbufsize = SVN_RA_SVN__READBUF_SIZE;
secprops->security_flags = 0;
secprops->property_names = secprops->property_values = NULL;
}
typedef struct cred_baton {
svn_auth_baton_t *auth_baton;
svn_auth_iterstate_t *iterstate;
const char *realmstring;
const char *username;
const char *password;
svn_error_t *err;
svn_boolean_t no_more_creds;
svn_boolean_t was_used;
apr_pool_t *pool;
} cred_baton_t;
static svn_boolean_t
get_credentials(cred_baton_t *baton) {
void *creds;
if (baton->iterstate)
baton->err = svn_auth_next_credentials(&creds, baton->iterstate,
baton->pool);
else
baton->err = svn_auth_first_credentials(&creds, &baton->iterstate,
SVN_AUTH_CRED_SIMPLE,
baton->realmstring,
baton->auth_baton, baton->pool);
if (baton->err)
return FALSE;
if (! creds) {
baton->no_more_creds = TRUE;
return FALSE;
}
baton->username = ((svn_auth_cred_simple_t *)creds)->username;
baton->password = ((svn_auth_cred_simple_t *)creds)->password;
baton->was_used = TRUE;
return TRUE;
}
static int
get_username_cb(void *b, int id, const char **username, unsigned *len) {
cred_baton_t *baton = b;
if (baton->username || get_credentials(baton)) {
*username = baton->username;
if (len)
*len = strlen(baton->username);
baton->username = NULL;
return SASL_OK;
}
return SASL_FAIL;
}
static int
get_password_cb(sasl_conn_t *conn, void *b, int id, sasl_secret_t **psecret) {
cred_baton_t *baton = b;
if (baton->password || get_credentials(baton)) {
sasl_secret_t *secret;
int len = strlen(baton->password);
secret = apr_palloc(baton->pool, sizeof(*secret) + len - 1);
secret->len = len;
memcpy(secret->data, baton->password, len);
baton->password = NULL;
*psecret = secret;
return SASL_OK;
}
return SASL_FAIL;
}
static svn_error_t *new_sasl_ctx(sasl_conn_t **sasl_ctx,
svn_boolean_t is_tunneled,
const char *hostname,
const char *local_addrport,
const char *remote_addrport,
sasl_callback_t *callbacks,
apr_pool_t *pool) {
sasl_security_properties_t secprops;
int result;
result = sasl_client_new(SVN_RA_SVN_SASL_NAME,
hostname, local_addrport, remote_addrport,
callbacks, SASL_SUCCESS_DATA,
sasl_ctx);
if (result != SASL_OK)
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errstring(result, NULL, NULL));
svn_atomic_inc(&sasl_ctx_count);
apr_pool_cleanup_register(pool, *sasl_ctx, sasl_dispose_cb,
apr_pool_cleanup_null);
if (is_tunneled) {
result = sasl_setprop(*sasl_ctx,
SASL_AUTH_EXTERNAL, " ");
if (result != SASL_OK)
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errdetail(*sasl_ctx));
}
svn_ra_svn__default_secprops(&secprops);
secprops.security_flags = SASL_SEC_NOPLAINTEXT;
sasl_setprop(*sasl_ctx, SASL_SEC_PROPS, &secprops);
return SVN_NO_ERROR;
}
static svn_error_t *try_auth(svn_ra_svn__session_baton_t *sess,
sasl_conn_t *sasl_ctx,
svn_boolean_t *success,
const char **last_err,
const char *mechstring,
apr_pool_t *pool) {
sasl_interact_t *client_interact = NULL;
const char *out, *mech, *status = NULL;
const svn_string_t *arg = NULL, *in;
int result;
unsigned int outlen;
svn_boolean_t again;
do {
again = FALSE;
result = sasl_client_start(sasl_ctx,
mechstring,
&client_interact,
&out,
&outlen,
&mech);
switch (result) {
case SASL_OK:
case SASL_CONTINUE:
break;
case SASL_NOMECH:
return svn_error_create(SVN_ERR_RA_SVN_NO_MECHANISMS, NULL, NULL);
case SASL_BADPARAM:
case SASL_NOMEM:
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errdetail(sasl_ctx));
default:
{
char *dst = strstr(mechstring, mech);
char *src = dst + strlen(mech);
while ((*dst++ = *src++) != '\0')
;
again = TRUE;
}
}
} while (again);
if (outlen > 0 || strcmp(mech, "EXTERNAL") == 0)
arg = svn_base64_encode_string(svn_string_ncreate(out, outlen, pool),
pool);
SVN_ERR(svn_ra_svn__auth_response(sess->conn, pool, mech,
arg ? arg->data : NULL));
while (result == SASL_CONTINUE) {
SVN_ERR(svn_ra_svn_read_tuple(sess->conn, pool, "w(?s)",
&status, &in));
if (strcmp(status, "failure") == 0) {
*success = FALSE;
*last_err = in ? in->data : "";
return SVN_NO_ERROR;
}
if ((strcmp(status, "success") != 0 && strcmp(status, "step") != 0)
|| in == NULL)
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
_("Unexpected server response"
" to authentication"));
if (strcmp(mech, "CRAM-MD5") != 0)
in = svn_base64_decode_string(in, pool);
result = sasl_client_step(sasl_ctx,
in->data,
in->len,
&client_interact,
&out,
&outlen);
if (result != SASL_OK && result != SASL_CONTINUE)
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errdetail(sasl_ctx));
if (strcmp(status, "success") == 0)
break;
if (outlen > 0) {
arg = svn_string_ncreate(out, outlen, pool);
if (strcmp(mech, "CRAM-MD5") != 0)
arg = svn_base64_encode_string(arg, pool);
SVN_ERR(svn_ra_svn_write_cstring(sess->conn, pool, arg->data));
} else {
SVN_ERR(svn_ra_svn_write_cstring(sess->conn, pool, ""));
}
}
if (!status || strcmp(status, "step") == 0) {
SVN_ERR(svn_ra_svn_read_tuple(sess->conn, pool, "w(?s)",
&status, &in));
if (strcmp(status, "failure") == 0) {
*success = FALSE;
*last_err = in ? in->data : "";
} else if (strcmp(status, "success") == 0) {
*success = TRUE;
} else
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
_("Unexpected server response"
" to authentication"));
} else
*success = TRUE;
return SVN_NO_ERROR;
}
typedef struct sasl_baton {
svn_ra_svn__stream_t *stream;
sasl_conn_t *ctx;
unsigned int maxsize;
const char *read_buf;
unsigned int read_len;
const char *write_buf;
unsigned int write_len;
} sasl_baton_t;
static svn_error_t *sasl_read_cb(void *baton, char *buffer, apr_size_t *len) {
sasl_baton_t *sasl_baton = baton;
int result;
apr_size_t len2 = *len;
while (! sasl_baton->read_buf || sasl_baton->read_len == 0) {
SVN_ERR(svn_ra_svn__stream_read(sasl_baton->stream, buffer, &len2));
if (len2 == 0) {
*len = 0;
return SVN_NO_ERROR;
}
result = sasl_decode(sasl_baton->ctx, buffer, len2,
&sasl_baton->read_buf,
&sasl_baton->read_len);
if (result != SASL_OK)
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errdetail(sasl_baton->ctx));
}
if (*len >= sasl_baton->read_len) {
memcpy(buffer, sasl_baton->read_buf, sasl_baton->read_len);
*len = sasl_baton->read_len;
sasl_baton->read_buf = NULL;
sasl_baton->read_len = 0;
} else {
memcpy(buffer, sasl_baton->read_buf, *len);
sasl_baton->read_len -= *len;
sasl_baton->read_buf += *len;
}
return SVN_NO_ERROR;
}
static svn_error_t *
sasl_write_cb(void *baton, const char *buffer, apr_size_t *len) {
sasl_baton_t *sasl_baton = baton;
int result;
if (! sasl_baton->write_buf || sasl_baton->write_len == 0) {
*len = (*len > sasl_baton->maxsize) ? sasl_baton->maxsize : *len;
result = sasl_encode(sasl_baton->ctx, buffer, *len,
&sasl_baton->write_buf,
&sasl_baton->write_len);
if (result != SASL_OK)
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errdetail(sasl_baton->ctx));
}
do {
apr_size_t tmplen = sasl_baton->write_len;
SVN_ERR(svn_ra_svn__stream_write(sasl_baton->stream,
sasl_baton->write_buf,
&tmplen));
if (tmplen == 0) {
*len = 0;
return SVN_NO_ERROR;
}
sasl_baton->write_len -= tmplen;
sasl_baton->write_buf += tmplen;
} while (sasl_baton->write_len > 0);
sasl_baton->write_buf = NULL;
sasl_baton->write_len = 0;
return SVN_NO_ERROR;
}
static void sasl_timeout_cb(void *baton, apr_interval_time_t interval) {
sasl_baton_t *sasl_baton = baton;
svn_ra_svn__stream_timeout(sasl_baton->stream, interval);
}
static svn_boolean_t sasl_pending_cb(void *baton) {
sasl_baton_t *sasl_baton = baton;
return svn_ra_svn__stream_pending(sasl_baton->stream);
}
svn_error_t *svn_ra_svn__enable_sasl_encryption(svn_ra_svn_conn_t *conn,
sasl_conn_t *sasl_ctx,
apr_pool_t *pool) {
sasl_baton_t *sasl_baton;
const sasl_ssf_t *ssfp;
int result;
const void *maxsize;
if (! conn->encrypted) {
result = sasl_getprop(sasl_ctx, SASL_SSF, (void*) &ssfp);
if (result != SASL_OK)
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errdetail(sasl_ctx));
if (*ssfp > 0) {
SVN_ERR(svn_ra_svn_flush(conn, pool));
sasl_baton = apr_pcalloc(conn->pool, sizeof(*sasl_baton));
sasl_baton->ctx = sasl_ctx;
result = sasl_getprop(sasl_ctx, SASL_MAXOUTBUF, &maxsize);
if (result != SASL_OK)
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errdetail(sasl_ctx));
sasl_baton->maxsize = *((unsigned int *) maxsize);
if (conn->read_end > conn->read_ptr) {
result = sasl_decode(sasl_ctx, conn->read_ptr,
conn->read_end - conn->read_ptr,
&sasl_baton->read_buf,
&sasl_baton->read_len);
if (result != SASL_OK)
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
sasl_errdetail(sasl_ctx));
conn->read_end = conn->read_ptr;
}
sasl_baton->stream = conn->stream;
conn->stream = svn_ra_svn__stream_create(sasl_baton, sasl_read_cb,
sasl_write_cb,
sasl_timeout_cb,
sasl_pending_cb, conn->pool);
conn->encrypted = TRUE;
}
}
return SVN_NO_ERROR;
}
svn_error_t *svn_ra_svn__get_addresses(const char **local_addrport,
const char **remote_addrport,
svn_ra_svn_conn_t *conn,
apr_pool_t *pool) {
if (conn->sock) {
apr_status_t apr_err;
apr_sockaddr_t *local_sa, *remote_sa;
char *local_addr, *remote_addr;
apr_err = apr_socket_addr_get(&local_sa, APR_LOCAL, conn->sock);
if (apr_err)
return svn_error_wrap_apr(apr_err, NULL);
apr_err = apr_socket_addr_get(&remote_sa, APR_REMOTE, conn->sock);
if (apr_err)
return svn_error_wrap_apr(apr_err, NULL);
apr_err = apr_sockaddr_ip_get(&local_addr, local_sa);
if (apr_err)
return svn_error_wrap_apr(apr_err, NULL);
apr_err = apr_sockaddr_ip_get(&remote_addr, remote_sa);
if (apr_err)
return svn_error_wrap_apr(apr_err, NULL);
*local_addrport = apr_pstrcat(pool, local_addr, ";",
apr_itoa(pool, (int)local_sa->port), NULL);
*remote_addrport = apr_pstrcat(pool, remote_addr, ";",
apr_itoa(pool, (int)remote_sa->port), NULL);
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_ra_svn__do_cyrus_auth(svn_ra_svn__session_baton_t *sess,
apr_array_header_t *mechlist,
const char *realm, apr_pool_t *pool) {
apr_pool_t *subpool;
sasl_conn_t *sasl_ctx;
const char *mechstring = "", *last_err = "", *realmstring;
const char *local_addrport = NULL, *remote_addrport = NULL;
svn_boolean_t success;
sasl_callback_t callbacks[3];
cred_baton_t cred_baton;
int i;
if (!sess->is_tunneled) {
SVN_ERR(svn_ra_svn__get_addresses(&local_addrport, &remote_addrport,
sess->conn, pool));
}
for (i = 0; i < mechlist->nelts; i++) {
svn_ra_svn_item_t *elt = &APR_ARRAY_IDX(mechlist, i, svn_ra_svn_item_t);
if (strcmp(elt->u.word, "ANONYMOUS") == 0
|| strcmp(elt->u.word, "EXTERNAL") == 0) {
mechstring = elt->u.word;
break;
}
mechstring = apr_pstrcat(pool,
mechstring,
i == 0 ? "" : " ",
elt->u.word, NULL);
}
realmstring = apr_psprintf(pool, "%s %s", sess->realm_prefix, realm);
memset(&cred_baton, 0, sizeof(cred_baton));
cred_baton.auth_baton = sess->callbacks->auth_baton;
cred_baton.realmstring = realmstring;
cred_baton.pool = pool;
callbacks[0].id = SASL_CB_AUTHNAME;
callbacks[0].proc = get_username_cb;
callbacks[0].context = &cred_baton;
callbacks[1].id = SASL_CB_PASS;
callbacks[1].proc = get_password_cb;
callbacks[1].context = &cred_baton;
callbacks[2].id = SASL_CB_LIST_END;
callbacks[2].proc = NULL;
callbacks[2].context = NULL;
subpool = svn_pool_create(pool);
do {
svn_error_t *err;
if (*last_err)
last_err = apr_pstrdup(pool, last_err);
svn_pool_clear(subpool);
SVN_ERR(new_sasl_ctx(&sasl_ctx, sess->is_tunneled,
sess->hostname, local_addrport, remote_addrport,
callbacks, sess->conn->pool));
err = try_auth(sess, sasl_ctx, &success, &last_err, mechstring,
subpool);
if (cred_baton.err) {
svn_error_clear(err);
return cred_baton.err;
}
if (cred_baton.no_more_creds
|| (! success && ! err && ! cred_baton.was_used)) {
svn_error_clear(err);
if (*last_err)
return svn_error_createf(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
_("Authentication error from server: %s"),
last_err);
return svn_error_create(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
_("Can't get username or password"));
}
if (err) {
if (err->apr_err == SVN_ERR_RA_SVN_NO_MECHANISMS) {
svn_error_clear(err);
return svn_ra_svn__do_internal_auth(sess, mechlist,
realm, pool);
}
return err;
}
} while (!success);
svn_pool_destroy(subpool);
SVN_ERR(svn_ra_svn__enable_sasl_encryption(sess->conn, sasl_ctx, pool));
SVN_ERR(svn_auth_save_credentials(cred_baton.iterstate, pool));
return SVN_NO_ERROR;
}
#endif
