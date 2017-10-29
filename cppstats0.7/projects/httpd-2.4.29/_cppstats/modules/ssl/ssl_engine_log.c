#include "ssl_private.h"
static const struct {
const char *cpPattern;
const char *cpAnnotation;
} ssl_log_annotate[] = {
{ "*envelope*bad*decrypt*", "wrong pass phrase!?" },
{ "*CLIENT_HELLO*unknown*protocol*", "speaking not SSL to HTTPS port!?" },
{ "*CLIENT_HELLO*http*request*", "speaking HTTP to HTTPS port!?" },
{ "*SSL3_READ_BYTES:sslv3*alert*bad*certificate*", "Subject CN in certificate not server name or identical to CA!?" },
{ "*self signed certificate in certificate chain*", "Client certificate signed by CA not known to server?" },
{ "*peer did not return a certificate*", "No CAs known to server for verification?" },
{ "*no shared cipher*", "Too restrictive SSLCipherSuite or using DSA server certificate?" },
{ "*no start line*", "Bad file contents or format - or even just a forgotten SSLCertificateKeyFile?" },
{ "*bad password read*", "You entered an incorrect pass phrase!?" },
{ "*bad mac decode*", "Browser still remembered details of a re-created server certificate?" },
{ NULL, NULL }
};
static const char *ssl_log_annotation(const char *error) {
int i = 0;
while (ssl_log_annotate[i].cpPattern != NULL
&& ap_strcmp_match(error, ssl_log_annotate[i].cpPattern) != 0)
i++;
return ssl_log_annotate[i].cpAnnotation;
}
apr_status_t ssl_die(server_rec *s) {
if (s != NULL && s->is_virtual && s->error_fname != NULL)
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, NULL, APLOGNO(02311)
"Fatal error initialising mod_ssl, exiting. "
"See %s for more information",
ap_server_root_relative(s->process->pool,
s->error_fname));
else
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, NULL, APLOGNO(02312)
"Fatal error initialising mod_ssl, exiting.");
return APR_EGENERAL;
}
void ssl_log_ssl_error(const char *file, int line, int level, server_rec *s) {
unsigned long e;
const char *data;
int flags;
while ((e = ERR_peek_error_line_data(NULL, NULL, &data, &flags))) {
const char *annotation;
char err[256];
if (!(flags & ERR_TXT_STRING)) {
data = NULL;
}
ERR_error_string_n(e, err, sizeof err);
annotation = ssl_log_annotation(err);
ap_log_error(file, line, APLOG_MODULE_INDEX, level, 0, s,
"SSL Library Error: %s%s%s%s%s%s",
err,
data ? " (" : "", data ? data : "", data ? ")" : "",
annotation ? " -- " : "",
annotation ? annotation : "");
ERR_get_error();
}
}
static void ssl_log_cert_error(const char *file, int line, int level,
apr_status_t rv, const server_rec *s,
const conn_rec *c, const request_rec *r,
apr_pool_t *p, X509 *cert, const char *format,
va_list ap) {
char buf[HUGE_STRING_LEN];
int msglen, n;
char *name;
apr_vsnprintf(buf, sizeof buf, format, ap);
msglen = strlen(buf);
if (cert) {
BIO *bio = BIO_new(BIO_s_mem());
if (bio) {
int maxdnlen = (HUGE_STRING_LEN - msglen - 300) / 2;
BIO_puts(bio, " [subject: ");
name = modssl_X509_NAME_to_string(p, X509_get_subject_name(cert),
maxdnlen);
if (!strIsEmpty(name)) {
BIO_puts(bio, name);
} else {
BIO_puts(bio, "-empty-");
}
BIO_puts(bio, " / issuer: ");
name = modssl_X509_NAME_to_string(p, X509_get_issuer_name(cert),
maxdnlen);
if (!strIsEmpty(name)) {
BIO_puts(bio, name);
} else {
BIO_puts(bio, "-empty-");
}
BIO_puts(bio, " / serial: ");
if (i2a_ASN1_INTEGER(bio, X509_get_serialNumber(cert)) == -1)
BIO_puts(bio, "(ERROR)");
BIO_puts(bio, " / notbefore: ");
ASN1_TIME_print(bio, X509_get_notBefore(cert));
BIO_puts(bio, " / notafter: ");
ASN1_TIME_print(bio, X509_get_notAfter(cert));
BIO_puts(bio, "]");
n = BIO_read(bio, buf + msglen, sizeof buf - msglen - 1);
if (n > 0)
buf[msglen + n] = '\0';
BIO_free(bio);
}
} else {
apr_snprintf(buf + msglen, sizeof buf - msglen,
" [certificate: -not available-]");
}
if (r) {
ap_log_rerror(file, line, APLOG_MODULE_INDEX, level, rv, r, "%s", buf);
} else if (c) {
ap_log_cerror(file, line, APLOG_MODULE_INDEX, level, rv, c, "%s", buf);
} else if (s) {
ap_log_error(file, line, APLOG_MODULE_INDEX, level, rv, s, "%s", buf);
}
}
void ssl_log_xerror(const char *file, int line, int level, apr_status_t rv,
apr_pool_t *ptemp, server_rec *s, X509 *cert,
const char *fmt, ...) {
if (APLOG_IS_LEVEL(s,level)) {
va_list ap;
va_start(ap, fmt);
ssl_log_cert_error(file, line, level, rv, s, NULL, NULL, ptemp,
cert, fmt, ap);
va_end(ap);
}
}
void ssl_log_cxerror(const char *file, int line, int level, apr_status_t rv,
conn_rec *c, X509 *cert, const char *fmt, ...) {
if (APLOG_IS_LEVEL(mySrvFromConn(c),level)) {
va_list ap;
va_start(ap, fmt);
ssl_log_cert_error(file, line, level, rv, NULL, c, NULL, c->pool,
cert, fmt, ap);
va_end(ap);
}
}
void ssl_log_rxerror(const char *file, int line, int level, apr_status_t rv,
request_rec *r, X509 *cert, const char *fmt, ...) {
if (APLOG_R_IS_LEVEL(r,level)) {
va_list ap;
va_start(ap, fmt);
ssl_log_cert_error(file, line, level, rv, NULL, NULL, r, r->pool,
cert, fmt, ap);
va_end(ap);
}
}
