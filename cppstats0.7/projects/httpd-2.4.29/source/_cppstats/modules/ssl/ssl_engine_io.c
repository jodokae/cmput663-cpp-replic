#include "ssl_private.h"
#include "mod_ssl.h"
#include "mod_ssl_openssl.h"
#include "apr_date.h"
APR_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(ssl, SSL, int, proxy_post_handshake,
(conn_rec *c,SSL *ssl),
(c,ssl),OK,DECLINED);
typedef struct {
SSL *pssl;
BIO *pbioRead;
BIO *pbioWrite;
ap_filter_t *pInputFilter;
ap_filter_t *pOutputFilter;
SSLConnRec *config;
} ssl_filter_ctx_t;
typedef struct {
ssl_filter_ctx_t *filter_ctx;
conn_rec *c;
apr_bucket_brigade *bb;
apr_status_t rc;
} bio_filter_out_ctx_t;
static bio_filter_out_ctx_t *bio_filter_out_ctx_new(ssl_filter_ctx_t *filter_ctx,
conn_rec *c) {
bio_filter_out_ctx_t *outctx = apr_palloc(c->pool, sizeof(*outctx));
outctx->filter_ctx = filter_ctx;
outctx->c = c;
outctx->bb = apr_brigade_create(c->pool, c->bucket_alloc);
return outctx;
}
static int bio_filter_out_pass(bio_filter_out_ctx_t *outctx) {
AP_DEBUG_ASSERT(!APR_BRIGADE_EMPTY(outctx->bb));
outctx->rc = ap_pass_brigade(outctx->filter_ctx->pOutputFilter->next,
outctx->bb);
if (outctx->rc == APR_SUCCESS && outctx->c->aborted) {
outctx->rc = APR_ECONNRESET;
}
return (outctx->rc == APR_SUCCESS) ? 1 : -1;
}
static int bio_filter_out_flush(BIO *bio) {
bio_filter_out_ctx_t *outctx = (bio_filter_out_ctx_t *)BIO_get_data(bio);
apr_bucket *e;
AP_DEBUG_ASSERT(APR_BRIGADE_EMPTY(outctx->bb));
e = apr_bucket_flush_create(outctx->bb->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(outctx->bb, e);
return bio_filter_out_pass(outctx);
}
static int bio_filter_create(BIO *bio) {
BIO_set_shutdown(bio, 1);
BIO_set_init(bio, 1);
#if MODSSL_USE_OPENSSL_PRE_1_1_API
bio->num = -1;
#endif
BIO_set_data(bio, NULL);
return 1;
}
static int bio_filter_destroy(BIO *bio) {
if (bio == NULL) {
return 0;
}
return 1;
}
static int bio_filter_out_read(BIO *bio, char *out, int outl) {
return -1;
}
static int bio_filter_out_write(BIO *bio, const char *in, int inl) {
bio_filter_out_ctx_t *outctx = (bio_filter_out_ctx_t *)BIO_get_data(bio);
apr_bucket *e;
int need_flush;
if (outctx->filter_ctx->config->reneg_state == RENEG_ABORT) {
outctx->rc = APR_ECONNABORTED;
return -1;
}
BIO_clear_retry_flags(bio);
e = apr_bucket_transient_create(in, inl, outctx->bb->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(outctx->bb, e);
#if OPENSSL_VERSION_NUMBER < 0x0009080df
need_flush = !SSL_is_init_finished(outctx->filter_ctx->pssl);
#else
need_flush = SSL_in_connect_init(outctx->filter_ctx->pssl);
#endif
if (need_flush) {
e = apr_bucket_flush_create(outctx->bb->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(outctx->bb, e);
}
if (bio_filter_out_pass(outctx) < 0) {
return -1;
}
return inl;
}
static long bio_filter_out_ctrl(BIO *bio, int cmd, long num, void *ptr) {
long ret = 1;
bio_filter_out_ctx_t *outctx = (bio_filter_out_ctx_t *)BIO_get_data(bio);
switch (cmd) {
case BIO_CTRL_RESET:
case BIO_CTRL_EOF:
case BIO_C_SET_BUF_MEM_EOF_RETURN:
ap_log_cerror(APLOG_MARK, APLOG_TRACE4, 0, outctx->c,
"output bio: unhandled control %d", cmd);
ret = 0;
break;
case BIO_CTRL_WPENDING:
case BIO_CTRL_PENDING:
case BIO_CTRL_INFO:
ret = 0;
break;
case BIO_CTRL_GET_CLOSE:
ret = (long)BIO_get_shutdown(bio);
break;
case BIO_CTRL_SET_CLOSE:
BIO_set_shutdown(bio, (int)num);
break;
case BIO_CTRL_FLUSH:
ret = bio_filter_out_flush(bio);
break;
case BIO_CTRL_DUP:
ret = 1;
break;
case BIO_C_SET_BUF_MEM:
case BIO_C_GET_BUF_MEM_PTR:
case BIO_CTRL_PUSH:
case BIO_CTRL_POP:
default:
ret = 0;
break;
}
return ret;
}
static int bio_filter_out_gets(BIO *bio, char *buf, int size) {
return -1;
}
static int bio_filter_out_puts(BIO *bio, const char *str) {
return -1;
}
typedef struct {
int length;
char *value;
} char_buffer_t;
typedef struct {
SSL *ssl;
BIO *bio_out;
ap_filter_t *f;
apr_status_t rc;
ap_input_mode_t mode;
apr_read_type_e block;
apr_bucket_brigade *bb;
char_buffer_t cbuf;
apr_pool_t *pool;
char buffer[AP_IOBUFSIZE];
ssl_filter_ctx_t *filter_ctx;
} bio_filter_in_ctx_t;
static int char_buffer_read(char_buffer_t *buffer, char *in, int inl) {
if (!buffer->length) {
return 0;
}
if (buffer->length > inl) {
memmove(in, buffer->value, inl);
buffer->value += inl;
buffer->length -= inl;
} else {
memmove(in, buffer->value, buffer->length);
inl = buffer->length;
buffer->value = NULL;
buffer->length = 0;
}
return inl;
}
static int char_buffer_write(char_buffer_t *buffer, char *in, int inl) {
buffer->value = in;
buffer->length = inl;
return inl;
}
static apr_status_t brigade_consume(apr_bucket_brigade *bb,
apr_read_type_e block,
char *c, apr_size_t *len) {
apr_size_t actual = 0;
apr_status_t status = APR_SUCCESS;
while (!APR_BRIGADE_EMPTY(bb)) {
apr_bucket *b = APR_BRIGADE_FIRST(bb);
const char *str;
apr_size_t str_len;
apr_size_t consume;
if (APR_BUCKET_IS_EOS(b)) {
status = APR_EOF;
break;
}
status = apr_bucket_read(b, &str, &str_len, block);
if (status != APR_SUCCESS) {
if (APR_STATUS_IS_EOF(status)) {
apr_bucket_delete(b);
continue;
}
break;
}
if (str_len > 0) {
block = APR_NONBLOCK_READ;
consume = (str_len + actual > *len) ? *len - actual : str_len;
memcpy(c, str, consume);
c += consume;
actual += consume;
if (consume >= b->length) {
apr_bucket_delete(b);
} else {
b->start += consume;
b->length -= consume;
}
} else if (b->length == 0) {
apr_bucket_delete(b);
}
if (actual >= *len) {
break;
}
}
*len = actual;
return status;
}
static int bio_filter_in_read(BIO *bio, char *in, int inlen) {
apr_size_t inl = inlen;
bio_filter_in_ctx_t *inctx = (bio_filter_in_ctx_t *)BIO_get_data(bio);
apr_read_type_e block = inctx->block;
inctx->rc = APR_SUCCESS;
if (!in)
return 0;
if (inctx->filter_ctx->config->reneg_state == RENEG_ABORT) {
inctx->rc = APR_ECONNABORTED;
return -1;
}
BIO_clear_retry_flags(bio);
if (!inctx->bb) {
inctx->rc = APR_EOF;
return -1;
}
if (APR_BRIGADE_EMPTY(inctx->bb)) {
inctx->rc = ap_get_brigade(inctx->f->next, inctx->bb,
AP_MODE_READBYTES, block,
inl);
if (APR_STATUS_IS_EAGAIN(inctx->rc) || APR_STATUS_IS_EINTR(inctx->rc)
|| (inctx->rc == APR_SUCCESS && APR_BRIGADE_EMPTY(inctx->bb))) {
BIO_set_retry_read(bio);
return -1;
}
if (block == APR_BLOCK_READ
&& APR_STATUS_IS_TIMEUP(inctx->rc)
&& APR_BRIGADE_EMPTY(inctx->bb)) {
return -1;
}
if (inctx->rc != APR_SUCCESS) {
apr_brigade_cleanup(inctx->bb);
inctx->bb = NULL;
return -1;
}
}
inctx->rc = brigade_consume(inctx->bb, block, in, &inl);
if (inctx->rc == APR_SUCCESS) {
return (int)inl;
}
if (APR_STATUS_IS_EAGAIN(inctx->rc)
|| APR_STATUS_IS_EINTR(inctx->rc)) {
BIO_set_retry_read(bio);
return (int)inl;
}
apr_brigade_cleanup(inctx->bb);
inctx->bb = NULL;
if (APR_STATUS_IS_EOF(inctx->rc) && inl) {
return (int)inl;
}
return -1;
}
static int bio_filter_in_write(BIO *bio, const char *in, int inl) {
return -1;
}
static int bio_filter_in_puts(BIO *bio, const char *str) {
return -1;
}
static int bio_filter_in_gets(BIO *bio, char *buf, int size) {
return -1;
}
static long bio_filter_in_ctrl(BIO *bio, int cmd, long num, void *ptr) {
return -1;
}
#if MODSSL_USE_OPENSSL_PRE_1_1_API
static BIO_METHOD bio_filter_out_method = {
BIO_TYPE_MEM,
"APR output filter",
bio_filter_out_write,
bio_filter_out_read,
bio_filter_out_puts,
bio_filter_out_gets,
bio_filter_out_ctrl,
bio_filter_create,
bio_filter_destroy,
NULL
};
static BIO_METHOD bio_filter_in_method = {
BIO_TYPE_MEM,
"APR input filter",
bio_filter_in_write,
bio_filter_in_read,
bio_filter_in_puts,
bio_filter_in_gets,
bio_filter_in_ctrl,
bio_filter_create,
bio_filter_destroy,
NULL
};
#else
static BIO_METHOD *bio_filter_out_method = NULL;
static BIO_METHOD *bio_filter_in_method = NULL;
void init_bio_methods(void) {
bio_filter_out_method = BIO_meth_new(BIO_TYPE_MEM, "APR output filter");
BIO_meth_set_write(bio_filter_out_method, &bio_filter_out_write);
BIO_meth_set_read(bio_filter_out_method, &bio_filter_out_read);
BIO_meth_set_puts(bio_filter_out_method, &bio_filter_out_puts);
BIO_meth_set_gets(bio_filter_out_method, &bio_filter_out_gets);
BIO_meth_set_ctrl(bio_filter_out_method, &bio_filter_out_ctrl);
BIO_meth_set_create(bio_filter_out_method, &bio_filter_create);
BIO_meth_set_destroy(bio_filter_out_method, &bio_filter_destroy);
bio_filter_in_method = BIO_meth_new(BIO_TYPE_MEM, "APR input filter");
BIO_meth_set_write(bio_filter_in_method, &bio_filter_in_write);
BIO_meth_set_read(bio_filter_in_method, &bio_filter_in_read);
BIO_meth_set_puts(bio_filter_in_method, &bio_filter_in_puts);
BIO_meth_set_gets(bio_filter_in_method, &bio_filter_in_gets);
BIO_meth_set_ctrl(bio_filter_in_method, &bio_filter_in_ctrl);
BIO_meth_set_create(bio_filter_in_method, &bio_filter_create);
BIO_meth_set_destroy(bio_filter_in_method, &bio_filter_destroy);
}
void free_bio_methods(void) {
BIO_meth_free(bio_filter_out_method);
BIO_meth_free(bio_filter_in_method);
}
#endif
static apr_status_t ssl_io_input_read(bio_filter_in_ctx_t *inctx,
char *buf,
apr_size_t *len) {
apr_size_t wanted = *len;
apr_size_t bytes = 0;
int rc;
*len = 0;
if ((bytes = char_buffer_read(&inctx->cbuf, buf, wanted))) {
*len = bytes;
if (inctx->mode == AP_MODE_SPECULATIVE) {
if (inctx->cbuf.length > 0) {
inctx->cbuf.value -= bytes;
inctx->cbuf.length += bytes;
} else {
char_buffer_write(&inctx->cbuf, buf, (int)bytes);
}
return APR_SUCCESS;
}
if (*len >= wanted) {
return APR_SUCCESS;
}
if (inctx->mode == AP_MODE_GETLINE) {
if (memchr(buf, APR_ASCII_LF, *len)) {
return APR_SUCCESS;
}
} else {
inctx->block = APR_NONBLOCK_READ;
}
}
while (1) {
if (!inctx->filter_ctx->pssl) {
if (inctx->rc == APR_SUCCESS) {
inctx->rc = APR_EGENERAL;
}
break;
}
ERR_clear_error();
rc = SSL_read(inctx->filter_ctx->pssl, buf + bytes, wanted - bytes);
if (rc > 0) {
*len += rc;
if (inctx->mode == AP_MODE_SPECULATIVE) {
char_buffer_write(&inctx->cbuf, buf, rc);
}
return inctx->rc;
} else if (rc == 0) {
if (APR_STATUS_IS_EAGAIN(inctx->rc)
|| APR_STATUS_IS_EINTR(inctx->rc)) {
if (*len > 0) {
inctx->rc = APR_SUCCESS;
break;
}
if (inctx->block == APR_NONBLOCK_READ) {
break;
}
} else {
if (*len > 0) {
inctx->rc = APR_SUCCESS;
} else {
inctx->rc = APR_EOF;
}
break;
}
} else {
int ssl_err = SSL_get_error(inctx->filter_ctx->pssl, rc);
conn_rec *c = (conn_rec*)SSL_get_app_data(inctx->filter_ctx->pssl);
if (ssl_err == SSL_ERROR_WANT_READ) {
inctx->rc = APR_EAGAIN;
if (*len > 0) {
inctx->rc = APR_SUCCESS;
break;
}
if (inctx->block == APR_NONBLOCK_READ) {
break;
}
continue;
} else if (ssl_err == SSL_ERROR_SYSCALL) {
if (APR_STATUS_IS_EAGAIN(inctx->rc)
|| APR_STATUS_IS_EINTR(inctx->rc)) {
if (*len > 0) {
inctx->rc = APR_SUCCESS;
break;
}
if (inctx->block == APR_NONBLOCK_READ) {
break;
}
continue;
} else if (APR_STATUS_IS_TIMEUP(inctx->rc)) {
} else {
ap_log_cerror(APLOG_MARK, APLOG_INFO, inctx->rc, c, APLOGNO(01991)
"SSL input filter read failed.");
}
} else {
ap_log_cerror(APLOG_MARK, APLOG_INFO, inctx->rc, c, APLOGNO(01992)
"SSL library error %d reading data", ssl_err);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_INFO, mySrvFromConn(c));
}
if (inctx->rc == APR_SUCCESS) {
inctx->rc = APR_EGENERAL;
}
break;
}
}
return inctx->rc;
}
static apr_status_t ssl_io_input_getline(bio_filter_in_ctx_t *inctx,
char *buf,
apr_size_t *len) {
const char *pos = NULL;
apr_status_t status;
apr_size_t tmplen = *len, buflen = *len, offset = 0;
*len = 0;
while (tmplen > 0) {
status = ssl_io_input_read(inctx, buf + offset, &tmplen);
if (status != APR_SUCCESS) {
if (APR_STATUS_IS_EAGAIN(status) && (*len > 0)) {
char_buffer_write(&inctx->cbuf, buf, *len);
}
return status;
}
*len += tmplen;
if ((pos = memchr(buf, APR_ASCII_LF, *len))) {
break;
}
offset += tmplen;
tmplen = buflen - offset;
}
if (pos) {
char *value;
int length;
apr_size_t bytes = pos - buf;
bytes += 1;
value = buf + bytes;
length = *len - bytes;
char_buffer_write(&inctx->cbuf, value, length);
*len = bytes;
}
return APR_SUCCESS;
}
static apr_status_t ssl_filter_write(ap_filter_t *f,
const char *data,
apr_size_t len) {
ssl_filter_ctx_t *filter_ctx = f->ctx;
bio_filter_out_ctx_t *outctx;
int res;
if (filter_ctx->pssl == NULL) {
return APR_EGENERAL;
}
ERR_clear_error();
outctx = (bio_filter_out_ctx_t *)BIO_get_data(filter_ctx->pbioWrite);
res = SSL_write(filter_ctx->pssl, (unsigned char *)data, len);
if (res < 0) {
int ssl_err = SSL_get_error(filter_ctx->pssl, res);
conn_rec *c = (conn_rec*)SSL_get_app_data(outctx->filter_ctx->pssl);
if (ssl_err == SSL_ERROR_WANT_WRITE) {
outctx->rc = APR_EAGAIN;
} else if (ssl_err == SSL_ERROR_WANT_READ) {
outctx->c->cs->sense = CONN_SENSE_WANT_READ;
outctx->rc = APR_EAGAIN;
} else if (ssl_err == SSL_ERROR_SYSCALL) {
ap_log_cerror(APLOG_MARK, APLOG_INFO, outctx->rc, c, APLOGNO(01993)
"SSL output filter write failed.");
} else {
ap_log_cerror(APLOG_MARK, APLOG_INFO, outctx->rc, c, APLOGNO(01994)
"SSL library error %d writing data", ssl_err);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_INFO, mySrvFromConn(c));
}
if (outctx->rc == APR_SUCCESS) {
outctx->rc = APR_EGENERAL;
}
} else if ((apr_size_t)res != len) {
conn_rec *c = f->c;
char *reason = "reason unknown";
if (SSL_total_renegotiations(filter_ctx->pssl)) {
reason = "likely due to failed renegotiation";
}
ap_log_cerror(APLOG_MARK, APLOG_INFO, outctx->rc, c, APLOGNO(01995)
"failed to write %" APR_SSIZE_T_FMT
" of %" APR_SIZE_T_FMT " bytes (%s)",
len - (apr_size_t)res, len, reason);
outctx->rc = APR_EGENERAL;
}
return outctx->rc;
}
#define HTTP_ON_HTTPS_PORT "GET / HTTP/1.0" CRLF
#define HTTP_ON_HTTPS_PORT_BUCKET(alloc) apr_bucket_immortal_create(HTTP_ON_HTTPS_PORT, sizeof(HTTP_ON_HTTPS_PORT) - 1, alloc)
#define MODSSL_ERROR_HTTP_ON_HTTPS (APR_OS_START_USERERR + 0)
#define MODSSL_ERROR_BAD_GATEWAY (APR_OS_START_USERERR + 1)
static void ssl_io_filter_disable(SSLConnRec *sslconn,
bio_filter_in_ctx_t *inctx) {
SSL_free(inctx->ssl);
sslconn->ssl = NULL;
inctx->ssl = NULL;
inctx->filter_ctx->pssl = NULL;
}
static apr_status_t ssl_io_filter_error(bio_filter_in_ctx_t *inctx,
apr_bucket_brigade *bb,
apr_status_t status,
int is_init) {
ap_filter_t *f = inctx->f;
SSLConnRec *sslconn = myConnConfig(f->c);
apr_bucket *bucket;
int send_eos = 1;
switch (status) {
case MODSSL_ERROR_HTTP_ON_HTTPS:
ap_log_cerror(APLOG_MARK, APLOG_INFO, 0, f->c, APLOGNO(01996)
"SSL handshake failed: HTTP spoken on HTTPS port; "
"trying to send HTML error page");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_INFO, sslconn->server);
ssl_io_filter_disable(sslconn, inctx);
f->c->keepalive = AP_CONN_CLOSE;
if (is_init) {
sslconn->non_ssl_request = NON_SSL_SEND_REQLINE;
return APR_EGENERAL;
}
sslconn->non_ssl_request = NON_SSL_SEND_HDR_SEP;
bucket = HTTP_ON_HTTPS_PORT_BUCKET(f->c->bucket_alloc);
send_eos = 0;
break;
case MODSSL_ERROR_BAD_GATEWAY:
bucket = ap_bucket_error_create(HTTP_BAD_REQUEST, NULL,
f->c->pool,
f->c->bucket_alloc);
ap_log_cerror(APLOG_MARK, APLOG_INFO, 0, f->c, APLOGNO(01997)
"SSL handshake failed: sending 502");
break;
default:
return status;
}
APR_BRIGADE_INSERT_TAIL(bb, bucket);
if (send_eos) {
bucket = apr_bucket_eos_create(f->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bucket);
}
return APR_SUCCESS;
}
static const char ssl_io_filter[] = "SSL/TLS Filter";
static const char ssl_io_buffer[] = "SSL/TLS Buffer";
static const char ssl_io_coalesce[] = "SSL/TLS Coalescing Filter";
static void ssl_filter_io_shutdown(ssl_filter_ctx_t *filter_ctx,
conn_rec *c, int abortive) {
SSL *ssl = filter_ctx->pssl;
const char *type = "";
SSLConnRec *sslconn = myConnConfig(c);
int shutdown_type;
int loglevel = APLOG_DEBUG;
const char *logno;
if (!ssl) {
return;
}
if (abortive) {
shutdown_type = SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN;
type = "abortive";
logno = APLOGNO(01998);
loglevel = APLOG_INFO;
} else switch (sslconn->shutdown_type) {
case SSL_SHUTDOWN_TYPE_UNCLEAN:
shutdown_type = SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN;
type = "unclean";
logno = APLOGNO(01999);
break;
case SSL_SHUTDOWN_TYPE_ACCURATE:
shutdown_type = 0;
type = "accurate";
logno = APLOGNO(02000);
break;
default:
shutdown_type = SSL_RECEIVED_SHUTDOWN;
type = "standard";
logno = APLOGNO(02001);
break;
}
SSL_set_shutdown(ssl, shutdown_type);
modssl_smart_shutdown(ssl);
if (APLOG_CS_IS_LEVEL(c, mySrvFromConn(c), loglevel)) {
ap_log_cserror(APLOG_MARK, loglevel, 0, c, mySrvFromConn(c),
"%sConnection closed to child %ld with %s shutdown "
"(server %s)",
logno, c->id, type,
ssl_util_vhostid(c->pool, mySrvFromConn(c)));
}
if (sslconn->client_cert) {
X509_free(sslconn->client_cert);
sslconn->client_cert = NULL;
}
SSL_free(ssl);
sslconn->ssl = NULL;
filter_ctx->pssl = NULL;
if (abortive) {
c->aborted = 1;
}
}
static apr_status_t ssl_io_filter_cleanup(void *data) {
ssl_filter_ctx_t *filter_ctx = data;
if (filter_ctx->pssl) {
conn_rec *c = (conn_rec *)SSL_get_app_data(filter_ctx->pssl);
SSLConnRec *sslconn = myConnConfig(c);
SSL_free(filter_ctx->pssl);
sslconn->ssl = filter_ctx->pssl = NULL;
}
return APR_SUCCESS;
}
static apr_status_t ssl_io_filter_handshake(ssl_filter_ctx_t *filter_ctx) {
conn_rec *c = (conn_rec *)SSL_get_app_data(filter_ctx->pssl);
SSLConnRec *sslconn = myConnConfig(c);
SSLSrvConfigRec *sc;
X509 *cert;
int n;
int ssl_err;
long verify_result;
server_rec *server;
if (SSL_is_init_finished(filter_ctx->pssl)) {
return APR_SUCCESS;
}
server = sslconn->server;
if (sslconn->is_proxy) {
#if defined(HAVE_TLSEXT)
apr_ipsubnet_t *ip;
#if defined(HAVE_TLS_ALPN)
const char *alpn_note;
#endif
#endif
const char *hostname_note = apr_table_get(c->notes,
"proxy-request-hostname");
BOOL proxy_ssl_check_peer_ok = TRUE;
int post_handshake_rc = OK;
sc = mySrvConfig(server);
#if defined(HAVE_TLSEXT)
#if defined(HAVE_TLS_ALPN)
alpn_note = apr_table_get(c->notes, "proxy-request-alpn-protos");
if (alpn_note) {
char *protos, *s, *p, *last;
apr_size_t len;
s = protos = apr_pcalloc(c->pool, strlen(alpn_note)+1);
p = apr_pstrdup(c->pool, alpn_note);
while ((p = apr_strtok(p, ", ", &last))) {
len = last - p - (*last? 1 : 0);
if (len > 255) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, c, APLOGNO(03309)
"ALPN proxy protocol identifier too long: %s",
p);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, server);
return APR_EGENERAL;
}
*s++ = (unsigned char)len;
while (len--) {
*s++ = *p++;
}
p = NULL;
}
ap_log_cerror(APLOG_MARK, APLOG_TRACE1, 0, c,
"setting alpn protos from '%s', protolen=%d",
alpn_note, (int)(s - protos));
if (protos != s && SSL_set_alpn_protos(filter_ctx->pssl,
(unsigned char *)protos,
s - protos)) {
ap_log_cerror(APLOG_MARK, APLOG_WARNING, 0, c, APLOGNO(03310)
"error setting alpn protos from '%s'", alpn_note);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_WARNING, server);
}
}
#endif
if (hostname_note &&
#if !defined(OPENSSL_NO_SSL3)
sc->proxy->protocol != SSL_PROTOCOL_SSLV3 &&
#endif
apr_ipsubnet_create(&ip, hostname_note, NULL,
c->pool) != APR_SUCCESS) {
if (SSL_set_tlsext_host_name(filter_ctx->pssl, hostname_note)) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE3, 0, c,
"SNI extension for SSL Proxy request set to '%s'",
hostname_note);
} else {
ap_log_cerror(APLOG_MARK, APLOG_WARNING, 0, c, APLOGNO(02002)
"Failed to set SNI extension for SSL Proxy "
"request to '%s'", hostname_note);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_WARNING, server);
}
}
#endif
if ((n = SSL_connect(filter_ctx->pssl)) <= 0) {
ap_log_cerror(APLOG_MARK, APLOG_INFO, 0, c, APLOGNO(02003)
"SSL Proxy connect failed");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_INFO, server);
ssl_filter_io_shutdown(filter_ctx, c, 1);
apr_table_setn(c->notes, "SSL_connect_rv", "err");
return MODSSL_ERROR_BAD_GATEWAY;
}
cert = SSL_get_peer_certificate(filter_ctx->pssl);
if (sc->proxy_ssl_check_peer_expire != SSL_ENABLED_FALSE) {
if (!cert
|| (X509_cmp_current_time(
X509_get_notBefore(cert)) >= 0)
|| (X509_cmp_current_time(
X509_get_notAfter(cert)) <= 0)) {
proxy_ssl_check_peer_ok = FALSE;
ap_log_cerror(APLOG_MARK, APLOG_INFO, 0, c, APLOGNO(02004)
"SSL Proxy: Peer certificate is expired");
}
}
if ((sc->proxy_ssl_check_peer_name != SSL_ENABLED_FALSE) &&
((sc->proxy_ssl_check_peer_cn != SSL_ENABLED_FALSE) ||
(sc->proxy_ssl_check_peer_name == SSL_ENABLED_TRUE)) &&
hostname_note) {
apr_table_unset(c->notes, "proxy-request-hostname");
if (!cert
|| modssl_X509_match_name(c->pool, cert, hostname_note,
TRUE, server) == FALSE) {
proxy_ssl_check_peer_ok = FALSE;
ap_log_cerror(APLOG_MARK, APLOG_INFO, 0, c, APLOGNO(02411)
"SSL Proxy: Peer certificate does not match "
"for hostname %s", hostname_note);
}
} else if ((sc->proxy_ssl_check_peer_cn == SSL_ENABLED_TRUE) &&
hostname_note) {
const char *hostname;
int match = 0;
hostname = ssl_var_lookup(NULL, server, c, NULL,
"SSL_CLIENT_S_DN_CN");
apr_table_unset(c->notes, "proxy-request-hostname");
match = strcasecmp(hostname, hostname_note) == 0;
if (!match && strncmp(hostname, "*.", 2) == 0) {
const char *p = ap_strchr_c(hostname_note, '.');
match = p && strcasecmp(p, hostname + 1) == 0;
}
if (!match) {
proxy_ssl_check_peer_ok = FALSE;
ap_log_cerror(APLOG_MARK, APLOG_INFO, 0, c, APLOGNO(02005)
"SSL Proxy: Peer certificate CN mismatch:"
" Certificate CN: %s Requested hostname: %s",
hostname, hostname_note);
}
}
if (proxy_ssl_check_peer_ok == TRUE) {
post_handshake_rc = ssl_run_proxy_post_handshake(c, filter_ctx->pssl);
}
if (cert) {
X509_free(cert);
}
if (proxy_ssl_check_peer_ok != TRUE
|| (post_handshake_rc != OK && post_handshake_rc != DECLINED)) {
ssl_filter_io_shutdown(filter_ctx, c, 1);
apr_table_setn(c->notes, "SSL_connect_rv", "err");
return HTTP_BAD_GATEWAY;
}
apr_table_setn(c->notes, "SSL_connect_rv", "ok");
return APR_SUCCESS;
}
ERR_clear_error();
if ((n = SSL_accept(filter_ctx->pssl)) <= 0) {
bio_filter_in_ctx_t *inctx = (bio_filter_in_ctx_t *)
BIO_get_data(filter_ctx->pbioRead);
bio_filter_out_ctx_t *outctx = (bio_filter_out_ctx_t *)
BIO_get_data(filter_ctx->pbioWrite);
apr_status_t rc = inctx->rc ? inctx->rc : outctx->rc ;
ssl_err = SSL_get_error(filter_ctx->pssl, n);
if (ssl_err == SSL_ERROR_ZERO_RETURN) {
ap_log_cerror(APLOG_MARK, APLOG_INFO, rc, c, APLOGNO(02006)
"SSL handshake stopped: connection was closed");
} else if (ssl_err == SSL_ERROR_WANT_READ) {
outctx->rc = APR_EAGAIN;
return APR_EAGAIN;
} else if (ERR_GET_LIB(ERR_peek_error()) == ERR_LIB_SSL &&
ERR_GET_REASON(ERR_peek_error()) == SSL_R_HTTP_REQUEST) {
return MODSSL_ERROR_HTTP_ON_HTTPS;
} else if (ssl_err == SSL_ERROR_SYSCALL) {
ap_log_cerror(APLOG_MARK, APLOG_DEBUG, rc, c, APLOGNO(02007)
"SSL handshake interrupted by system "
"[Hint: Stop button pressed in browser?!]");
} else {
ap_log_cerror(APLOG_MARK, APLOG_INFO, rc, c, APLOGNO(02008)
"SSL library error %d in handshake "
"(server %s)", ssl_err,
ssl_util_vhostid(c->pool, server));
ssl_log_ssl_error(SSLLOG_MARK, APLOG_INFO, server);
}
if (inctx->rc == APR_SUCCESS) {
inctx->rc = APR_EGENERAL;
}
ssl_filter_io_shutdown(filter_ctx, c, 1);
return inctx->rc;
}
sc = mySrvConfig(sslconn->server);
verify_result = SSL_get_verify_result(filter_ctx->pssl);
if ((verify_result != X509_V_OK) ||
sslconn->verify_error) {
if (ssl_verify_error_is_optional(verify_result) &&
(sc->server->auth.verify_mode == SSL_CVERIFY_OPTIONAL_NO_CA)) {
ap_log_cerror(APLOG_MARK, APLOG_INFO, 0, c, APLOGNO(02009)
"SSL client authentication failed, "
"accepting certificate based on "
"\"SSLVerifyClient optional_no_ca\" "
"configuration");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_INFO, server);
} else {
const char *error = sslconn->verify_error ?
sslconn->verify_error :
X509_verify_cert_error_string(verify_result);
ap_log_cerror(APLOG_MARK, APLOG_INFO, 0, c, APLOGNO(02010)
"SSL client authentication failed: %s",
error ? error : "unknown");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_INFO, server);
ssl_filter_io_shutdown(filter_ctx, c, 1);
return APR_ECONNABORTED;
}
}
if ((cert = SSL_get_peer_certificate(filter_ctx->pssl))) {
if (sslconn->client_cert) {
X509_free(sslconn->client_cert);
}
sslconn->client_cert = cert;
sslconn->client_dn = NULL;
}
if ((sc->server->auth.verify_mode == SSL_CVERIFY_REQUIRE) &&
!sslconn->client_cert) {
ap_log_cerror(APLOG_MARK, APLOG_INFO, 0, c, APLOGNO(02011)
"No acceptable peer certificate available");
ssl_filter_io_shutdown(filter_ctx, c, 1);
return APR_ECONNABORTED;
}
return APR_SUCCESS;
}
static apr_status_t ssl_io_filter_input(ap_filter_t *f,
apr_bucket_brigade *bb,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t readbytes) {
apr_status_t status;
bio_filter_in_ctx_t *inctx = f->ctx;
const char *start = inctx->buffer;
apr_size_t len = sizeof(inctx->buffer);
int is_init = (mode == AP_MODE_INIT);
apr_bucket *bucket;
if (f->c->aborted) {
bucket = apr_bucket_eos_create(f->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bucket);
return APR_ECONNABORTED;
}
if (!inctx->ssl) {
SSLConnRec *sslconn = myConnConfig(f->c);
if (sslconn->non_ssl_request == NON_SSL_SEND_REQLINE) {
bucket = HTTP_ON_HTTPS_PORT_BUCKET(f->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bucket);
if (mode != AP_MODE_SPECULATIVE) {
sslconn->non_ssl_request = NON_SSL_SEND_HDR_SEP;
}
return APR_SUCCESS;
}
if (sslconn->non_ssl_request == NON_SSL_SEND_HDR_SEP) {
bucket = apr_bucket_immortal_create(CRLF, 2, f->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bucket);
if (mode != AP_MODE_SPECULATIVE) {
sslconn->non_ssl_request = NON_SSL_SET_ERROR_MSG;
}
return APR_SUCCESS;
}
return ap_get_brigade(f->next, bb, mode, block, readbytes);
}
if (mode != AP_MODE_READBYTES && mode != AP_MODE_GETLINE &&
mode != AP_MODE_SPECULATIVE && mode != AP_MODE_INIT) {
return APR_ENOTIMPL;
}
inctx->mode = mode;
inctx->block = block;
if ((status = ssl_io_filter_handshake(inctx->filter_ctx)) != APR_SUCCESS) {
return ssl_io_filter_error(inctx, bb, status, is_init);
}
if (is_init) {
return APR_SUCCESS;
}
if (inctx->mode == AP_MODE_READBYTES ||
inctx->mode == AP_MODE_SPECULATIVE) {
if (readbytes < len) {
len = (apr_size_t)readbytes;
}
status = ssl_io_input_read(inctx, inctx->buffer, &len);
} else if (inctx->mode == AP_MODE_GETLINE) {
const char *pos;
if (inctx->cbuf.length
&& (pos = memchr(inctx->cbuf.value, APR_ASCII_LF,
inctx->cbuf.length)) != NULL) {
start = inctx->cbuf.value;
len = 1 + pos - start;
inctx->cbuf.value += len;
inctx->cbuf.length -= len;
status = APR_SUCCESS;
} else {
status = ssl_io_input_getline(inctx, inctx->buffer, &len);
}
} else {
status = APR_ENOTIMPL;
}
inctx->block = APR_BLOCK_READ;
if (status != APR_SUCCESS) {
return ssl_io_filter_error(inctx, bb, status, 0);
}
if (len > 0) {
bucket =
apr_bucket_transient_create(start, len, f->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bucket);
}
return APR_SUCCESS;
}
#define COALESCE_BYTES (2048)
struct coalesce_ctx {
char buffer[COALESCE_BYTES];
apr_size_t bytes;
};
static apr_status_t ssl_io_filter_coalesce(ap_filter_t *f,
apr_bucket_brigade *bb) {
apr_bucket *e, *upto;
apr_size_t bytes = 0;
struct coalesce_ctx *ctx = f->ctx;
unsigned count = 0;
for (e = APR_BRIGADE_FIRST(bb);
e != APR_BRIGADE_SENTINEL(bb)
&& !APR_BUCKET_IS_METADATA(e)
&& e->length != (apr_size_t)-1
&& e->length < COALESCE_BYTES
&& (bytes + e->length) < COALESCE_BYTES
&& (ctx == NULL
|| bytes + ctx->bytes + e->length < COALESCE_BYTES);
e = APR_BUCKET_NEXT(e)) {
if (e->length) count++;
bytes += e->length;
}
upto = e;
if (bytes > 0
&& (count > 1
|| (upto == APR_BRIGADE_SENTINEL(bb))
|| (ctx && ctx->bytes > 0))) {
if (!ctx) {
f->ctx = ctx = apr_palloc(f->c->pool, sizeof *ctx);
ctx->bytes = 0;
}
ap_log_cerror(APLOG_MARK, APLOG_TRACE4, 0, f->c,
"coalesce: have %" APR_SIZE_T_FMT " bytes, "
"adding %" APR_SIZE_T_FMT " more", ctx->bytes, bytes);
e = APR_BRIGADE_FIRST(bb);
while (e != upto) {
apr_size_t len;
const char *data;
apr_bucket *next;
if (APR_BUCKET_IS_METADATA(e)
|| e->length == (apr_size_t)-1) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, f->c, APLOGNO(02012)
"unexpected bucket type during coalesce");
break;
}
if (e->length) {
apr_status_t rv;
rv = apr_bucket_read(e, &data, &len, APR_BLOCK_READ);
if (rv) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, f->c, APLOGNO(02013)
"coalesce failed to read from data bucket");
return AP_FILTER_ERROR;
}
if (len > sizeof ctx->buffer
|| (len + ctx->bytes > sizeof ctx->buffer)) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, 0, f->c, APLOGNO(02014)
"unexpected coalesced bucket data length");
break;
}
memcpy(ctx->buffer + ctx->bytes, data, len);
ctx->bytes += len;
}
next = APR_BUCKET_NEXT(e);
apr_bucket_delete(e);
e = next;
}
}
if (APR_BRIGADE_EMPTY(bb)) {
return APR_SUCCESS;
}
if (ctx && ctx->bytes) {
ap_log_cerror(APLOG_MARK, APLOG_TRACE4, 0, f->c,
"coalesce: passing on %" APR_SIZE_T_FMT " bytes", ctx->bytes);
e = apr_bucket_transient_create(ctx->buffer, ctx->bytes, bb->bucket_alloc);
APR_BRIGADE_INSERT_HEAD(bb, e);
ctx->bytes = 0;
}
return ap_pass_brigade(f->next, bb);
}
static apr_status_t ssl_io_filter_output(ap_filter_t *f,
apr_bucket_brigade *bb) {
apr_status_t status = APR_SUCCESS;
ssl_filter_ctx_t *filter_ctx = f->ctx;
bio_filter_in_ctx_t *inctx;
bio_filter_out_ctx_t *outctx;
apr_read_type_e rblock = APR_NONBLOCK_READ;
if (f->c->aborted) {
apr_brigade_cleanup(bb);
return APR_ECONNABORTED;
}
if (!filter_ctx->pssl) {
return ap_pass_brigade(f->next, bb);
}
inctx = (bio_filter_in_ctx_t *)BIO_get_data(filter_ctx->pbioRead);
outctx = (bio_filter_out_ctx_t *)BIO_get_data(filter_ctx->pbioWrite);
inctx->mode = AP_MODE_READBYTES;
inctx->block = APR_BLOCK_READ;
if ((status = ssl_io_filter_handshake(filter_ctx)) != APR_SUCCESS) {
return ssl_io_filter_error(inctx, bb, status, 0);
}
while (!APR_BRIGADE_EMPTY(bb) && status == APR_SUCCESS) {
apr_bucket *bucket = APR_BRIGADE_FIRST(bb);
if (APR_BUCKET_IS_METADATA(bucket)) {
if (AP_BUCKET_IS_EOC(bucket)) {
ssl_filter_io_shutdown(filter_ctx, f->c, 0);
}
AP_DEBUG_ASSERT(APR_BRIGADE_EMPTY(outctx->bb));
APR_BUCKET_REMOVE(bucket);
APR_BRIGADE_INSERT_HEAD(outctx->bb, bucket);
status = ap_pass_brigade(f->next, outctx->bb);
if (status == APR_SUCCESS && f->c->aborted)
status = APR_ECONNRESET;
apr_brigade_cleanup(outctx->bb);
} else {
const char *data;
apr_size_t len;
status = apr_bucket_read(bucket, &data, &len, rblock);
if (APR_STATUS_IS_EAGAIN(status)) {
if (bio_filter_out_flush(filter_ctx->pbioWrite) < 0) {
status = outctx->rc;
break;
}
rblock = APR_BLOCK_READ;
status = APR_SUCCESS;
continue;
}
rblock = APR_NONBLOCK_READ;
if (!APR_STATUS_IS_EOF(status) && (status != APR_SUCCESS)) {
break;
}
status = ssl_filter_write(f, data, len);
apr_bucket_delete(bucket);
}
}
return status;
}
struct modssl_buffer_ctx {
apr_bucket_brigade *bb;
};
int ssl_io_buffer_fill(request_rec *r, apr_size_t maxlen) {
conn_rec *c = r->connection;
struct modssl_buffer_ctx *ctx;
apr_bucket_brigade *tempb;
apr_off_t total = 0;
int eos = 0;
ctx = apr_palloc(r->pool, sizeof *ctx);
ctx->bb = apr_brigade_create(r->pool, c->bucket_alloc);
tempb = apr_brigade_create(r->pool, c->bucket_alloc);
ap_log_cerror(APLOG_MARK, APLOG_TRACE4, 0, c, "filling buffer, max size "
"%" APR_SIZE_T_FMT " bytes", maxlen);
do {
apr_status_t rv;
apr_bucket *e, *next;
rv = ap_get_brigade(r->proto_input_filters, tempb, AP_MODE_READBYTES,
APR_BLOCK_READ, 8192);
if (rv) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(02015)
"could not read request body for SSL buffer");
return ap_map_http_request_error(rv, HTTP_INTERNAL_SERVER_ERROR);
}
for (e = APR_BRIGADE_FIRST(tempb);
e != APR_BRIGADE_SENTINEL(tempb) && !eos; e = next) {
const char *data;
apr_size_t len;
next = APR_BUCKET_NEXT(e);
if (APR_BUCKET_IS_EOS(e)) {
eos = 1;
} else if (!APR_BUCKET_IS_METADATA(e)) {
rv = apr_bucket_read(e, &data, &len, APR_BLOCK_READ);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(02016)
"could not read bucket for SSL buffer");
return HTTP_INTERNAL_SERVER_ERROR;
}
total += len;
}
rv = apr_bucket_setaside(e, r->pool);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(02017)
"could not setaside bucket for SSL buffer");
return HTTP_INTERNAL_SERVER_ERROR;
}
APR_BUCKET_REMOVE(e);
APR_BRIGADE_INSERT_TAIL(ctx->bb, e);
}
ap_log_cerror(APLOG_MARK, APLOG_TRACE4, 0, c,
"total of %" APR_OFF_T_FMT " bytes in buffer, eos=%d",
total, eos);
if (total > maxlen) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(02018)
"request body exceeds maximum size (%" APR_SIZE_T_FMT
") for SSL buffer", maxlen);
return HTTP_REQUEST_ENTITY_TOO_LARGE;
}
} while (!eos);
apr_brigade_destroy(tempb);
while (r->proto_input_filters->frec->ftype < AP_FTYPE_CONNECTION) {
ap_remove_input_filter(r->proto_input_filters);
}
ap_add_input_filter(ssl_io_buffer, ctx, r, c);
return 0;
}
static apr_status_t ssl_io_filter_buffer(ap_filter_t *f,
apr_bucket_brigade *bb,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t bytes) {
struct modssl_buffer_ctx *ctx = f->ctx;
apr_status_t rv;
apr_bucket *e, *d;
ap_log_cerror(APLOG_MARK, APLOG_TRACE4, 0, f->c,
"read from buffered SSL brigade, mode %d, "
"%" APR_OFF_T_FMT " bytes",
mode, bytes);
if (mode != AP_MODE_READBYTES && mode != AP_MODE_GETLINE) {
return APR_ENOTIMPL;
}
if (APR_BRIGADE_EMPTY(ctx->bb)) {
APR_BRIGADE_INSERT_TAIL(bb, apr_bucket_eos_create(f->c->bucket_alloc));
return APR_SUCCESS;
}
if (mode == AP_MODE_READBYTES) {
rv = apr_brigade_partition(ctx->bb, bytes, &e);
if (rv && rv != APR_INCOMPLETE) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, f->c, APLOGNO(02019)
"could not partition buffered SSL brigade");
ap_remove_input_filter(f);
return rv;
}
if (rv == APR_INCOMPLETE) {
APR_BRIGADE_CONCAT(bb, ctx->bb);
} else {
d = APR_BRIGADE_FIRST(ctx->bb);
e = APR_BUCKET_PREV(e);
APR_RING_UNSPLICE(d, e, link);
APR_RING_SPLICE_HEAD(&bb->list, d, e, apr_bucket, link);
APR_BRIGADE_CHECK_CONSISTENCY(bb);
APR_BRIGADE_CHECK_CONSISTENCY(ctx->bb);
}
} else {
rv = apr_brigade_split_line(bb, ctx->bb, block, bytes);
if (rv) {
ap_log_cerror(APLOG_MARK, APLOG_ERR, rv, f->c, APLOGNO(02020)
"could not split line from buffered SSL brigade");
ap_remove_input_filter(f);
return rv;
}
}
if (APR_BRIGADE_EMPTY(ctx->bb)) {
e = APR_BRIGADE_LAST(bb);
if (e == APR_BRIGADE_SENTINEL(bb) || !APR_BUCKET_IS_EOS(e)) {
e = apr_bucket_eos_create(f->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, e);
}
ap_log_cerror(APLOG_MARK, APLOG_TRACE4, 0, f->c,
"buffered SSL brigade exhausted");
}
return APR_SUCCESS;
}
static void ssl_io_input_add_filter(ssl_filter_ctx_t *filter_ctx, conn_rec *c,
request_rec *r, SSL *ssl) {
bio_filter_in_ctx_t *inctx;
inctx = apr_palloc(c->pool, sizeof(*inctx));
filter_ctx->pInputFilter = ap_add_input_filter(ssl_io_filter, inctx, r, c);
#if MODSSL_USE_OPENSSL_PRE_1_1_API
filter_ctx->pbioRead = BIO_new(&bio_filter_in_method);
#else
filter_ctx->pbioRead = BIO_new(bio_filter_in_method);
#endif
BIO_set_data(filter_ctx->pbioRead, (void *)inctx);
inctx->ssl = ssl;
inctx->bio_out = filter_ctx->pbioWrite;
inctx->f = filter_ctx->pInputFilter;
inctx->rc = APR_SUCCESS;
inctx->mode = AP_MODE_READBYTES;
inctx->cbuf.length = 0;
inctx->bb = apr_brigade_create(c->pool, c->bucket_alloc);
inctx->block = APR_BLOCK_READ;
inctx->pool = c->pool;
inctx->filter_ctx = filter_ctx;
}
void ssl_io_filter_init(conn_rec *c, request_rec *r, SSL *ssl) {
ssl_filter_ctx_t *filter_ctx;
filter_ctx = apr_palloc(c->pool, sizeof(ssl_filter_ctx_t));
filter_ctx->config = myConnConfig(c);
ap_add_output_filter(ssl_io_coalesce, NULL, r, c);
filter_ctx->pOutputFilter = ap_add_output_filter(ssl_io_filter,
filter_ctx, r, c);
#if MODSSL_USE_OPENSSL_PRE_1_1_API
filter_ctx->pbioWrite = BIO_new(&bio_filter_out_method);
#else
filter_ctx->pbioWrite = BIO_new(bio_filter_out_method);
#endif
BIO_set_data(filter_ctx->pbioWrite, (void *)bio_filter_out_ctx_new(filter_ctx, c));
if (c->cs) {
BIO_set_nbio(filter_ctx->pbioWrite, 1);
}
ssl_io_input_add_filter(filter_ctx, c, r, ssl);
SSL_set_bio(ssl, filter_ctx->pbioRead, filter_ctx->pbioWrite);
filter_ctx->pssl = ssl;
apr_pool_cleanup_register(c->pool, (void*)filter_ctx,
ssl_io_filter_cleanup, apr_pool_cleanup_null);
if (APLOG_CS_IS_LEVEL(c, mySrvFromConn(c), APLOG_TRACE4)) {
BIO *rbio = SSL_get_rbio(ssl),
*wbio = SSL_get_wbio(ssl);
BIO_set_callback(rbio, ssl_io_data_cb);
BIO_set_callback_arg(rbio, (void *)ssl);
if (wbio && wbio != rbio) {
BIO_set_callback(wbio, ssl_io_data_cb);
BIO_set_callback_arg(wbio, (void *)ssl);
}
}
return;
}
void ssl_io_filter_register(apr_pool_t *p) {
ap_register_input_filter (ssl_io_filter, ssl_io_filter_input, NULL, AP_FTYPE_CONNECTION + 5);
ap_register_output_filter (ssl_io_coalesce, ssl_io_filter_coalesce, NULL, AP_FTYPE_CONNECTION + 4);
ap_register_output_filter (ssl_io_filter, ssl_io_filter_output, NULL, AP_FTYPE_CONNECTION + 5);
ap_register_input_filter (ssl_io_buffer, ssl_io_filter_buffer, NULL, AP_FTYPE_PROTOCOL);
return;
}
#define DUMP_WIDTH 16
static void ssl_io_data_dump(server_rec *s,
const char *b,
long len) {
char buf[256];
char tmp[64];
int i, j, rows, trunc;
unsigned char ch;
trunc = 0;
for(; (len > 0) && ((b[len-1] == ' ') || (b[len-1] == '\0')); len--)
trunc++;
rows = (len / DUMP_WIDTH);
if ((rows * DUMP_WIDTH) < len)
rows++;
ap_log_error(APLOG_MARK, APLOG_TRACE7, 0, s,
"+-------------------------------------------------------------------------+");
for(i = 0 ; i< rows; i++) {
#if APR_CHARSET_EBCDIC
char ebcdic_text[DUMP_WIDTH];
j = DUMP_WIDTH;
if ((i * DUMP_WIDTH + j) > len)
j = len % DUMP_WIDTH;
if (j == 0)
j = DUMP_WIDTH;
memcpy(ebcdic_text,(char *)(b) + i * DUMP_WIDTH, j);
ap_xlate_proto_from_ascii(ebcdic_text, j);
#endif
apr_snprintf(tmp, sizeof(tmp), "| %04x: ", i * DUMP_WIDTH);
apr_cpystrn(buf, tmp, sizeof(buf));
for (j = 0; j < DUMP_WIDTH; j++) {
if (((i * DUMP_WIDTH) + j) >= len)
apr_cpystrn(buf+strlen(buf), " ", sizeof(buf)-strlen(buf));
else {
ch = ((unsigned char)*((char *)(b) + i * DUMP_WIDTH + j)) & 0xff;
apr_snprintf(tmp, sizeof(tmp), "%02x%c", ch , j==7 ? '-' : ' ');
apr_cpystrn(buf+strlen(buf), tmp, sizeof(buf)-strlen(buf));
}
}
apr_cpystrn(buf+strlen(buf), " ", sizeof(buf)-strlen(buf));
for (j = 0; j < DUMP_WIDTH; j++) {
if (((i * DUMP_WIDTH) + j) >= len)
apr_cpystrn(buf+strlen(buf), " ", sizeof(buf)-strlen(buf));
else {
ch = ((unsigned char)*((char *)(b) + i * DUMP_WIDTH + j)) & 0xff;
#if APR_CHARSET_EBCDIC
apr_snprintf(tmp, sizeof(tmp), "%c", (ch >= 0x20 && ch <= 0x7F) ? ebcdic_text[j] : '.');
#else
apr_snprintf(tmp, sizeof(tmp), "%c", ((ch >= ' ') && (ch <= '~')) ? ch : '.');
#endif
apr_cpystrn(buf+strlen(buf), tmp, sizeof(buf)-strlen(buf));
}
}
apr_cpystrn(buf+strlen(buf), " |", sizeof(buf)-strlen(buf));
ap_log_error(APLOG_MARK, APLOG_TRACE7, 0, s, "%s", buf);
}
if (trunc > 0)
ap_log_error(APLOG_MARK, APLOG_TRACE7, 0, s,
"| %04ld - <SPACES/NULS>", len + trunc);
ap_log_error(APLOG_MARK, APLOG_TRACE7, 0, s,
"+-------------------------------------------------------------------------+");
return;
}
long ssl_io_data_cb(BIO *bio, int cmd,
const char *argp,
int argi, long argl, long rc) {
SSL *ssl;
conn_rec *c;
server_rec *s;
if ((ssl = (SSL *)BIO_get_callback_arg(bio)) == NULL)
return rc;
if ((c = (conn_rec *)SSL_get_app_data(ssl)) == NULL)
return rc;
s = mySrvFromConn(c);
if ( cmd == (BIO_CB_WRITE|BIO_CB_RETURN)
|| cmd == (BIO_CB_READ |BIO_CB_RETURN) ) {
if (rc >= 0) {
ap_log_cserror(APLOG_MARK, APLOG_TRACE4, 0, c, s,
"%s: %s %ld/%d bytes %s BIO#%pp [mem: %pp] %s",
MODSSL_LIBRARY_NAME,
(cmd == (BIO_CB_WRITE|BIO_CB_RETURN) ? "write" : "read"),
rc, argi, (cmd == (BIO_CB_WRITE|BIO_CB_RETURN) ? "to" : "from"),
bio, argp,
(argp != NULL ? "(BIO dump follows)" : "(Oops, no memory buffer?)"));
if ((argp != NULL) && APLOG_CS_IS_LEVEL(c, s, APLOG_TRACE7))
ssl_io_data_dump(s, argp, rc);
} else {
ap_log_cserror(APLOG_MARK, APLOG_TRACE4, 0, c, s,
"%s: I/O error, %d bytes expected to %s on BIO#%pp [mem: %pp]",
MODSSL_LIBRARY_NAME, argi,
(cmd == (BIO_CB_WRITE|BIO_CB_RETURN) ? "write" : "read"),
bio, argp);
}
}
return rc;
}