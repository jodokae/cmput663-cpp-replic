#define AP_AB_BASEREVISION "2.3"
#if 'A' != 0x41
#define NOT_ASCII
#endif
#define BSD_COMP
#include "apr.h"
#include "apr_signal.h"
#include "apr_strings.h"
#include "apr_network_io.h"
#include "apr_file_io.h"
#include "apr_time.h"
#include "apr_getopt.h"
#include "apr_general.h"
#include "apr_lib.h"
#include "apr_portable.h"
#include "ap_release.h"
#include "apr_poll.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "apr_base64.h"
#if defined(NOT_ASCII)
#include "apr_xlate.h"
#endif
#if APR_HAVE_STDIO_H
#include <stdio.h>
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if !defined(WIN32) && !defined(NETWARE)
#include "ap_config_auto.h"
#endif
#if defined(HAVE_OPENSSL)
#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#define USE_SSL
#define SK_NUM(x) sk_X509_num(x)
#define SK_VALUE(x,y) sk_X509_value(x,y)
typedef STACK_OF(X509) X509_STACK_TYPE;
#if defined(_MSC_VER)
#include <openssl/applink.c>
#endif
#endif
#if defined(USE_SSL)
#if (OPENSSL_VERSION_NUMBER >= 0x00909000)
#define AB_SSL_METHOD_CONST const
#else
#define AB_SSL_METHOD_CONST
#endif
#if (OPENSSL_VERSION_NUMBER >= 0x0090707f)
#define AB_SSL_CIPHER_CONST const
#else
#define AB_SSL_CIPHER_CONST
#endif
#if defined(SSL_OP_NO_TLSv1_2)
#define HAVE_TLSV1_X
#endif
#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_set_tlsext_host_name)
#define HAVE_TLSEXT
#endif
#if defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2060000f
#define SSL_CTRL_SET_MIN_PROTO_VERSION 123
#define SSL_CTRL_SET_MAX_PROTO_VERSION 124
#define SSL_CTX_set_min_proto_version(ctx, version) SSL_CTX_ctrl(ctx, SSL_CTRL_SET_MIN_PROTO_VERSION, version, NULL)
#define SSL_CTX_set_max_proto_version(ctx, version) SSL_CTX_ctrl(ctx, SSL_CTRL_SET_MAX_PROTO_VERSION, version, NULL)
#endif
#endif
#include <math.h>
#if APR_HAVE_CTYPE_H
#include <ctype.h>
#endif
#if APR_HAVE_LIMITS_H
#include <limits.h>
#endif
#if !defined(LLONG_MAX)
#define AB_MAX APR_INT64_C(0x7fffffffffffffff)
#else
#define AB_MAX LLONG_MAX
#endif
#define MAX_REQUESTS (INT_MAX > 50000 ? 50000 : INT_MAX)
typedef enum {
STATE_UNCONNECTED = 0,
STATE_CONNECTING,
STATE_CONNECTED,
STATE_READ
} connect_state_e;
#define CBUFFSIZE (8192)
struct connection {
apr_pool_t *ctx;
apr_socket_t *aprsock;
apr_pollfd_t pollfd;
int state;
apr_size_t read;
apr_size_t bread;
apr_size_t rwrite, rwrote;
apr_size_t length;
char cbuff[CBUFFSIZE];
int cbx;
int keepalive;
int gotheader;
apr_time_t start,
connect,
endwrite,
beginread,
done;
int socknum;
#if defined(USE_SSL)
SSL *ssl;
#endif
};
struct data {
apr_time_t starttime;
apr_interval_time_t waittime;
apr_interval_time_t ctime;
apr_interval_time_t time;
};
#define ap_min(a,b) (((a)<(b))?(a):(b))
#define ap_max(a,b) (((a)>(b))?(a):(b))
#define ap_round_ms(a) ((apr_time_t)((a) + 500)/1000)
#define ap_double_ms(a) ((double)(a)/1000.0)
#define MAX_CONCURRENCY 20000
int verbosity = 0;
int recverrok = 0;
enum {NO_METH = 0, GET, HEAD, PUT, POST, CUSTOM_METHOD} method = NO_METH;
const char *method_str[] = {"bug", "GET", "HEAD", "PUT", "POST", ""};
int send_body = 0;
int requests = 1;
int heartbeatres = 100;
int concurrency = 1;
int percentile = 1;
int nolength = 0;
int confidence = 1;
int tlimit = 0;
int keepalive = 0;
int windowsize = 0;
char servername[1024];
char *hostname;
const char *host_field;
const char *path;
char *postdata;
apr_size_t postlen = 0;
char *content_type = NULL;
const char *cookie,
*auth,
*hdrs;
apr_port_t port;
char *proxyhost = NULL;
int proxyport = 0;
const char *connecthost;
const char *myhost;
apr_port_t connectport;
const char *gnuplot;
const char *csvperc;
const char *fullurl;
const char *colonhost;
int isproxy = 0;
apr_interval_time_t aprtimeout = apr_time_from_sec(30);
const char *opt_host;
int opt_useragent = 0;
int opt_accept = 0;
int use_html = 0;
const char *tablestring;
const char *trstring;
const char *tdstring;
apr_size_t doclen = 0;
apr_int64_t totalread = 0;
apr_int64_t totalbread = 0;
apr_int64_t totalposted = 0;
int started = 0;
int done = 0;
int doneka = 0;
int good = 0, bad = 0;
int epipe = 0;
int err_length = 0;
int err_conn = 0;
int err_recv = 0;
int err_except = 0;
int err_response = 0;
#if defined(USE_SSL)
int is_ssl;
SSL_CTX *ssl_ctx;
char *ssl_cipher = NULL;
char *ssl_info = NULL;
BIO *bio_out,*bio_err;
#if defined(HAVE_TLSEXT)
int tls_use_sni = 1;
const char *tls_sni = NULL;
#endif
#endif
apr_time_t start, lasttime, stoptime;
char _request[8192];
char *request = _request;
apr_size_t reqlen;
char buffer[8192];
int percs[] = {50, 66, 75, 80, 90, 95, 98, 99, 100};
struct connection *con;
struct data *stats;
apr_pool_t *cntxt;
apr_pollset_t *readbits;
apr_sockaddr_t *mysa;
apr_sockaddr_t *destsa;
#if defined(NOT_ASCII)
apr_xlate_t *from_ascii, *to_ascii;
#endif
static void write_request(struct connection * c);
static void close_connection(struct connection * c);
static void err(const char *s) {
fprintf(stderr, "%s\n", s);
if (done)
printf("Total of %d requests completed\n" , done);
exit(1);
}
static void apr_err(const char *s, apr_status_t rv) {
char buf[120];
fprintf(stderr,
"%s: %s (%d)\n",
s, apr_strerror(rv, buf, sizeof buf), rv);
if (done)
printf("Total of %d requests completed\n" , done);
exit(rv);
}
static void *xmalloc(size_t size) {
void *ret = malloc(size);
if (ret == NULL) {
fprintf(stderr, "Could not allocate memory (%"
APR_SIZE_T_FMT" bytes)\n", size);
exit(1);
}
return ret;
}
static void *xcalloc(size_t num, size_t size) {
void *ret = calloc(num, size);
if (ret == NULL) {
fprintf(stderr, "Could not allocate memory (%"
APR_SIZE_T_FMT" bytes)\n", size*num);
exit(1);
}
return ret;
}
static char *xstrdup(const char *s) {
char *ret = strdup(s);
if (ret == NULL) {
fprintf(stderr, "Could not allocate memory (%"
APR_SIZE_T_FMT " bytes)\n", strlen(s));
exit(1);
}
return ret;
}
static char *xstrcasestr(const char *s1, const char *s2) {
char *p1, *p2;
if (*s2 == '\0') {
return((char *)s1);
}
while(1) {
for ( ; (*s1 != '\0') && (apr_tolower(*s1) != apr_tolower(*s2)); s1++);
if (*s1 == '\0') {
return(NULL);
}
p1 = (char *)s1;
p2 = (char *)s2;
for (++p1, ++p2; apr_tolower(*p1) == apr_tolower(*p2); ++p1, ++p2) {
if (*p1 == '\0') {
return((char *)s1);
}
}
if (*p2 == '\0') {
break;
}
s1++;
}
return((char *)s1);
}
static int abort_on_oom(int retcode) {
fprintf(stderr, "Could not allocate memory\n");
exit(1);
return retcode;
}
static void set_polled_events(struct connection *c, apr_int16_t new_reqevents) {
apr_status_t rv;
if (c->pollfd.reqevents != new_reqevents) {
if (c->pollfd.reqevents != 0) {
rv = apr_pollset_remove(readbits, &c->pollfd);
if (rv != APR_SUCCESS) {
apr_err("apr_pollset_remove()", rv);
}
}
if (new_reqevents != 0) {
c->pollfd.reqevents = new_reqevents;
rv = apr_pollset_add(readbits, &c->pollfd);
if (rv != APR_SUCCESS) {
apr_err("apr_pollset_add()", rv);
}
}
}
}
static void set_conn_state(struct connection *c, connect_state_e new_state) {
apr_int16_t events_by_state[] = {
0,
APR_POLLOUT,
APR_POLLIN,
APR_POLLIN
};
c->state = new_state;
set_polled_events(c, events_by_state[new_state]);
}
#if defined(USE_SSL)
static long ssl_print_cb(BIO *bio,int cmd,const char *argp,int argi,long argl,long ret) {
BIO *out;
out=(BIO *)BIO_get_callback_arg(bio);
if (out == NULL) return(ret);
if (cmd == (BIO_CB_READ|BIO_CB_RETURN)) {
BIO_printf(out,"read from %p [%p] (%d bytes => %ld (0x%lX))\n",
bio, argp, argi, ret, ret);
BIO_dump(out,(char *)argp,(int)ret);
return(ret);
} else if (cmd == (BIO_CB_WRITE|BIO_CB_RETURN)) {
BIO_printf(out,"write to %p [%p] (%d bytes => %ld (0x%lX))\n",
bio, argp, argi, ret, ret);
BIO_dump(out,(char *)argp,(int)ret);
}
return ret;
}
static void ssl_state_cb(const SSL *s, int w, int r) {
if (w & SSL_CB_ALERT) {
BIO_printf(bio_err, "SSL/TLS Alert [%s] %s:%s\n",
(w & SSL_CB_READ ? "read" : "write"),
SSL_alert_type_string_long(r),
SSL_alert_desc_string_long(r));
} else if (w & SSL_CB_LOOP) {
BIO_printf(bio_err, "SSL/TLS State [%s] %s\n",
(SSL_in_connect_init((SSL*)s) ? "connect" : "-"),
SSL_state_string_long(s));
} else if (w & (SSL_CB_HANDSHAKE_START|SSL_CB_HANDSHAKE_DONE)) {
BIO_printf(bio_err, "SSL/TLS Handshake [%s] %s\n",
(w & SSL_CB_HANDSHAKE_START ? "Start" : "Done"),
SSL_state_string_long(s));
}
}
#if !defined(RAND_MAX)
#define RAND_MAX INT_MAX
#endif
static int ssl_rand_choosenum(int l, int h) {
int i;
char buf[50];
srand((unsigned int)time(NULL));
apr_snprintf(buf, sizeof(buf), "%.0f",
(((double)(rand()%RAND_MAX)/RAND_MAX)*(h-l)));
i = atoi(buf)+1;
if (i < l) i = l;
if (i > h) i = h;
return i;
}
static void ssl_rand_seed(void) {
int n, l;
time_t t;
pid_t pid;
unsigned char stackdata[256];
t = time(NULL);
l = sizeof(time_t);
RAND_seed((unsigned char *)&t, l);
pid = getpid();
l = sizeof(pid_t);
RAND_seed((unsigned char *)&pid, l);
n = ssl_rand_choosenum(0, sizeof(stackdata)-128-1);
RAND_seed(stackdata+n, 128);
}
static int ssl_print_connection_info(BIO *bio, SSL *ssl) {
AB_SSL_CIPHER_CONST SSL_CIPHER *c;
int alg_bits,bits;
BIO_printf(bio,"Transport Protocol :%s\n", SSL_get_version(ssl));
c = SSL_get_current_cipher(ssl);
BIO_printf(bio,"Cipher Suite Protocol :%s\n", SSL_CIPHER_get_version(c));
BIO_printf(bio,"Cipher Suite Name :%s\n",SSL_CIPHER_get_name(c));
bits = SSL_CIPHER_get_bits(c,&alg_bits);
BIO_printf(bio,"Cipher Suite Cipher Bits:%d (%d)\n",bits,alg_bits);
return(1);
}
static void ssl_print_cert_info(BIO *bio, X509 *cert) {
X509_NAME *dn;
EVP_PKEY *pk;
char buf[1024];
BIO_printf(bio, "Certificate version: %ld\n", X509_get_version(cert)+1);
BIO_printf(bio,"Valid from: ");
ASN1_UTCTIME_print(bio, X509_get_notBefore(cert));
BIO_printf(bio,"\n");
BIO_printf(bio,"Valid to : ");
ASN1_UTCTIME_print(bio, X509_get_notAfter(cert));
BIO_printf(bio,"\n");
pk = X509_get_pubkey(cert);
BIO_printf(bio,"Public key is %d bits\n",
EVP_PKEY_bits(pk));
EVP_PKEY_free(pk);
dn = X509_get_issuer_name(cert);
X509_NAME_oneline(dn, buf, sizeof(buf));
BIO_printf(bio,"The issuer name is %s\n", buf);
dn=X509_get_subject_name(cert);
X509_NAME_oneline(dn, buf, sizeof(buf));
BIO_printf(bio,"The subject name is %s\n", buf);
BIO_printf(bio, "Extension Count: %d\n", X509_get_ext_count(cert));
}
static void ssl_print_info(struct connection *c) {
X509_STACK_TYPE *sk;
X509 *cert;
int count;
BIO_printf(bio_err, "\n");
sk = SSL_get_peer_cert_chain(c->ssl);
if ((count = SK_NUM(sk)) > 0) {
int i;
for (i=1; i<count; i++) {
cert = (X509 *)SK_VALUE(sk, i);
ssl_print_cert_info(bio_out, cert);
}
}
cert = SSL_get_peer_certificate(c->ssl);
if (cert == NULL) {
BIO_printf(bio_out, "Anon DH\n");
} else {
BIO_printf(bio_out, "Peer certificate\n");
ssl_print_cert_info(bio_out, cert);
X509_free(cert);
}
ssl_print_connection_info(bio_err,c->ssl);
SSL_SESSION_print(bio_err, SSL_get_session(c->ssl));
}
static void ssl_proceed_handshake(struct connection *c) {
int do_next = 1;
while (do_next) {
int ret, ecode;
ret = SSL_do_handshake(c->ssl);
ecode = SSL_get_error(c->ssl, ret);
switch (ecode) {
case SSL_ERROR_NONE:
if (verbosity >= 2)
ssl_print_info(c);
if (ssl_info == NULL) {
AB_SSL_CIPHER_CONST SSL_CIPHER *ci;
X509 *cert;
int sk_bits, pk_bits, swork;
ci = SSL_get_current_cipher(c->ssl);
sk_bits = SSL_CIPHER_get_bits(ci, &swork);
cert = SSL_get_peer_certificate(c->ssl);
if (cert)
pk_bits = EVP_PKEY_bits(X509_get_pubkey(cert));
else
pk_bits = 0;
ssl_info = xmalloc(128);
apr_snprintf(ssl_info, 128, "%s,%s,%d,%d",
SSL_get_version(c->ssl),
SSL_CIPHER_get_name(ci),
pk_bits, sk_bits);
}
write_request(c);
do_next = 0;
break;
case SSL_ERROR_WANT_READ:
set_polled_events(c, APR_POLLIN);
do_next = 0;
break;
case SSL_ERROR_WANT_WRITE:
do_next = 1;
break;
case SSL_ERROR_WANT_CONNECT:
case SSL_ERROR_SSL:
case SSL_ERROR_SYSCALL:
BIO_printf(bio_err, "SSL handshake failed (%d).\n", ecode);
ERR_print_errors(bio_err);
close_connection(c);
do_next = 0;
break;
}
}
}
#endif
static void write_request(struct connection * c) {
if (started >= requests) {
return;
}
do {
apr_time_t tnow;
apr_size_t l = c->rwrite;
apr_status_t e = APR_SUCCESS;
tnow = lasttime = apr_time_now();
if (c->rwrite == 0) {
apr_socket_timeout_set(c->aprsock, 0);
c->connect = tnow;
c->rwrote = 0;
c->rwrite = reqlen;
if (send_body)
c->rwrite += postlen;
} else if (tnow > c->connect + aprtimeout) {
printf("Send request timed out!\n");
close_connection(c);
return;
}
#if defined(USE_SSL)
if (c->ssl) {
apr_size_t e_ssl;
e_ssl = SSL_write(c->ssl,request + c->rwrote, l);
if (e_ssl != l) {
BIO_printf(bio_err, "SSL write failed - closing connection\n");
ERR_print_errors(bio_err);
close_connection (c);
return;
}
l = e_ssl;
e = APR_SUCCESS;
} else
#endif
e = apr_socket_send(c->aprsock, request + c->rwrote, &l);
if (e != APR_SUCCESS && !APR_STATUS_IS_EAGAIN(e)) {
epipe++;
printf("Send request failed!\n");
close_connection(c);
return;
}
totalposted += l;
c->rwrote += l;
c->rwrite -= l;
} while (c->rwrite);
c->endwrite = lasttime = apr_time_now();
started++;
set_conn_state(c, STATE_READ);
}
static int compradre(struct data * a, struct data * b) {
if ((a->ctime) < (b->ctime))
return -1;
if ((a->ctime) > (b->ctime))
return +1;
return 0;
}
static int comprando(struct data * a, struct data * b) {
if ((a->time) < (b->time))
return -1;
if ((a->time) > (b->time))
return +1;
return 0;
}
static int compri(struct data * a, struct data * b) {
apr_interval_time_t p = a->time - a->ctime;
apr_interval_time_t q = b->time - b->ctime;
if (p < q)
return -1;
if (p > q)
return +1;
return 0;
}
static int compwait(struct data * a, struct data * b) {
if ((a->waittime) < (b->waittime))
return -1;
if ((a->waittime) > (b->waittime))
return 1;
return 0;
}
static void output_results(int sig) {
double timetaken;
if (sig) {
lasttime = apr_time_now();
}
timetaken = (double) (lasttime - start) / APR_USEC_PER_SEC;
printf("\n\n");
printf("Server Software: %s\n", servername);
printf("Server Hostname: %s\n", hostname);
printf("Server Port: %hu\n", port);
#if defined(USE_SSL)
if (is_ssl && ssl_info) {
printf("SSL/TLS Protocol: %s\n", ssl_info);
}
#if defined(HAVE_TLSEXT)
if (is_ssl && tls_sni) {
printf("TLS Server Name: %s\n", tls_sni);
}
#endif
#endif
printf("\n");
printf("Document Path: %s\n", path);
if (nolength)
printf("Document Length: Variable\n");
else
printf("Document Length: %" APR_SIZE_T_FMT " bytes\n", doclen);
printf("\n");
printf("Concurrency Level: %d\n", concurrency);
printf("Time taken for tests: %.3f seconds\n", timetaken);
printf("Complete requests: %d\n", done);
printf("Failed requests: %d\n", bad);
if (bad)
printf(" (Connect: %d, Receive: %d, Length: %d, Exceptions: %d)\n",
err_conn, err_recv, err_length, err_except);
if (epipe)
printf("Write errors: %d\n", epipe);
if (err_response)
printf("Non-2xx responses: %d\n", err_response);
if (keepalive)
printf("Keep-Alive requests: %d\n", doneka);
printf("Total transferred: %" APR_INT64_T_FMT " bytes\n", totalread);
if (send_body)
printf("Total body sent: %" APR_INT64_T_FMT "\n",
totalposted);
printf("HTML transferred: %" APR_INT64_T_FMT " bytes\n", totalbread);
if (timetaken && done) {
printf("Requests per second: %.2f [#/sec] (mean)\n",
(double) done / timetaken);
printf("Time per request: %.3f [ms] (mean)\n",
(double) concurrency * timetaken * 1000 / done);
printf("Time per request: %.3f [ms] (mean, across all concurrent requests)\n",
(double) timetaken * 1000 / done);
printf("Transfer rate: %.2f [Kbytes/sec] received\n",
(double) totalread / 1024 / timetaken);
if (send_body) {
printf(" %.2f kb/s sent\n",
(double) totalposted / 1024 / timetaken);
printf(" %.2f kb/s total\n",
(double) (totalread + totalposted) / 1024 / timetaken);
}
}
if (done > 0) {
int i;
apr_time_t totalcon = 0, total = 0, totald = 0, totalwait = 0;
apr_time_t meancon, meantot, meand, meanwait;
apr_interval_time_t mincon = AB_MAX, mintot = AB_MAX, mind = AB_MAX,
minwait = AB_MAX;
apr_interval_time_t maxcon = 0, maxtot = 0, maxd = 0, maxwait = 0;
apr_interval_time_t mediancon = 0, mediantot = 0, mediand = 0, medianwait = 0;
double sdtot = 0, sdcon = 0, sdd = 0, sdwait = 0;
for (i = 0; i < done; i++) {
struct data *s = &stats[i];
mincon = ap_min(mincon, s->ctime);
mintot = ap_min(mintot, s->time);
mind = ap_min(mind, s->time - s->ctime);
minwait = ap_min(minwait, s->waittime);
maxcon = ap_max(maxcon, s->ctime);
maxtot = ap_max(maxtot, s->time);
maxd = ap_max(maxd, s->time - s->ctime);
maxwait = ap_max(maxwait, s->waittime);
totalcon += s->ctime;
total += s->time;
totald += s->time - s->ctime;
totalwait += s->waittime;
}
meancon = totalcon / done;
meantot = total / done;
meand = totald / done;
meanwait = totalwait / done;
for (i = 0; i < done; i++) {
struct data *s = &stats[i];
double a;
a = ((double)s->time - meantot);
sdtot += a * a;
a = ((double)s->ctime - meancon);
sdcon += a * a;
a = ((double)s->time - (double)s->ctime - meand);
sdd += a * a;
a = ((double)s->waittime - meanwait);
sdwait += a * a;
}
sdtot = (done > 1) ? sqrt(sdtot / (done - 1)) : 0;
sdcon = (done > 1) ? sqrt(sdcon / (done - 1)) : 0;
sdd = (done > 1) ? sqrt(sdd / (done - 1)) : 0;
sdwait = (done > 1) ? sqrt(sdwait / (done - 1)) : 0;
qsort(stats, done, sizeof(struct data),
(int (*) (const void *, const void *)) compradre);
if ((done > 1) && (done % 2))
mediancon = (stats[done / 2].ctime + stats[done / 2 + 1].ctime) / 2;
else
mediancon = stats[done / 2].ctime;
qsort(stats, done, sizeof(struct data),
(int (*) (const void *, const void *)) compri);
if ((done > 1) && (done % 2))
mediand = (stats[done / 2].time + stats[done / 2 + 1].time \
-stats[done / 2].ctime - stats[done / 2 + 1].ctime) / 2;
else
mediand = stats[done / 2].time - stats[done / 2].ctime;
qsort(stats, done, sizeof(struct data),
(int (*) (const void *, const void *)) compwait);
if ((done > 1) && (done % 2))
medianwait = (stats[done / 2].waittime + stats[done / 2 + 1].waittime) / 2;
else
medianwait = stats[done / 2].waittime;
qsort(stats, done, sizeof(struct data),
(int (*) (const void *, const void *)) comprando);
if ((done > 1) && (done % 2))
mediantot = (stats[done / 2].time + stats[done / 2 + 1].time) / 2;
else
mediantot = stats[done / 2].time;
printf("\nConnection Times (ms)\n");
mincon = ap_round_ms(mincon);
mind = ap_round_ms(mind);
minwait = ap_round_ms(minwait);
mintot = ap_round_ms(mintot);
meancon = ap_round_ms(meancon);
meand = ap_round_ms(meand);
meanwait = ap_round_ms(meanwait);
meantot = ap_round_ms(meantot);
mediancon = ap_round_ms(mediancon);
mediand = ap_round_ms(mediand);
medianwait = ap_round_ms(medianwait);
mediantot = ap_round_ms(mediantot);
maxcon = ap_round_ms(maxcon);
maxd = ap_round_ms(maxd);
maxwait = ap_round_ms(maxwait);
maxtot = ap_round_ms(maxtot);
sdcon = ap_double_ms(sdcon);
sdd = ap_double_ms(sdd);
sdwait = ap_double_ms(sdwait);
sdtot = ap_double_ms(sdtot);
if (confidence) {
#define CONF_FMT_STRING "%5" APR_TIME_T_FMT " %4" APR_TIME_T_FMT " %5.1f %6" APR_TIME_T_FMT " %7" APR_TIME_T_FMT "\n"
printf(" min mean[+/-sd] median max\n");
printf("Connect: " CONF_FMT_STRING,
mincon, meancon, sdcon, mediancon, maxcon);
printf("Processing: " CONF_FMT_STRING,
mind, meand, sdd, mediand, maxd);
printf("Waiting: " CONF_FMT_STRING,
minwait, meanwait, sdwait, medianwait, maxwait);
printf("Total: " CONF_FMT_STRING,
mintot, meantot, sdtot, mediantot, maxtot);
#undef CONF_FMT_STRING
#define SANE(what,mean,median,sd) { double d = (double)mean - median; if (d < 0) d = -d; if (d > 2 * sd ) printf("ERROR: The median and mean for " what " are more than twice the standard\n" " deviation apart. These results are NOT reliable.\n"); else if (d > sd ) printf("WARNING: The median and mean for " what " are not within a normal deviation\n" " These results are probably not that reliable.\n"); }
SANE("the initial connection time", meancon, mediancon, sdcon);
SANE("the processing time", meand, mediand, sdd);
SANE("the waiting time", meanwait, medianwait, sdwait);
SANE("the total time", meantot, mediantot, sdtot);
} else {
printf(" min avg max\n");
#define CONF_FMT_STRING "%5" APR_TIME_T_FMT " %5" APR_TIME_T_FMT "%5" APR_TIME_T_FMT "\n"
printf("Connect: " CONF_FMT_STRING, mincon, meancon, maxcon);
printf("Processing: " CONF_FMT_STRING, mind, meand, maxd);
printf("Waiting: " CONF_FMT_STRING, minwait, meanwait, maxwait);
printf("Total: " CONF_FMT_STRING, mintot, meantot, maxtot);
#undef CONF_FMT_STRING
}
if (percentile && (done > 1)) {
printf("\nPercentage of the requests served within a certain time (ms)\n");
for (i = 0; i < sizeof(percs) / sizeof(int); i++) {
if (percs[i] <= 0)
printf(" 0%% <0> (never)\n");
else if (percs[i] >= 100)
printf(" 100%% %5" APR_TIME_T_FMT " (longest request)\n",
ap_round_ms(stats[done - 1].time));
else
printf(" %d%% %5" APR_TIME_T_FMT "\n", percs[i],
ap_round_ms(stats[(int) (done * percs[i] / 100)].time));
}
}
if (csvperc) {
FILE *out = fopen(csvperc, "w");
if (!out) {
perror("Cannot open CSV output file");
exit(1);
}
fprintf(out, "" "Percentage served" "," "Time in ms" "\n");
for (i = 0; i <= 100; i++) {
double t;
if (i == 0)
t = ap_double_ms(stats[0].time);
else if (i == 100)
t = ap_double_ms(stats[done - 1].time);
else
t = ap_double_ms(stats[(int) (0.5 + done * i / 100.0)].time);
fprintf(out, "%d,%.3f\n", i, t);
}
fclose(out);
}
if (gnuplot) {
FILE *out = fopen(gnuplot, "w");
char tmstring[APR_CTIME_LEN];
if (!out) {
perror("Cannot open gnuplot output file");
exit(1);
}
fprintf(out, "starttime\tseconds\tctime\tdtime\tttime\twait\n");
for (i = 0; i < done; i++) {
(void) apr_ctime(tmstring, stats[i].starttime);
fprintf(out, "%s\t%" APR_TIME_T_FMT "\t%" APR_TIME_T_FMT
"\t%" APR_TIME_T_FMT "\t%" APR_TIME_T_FMT
"\t%" APR_TIME_T_FMT "\n", tmstring,
apr_time_sec(stats[i].starttime),
ap_round_ms(stats[i].ctime),
ap_round_ms(stats[i].time - stats[i].ctime),
ap_round_ms(stats[i].time),
ap_round_ms(stats[i].waittime));
}
fclose(out);
}
}
if (sig) {
exit(1);
}
}
static void output_html_results(void) {
double timetaken = (double) (lasttime - start) / APR_USEC_PER_SEC;
printf("\n\n<table %s>\n", tablestring);
printf("<tr %s><th colspan=2 %s>Server Software:</th>"
"<td colspan=2 %s>%s</td></tr>\n",
trstring, tdstring, tdstring, servername);
printf("<tr %s><th colspan=2 %s>Server Hostname:</th>"
"<td colspan=2 %s>%s</td></tr>\n",
trstring, tdstring, tdstring, hostname);
printf("<tr %s><th colspan=2 %s>Server Port:</th>"
"<td colspan=2 %s>%hu</td></tr>\n",
trstring, tdstring, tdstring, port);
printf("<tr %s><th colspan=2 %s>Document Path:</th>"
"<td colspan=2 %s>%s</td></tr>\n",
trstring, tdstring, tdstring, path);
if (nolength)
printf("<tr %s><th colspan=2 %s>Document Length:</th>"
"<td colspan=2 %s>Variable</td></tr>\n",
trstring, tdstring, tdstring);
else
printf("<tr %s><th colspan=2 %s>Document Length:</th>"
"<td colspan=2 %s>%" APR_SIZE_T_FMT " bytes</td></tr>\n",
trstring, tdstring, tdstring, doclen);
printf("<tr %s><th colspan=2 %s>Concurrency Level:</th>"
"<td colspan=2 %s>%d</td></tr>\n",
trstring, tdstring, tdstring, concurrency);
printf("<tr %s><th colspan=2 %s>Time taken for tests:</th>"
"<td colspan=2 %s>%.3f seconds</td></tr>\n",
trstring, tdstring, tdstring, timetaken);
printf("<tr %s><th colspan=2 %s>Complete requests:</th>"
"<td colspan=2 %s>%d</td></tr>\n",
trstring, tdstring, tdstring, done);
printf("<tr %s><th colspan=2 %s>Failed requests:</th>"
"<td colspan=2 %s>%d</td></tr>\n",
trstring, tdstring, tdstring, bad);
if (bad)
printf("<tr %s><td colspan=4 %s > (Connect: %d, Length: %d, Exceptions: %d)</td></tr>\n",
trstring, tdstring, err_conn, err_length, err_except);
if (err_response)
printf("<tr %s><th colspan=2 %s>Non-2xx responses:</th>"
"<td colspan=2 %s>%d</td></tr>\n",
trstring, tdstring, tdstring, err_response);
if (keepalive)
printf("<tr %s><th colspan=2 %s>Keep-Alive requests:</th>"
"<td colspan=2 %s>%d</td></tr>\n",
trstring, tdstring, tdstring, doneka);
printf("<tr %s><th colspan=2 %s>Total transferred:</th>"
"<td colspan=2 %s>%" APR_INT64_T_FMT " bytes</td></tr>\n",
trstring, tdstring, tdstring, totalread);
if (send_body)
printf("<tr %s><th colspan=2 %s>Total body sent:</th>"
"<td colspan=2 %s>%" APR_INT64_T_FMT "</td></tr>\n",
trstring, tdstring,
tdstring, totalposted);
printf("<tr %s><th colspan=2 %s>HTML transferred:</th>"
"<td colspan=2 %s>%" APR_INT64_T_FMT " bytes</td></tr>\n",
trstring, tdstring, tdstring, totalbread);
if (timetaken) {
printf("<tr %s><th colspan=2 %s>Requests per second:</th>"
"<td colspan=2 %s>%.2f</td></tr>\n",
trstring, tdstring, tdstring, (double) done / timetaken);
printf("<tr %s><th colspan=2 %s>Transfer rate:</th>"
"<td colspan=2 %s>%.2f kb/s received</td></tr>\n",
trstring, tdstring, tdstring, (double) totalread / 1024 / timetaken);
if (send_body) {
printf("<tr %s><td colspan=2 %s>&nbsp;</td>"
"<td colspan=2 %s>%.2f kb/s sent</td></tr>\n",
trstring, tdstring, tdstring,
(double) totalposted / 1024 / timetaken);
printf("<tr %s><td colspan=2 %s>&nbsp;</td>"
"<td colspan=2 %s>%.2f kb/s total</td></tr>\n",
trstring, tdstring, tdstring,
(double) (totalread + totalposted) / 1024 / timetaken);
}
}
{
int i;
apr_interval_time_t totalcon = 0, total = 0;
apr_interval_time_t mincon = AB_MAX, mintot = AB_MAX;
apr_interval_time_t maxcon = 0, maxtot = 0;
for (i = 0; i < done; i++) {
struct data *s = &stats[i];
mincon = ap_min(mincon, s->ctime);
mintot = ap_min(mintot, s->time);
maxcon = ap_max(maxcon, s->ctime);
maxtot = ap_max(maxtot, s->time);
totalcon += s->ctime;
total += s->time;
}
mincon = ap_round_ms(mincon);
mintot = ap_round_ms(mintot);
maxcon = ap_round_ms(maxcon);
maxtot = ap_round_ms(maxtot);
totalcon = ap_round_ms(totalcon);
total = ap_round_ms(total);
if (done > 0) {
printf("<tr %s><th %s colspan=4>Connnection Times (ms)</th></tr>\n",
trstring, tdstring);
printf("<tr %s><th %s>&nbsp;</th> <th %s>min</th> <th %s>avg</th> <th %s>max</th></tr>\n",
trstring, tdstring, tdstring, tdstring, tdstring);
printf("<tr %s><th %s>Connect:</th>"
"<td %s>%5" APR_TIME_T_FMT "</td>"
"<td %s>%5" APR_TIME_T_FMT "</td>"
"<td %s>%5" APR_TIME_T_FMT "</td></tr>\n",
trstring, tdstring, tdstring, mincon, tdstring, totalcon / done, tdstring, maxcon);
printf("<tr %s><th %s>Processing:</th>"
"<td %s>%5" APR_TIME_T_FMT "</td>"
"<td %s>%5" APR_TIME_T_FMT "</td>"
"<td %s>%5" APR_TIME_T_FMT "</td></tr>\n",
trstring, tdstring, tdstring, mintot - mincon, tdstring,
(total / done) - (totalcon / done), tdstring, maxtot - maxcon);
printf("<tr %s><th %s>Total:</th>"
"<td %s>%5" APR_TIME_T_FMT "</td>"
"<td %s>%5" APR_TIME_T_FMT "</td>"
"<td %s>%5" APR_TIME_T_FMT "</td></tr>\n",
trstring, tdstring, tdstring, mintot, tdstring, total / done, tdstring, maxtot);
}
printf("</table>\n");
}
}
static void start_connect(struct connection * c) {
apr_status_t rv;
if (!(started < requests))
return;
c->read = 0;
c->bread = 0;
c->keepalive = 0;
c->cbx = 0;
c->gotheader = 0;
c->rwrite = 0;
if (c->ctx)
apr_pool_clear(c->ctx);
else
apr_pool_create(&c->ctx, cntxt);
if ((rv = apr_socket_create(&c->aprsock, destsa->family,
SOCK_STREAM, 0, c->ctx)) != APR_SUCCESS) {
apr_err("socket", rv);
}
if (myhost) {
if ((rv = apr_socket_bind(c->aprsock, mysa)) != APR_SUCCESS) {
apr_err("bind", rv);
}
}
c->pollfd.desc_type = APR_POLL_SOCKET;
c->pollfd.desc.s = c->aprsock;
c->pollfd.reqevents = 0;
c->pollfd.client_data = c;
if ((rv = apr_socket_opt_set(c->aprsock, APR_SO_NONBLOCK, 1))
!= APR_SUCCESS) {
apr_err("socket nonblock", rv);
}
if (windowsize != 0) {
rv = apr_socket_opt_set(c->aprsock, APR_SO_SNDBUF,
windowsize);
if (rv != APR_SUCCESS && rv != APR_ENOTIMPL) {
apr_err("socket send buffer", rv);
}
rv = apr_socket_opt_set(c->aprsock, APR_SO_RCVBUF,
windowsize);
if (rv != APR_SUCCESS && rv != APR_ENOTIMPL) {
apr_err("socket receive buffer", rv);
}
}
c->start = lasttime = apr_time_now();
#if defined(USE_SSL)
if (is_ssl) {
BIO *bio;
apr_os_sock_t fd;
if ((c->ssl = SSL_new(ssl_ctx)) == NULL) {
BIO_printf(bio_err, "SSL_new failed.\n");
ERR_print_errors(bio_err);
exit(1);
}
ssl_rand_seed();
apr_os_sock_get(&fd, c->aprsock);
bio = BIO_new_socket(fd, BIO_NOCLOSE);
SSL_set_bio(c->ssl, bio, bio);
SSL_set_connect_state(c->ssl);
if (verbosity >= 4) {
BIO_set_callback(bio, ssl_print_cb);
BIO_set_callback_arg(bio, (void *)bio_err);
}
#if defined(HAVE_TLSEXT)
if (tls_sni) {
SSL_set_tlsext_host_name(c->ssl, tls_sni);
}
#endif
} else {
c->ssl = NULL;
}
#endif
if ((rv = apr_socket_connect(c->aprsock, destsa)) != APR_SUCCESS) {
if (APR_STATUS_IS_EINPROGRESS(rv)) {
set_conn_state(c, STATE_CONNECTING);
c->rwrite = 0;
return;
} else {
set_conn_state(c, STATE_UNCONNECTED);
apr_socket_close(c->aprsock);
err_conn++;
if (bad++ > 10) {
fprintf(stderr,
"\nTest aborted after 10 failures\n\n");
apr_err("apr_socket_connect()", rv);
}
start_connect(c);
return;
}
}
set_conn_state(c, STATE_CONNECTED);
#if defined(USE_SSL)
if (c->ssl) {
ssl_proceed_handshake(c);
} else
#endif
{
write_request(c);
}
}
static void close_connection(struct connection * c) {
if (c->read == 0 && c->keepalive) {
if (good)
good--;
} else {
if (good == 1) {
doclen = c->bread;
} else if ((c->bread != doclen) && !nolength) {
bad++;
err_length++;
}
if (done < requests) {
struct data *s = &stats[done++];
c->done = lasttime = apr_time_now();
s->starttime = c->start;
s->ctime = ap_max(0, c->connect - c->start);
s->time = ap_max(0, c->done - c->start);
s->waittime = ap_max(0, c->beginread - c->endwrite);
if (heartbeatres && !(done % heartbeatres)) {
fprintf(stderr, "Completed %d requests\n", done);
fflush(stderr);
}
}
}
set_conn_state(c, STATE_UNCONNECTED);
#if defined(USE_SSL)
if (c->ssl) {
SSL_shutdown(c->ssl);
SSL_free(c->ssl);
c->ssl = NULL;
}
#endif
apr_socket_close(c->aprsock);
start_connect(c);
return;
}
static void read_connection(struct connection * c) {
apr_size_t r;
apr_status_t status;
char *part;
char respcode[4];
r = sizeof(buffer);
#if defined(USE_SSL)
if (c->ssl) {
status = SSL_read(c->ssl, buffer, r);
if (status <= 0) {
int scode = SSL_get_error(c->ssl, status);
if (scode == SSL_ERROR_ZERO_RETURN) {
good++;
close_connection(c);
} else if (scode == SSL_ERROR_SYSCALL
&& status == 0
&& c->read != 0) {
good++;
close_connection(c);
} else if (scode != SSL_ERROR_WANT_WRITE
&& scode != SSL_ERROR_WANT_READ) {
c->read = 0;
BIO_printf(bio_err, "SSL read failed (%d) - closing connection\n", scode);
ERR_print_errors(bio_err);
close_connection(c);
}
return;
}
r = status;
} else
#endif
{
status = apr_socket_recv(c->aprsock, buffer, &r);
if (APR_STATUS_IS_EAGAIN(status))
return;
else if (r == 0 && APR_STATUS_IS_EOF(status)) {
good++;
close_connection(c);
return;
} else if (status != APR_SUCCESS) {
err_recv++;
if (recverrok) {
bad++;
close_connection(c);
if (verbosity >= 1) {
char buf[120];
fprintf(stderr,"%s: %s (%d)\n", "apr_socket_recv", apr_strerror(status, buf, sizeof buf), status);
}
return;
} else {
apr_err("apr_socket_recv", status);
}
}
}
totalread += r;
if (c->read == 0) {
c->beginread = apr_time_now();
}
c->read += r;
if (!c->gotheader) {
char *s;
int l = 4;
apr_size_t space = CBUFFSIZE - c->cbx - 1;
int tocopy = (space < r) ? space : r;
#if defined(NOT_ASCII)
apr_size_t inbytes_left = space, outbytes_left = space;
status = apr_xlate_conv_buffer(from_ascii, buffer, &inbytes_left,
c->cbuff + c->cbx, &outbytes_left);
if (status || inbytes_left || outbytes_left) {
fprintf(stderr, "only simple translation is supported (%d/%" APR_SIZE_T_FMT
"/%" APR_SIZE_T_FMT ")\n", status, inbytes_left, outbytes_left);
exit(1);
}
#else
memcpy(c->cbuff + c->cbx, buffer, space);
#endif
c->cbx += tocopy;
space -= tocopy;
c->cbuff[c->cbx] = 0;
if (verbosity >= 2) {
printf("LOG: header received:\n%s\n", c->cbuff);
}
s = strstr(c->cbuff, "\r\n\r\n");
if (!s) {
s = strstr(c->cbuff, "\n\n");
l = 2;
}
if (!s) {
if (space) {
return;
} else {
set_conn_state(c, STATE_UNCONNECTED);
apr_socket_close(c->aprsock);
err_response++;
if (bad++ > 10) {
err("\nTest aborted after 10 failures\n\n");
}
start_connect(c);
}
} else {
if (!good) {
char *p, *q;
size_t len = 0;
p = xstrcasestr(c->cbuff, "Server:");
q = servername;
if (p) {
p += 8;
while (*p > 32 && len++ < sizeof(servername) - 1)
*q++ = *p++;
}
*q = 0;
}
part = strstr(c->cbuff, "HTTP");
if (part && strlen(part) > strlen("HTTP/1.x_")) {
strncpy(respcode, (part + strlen("HTTP/1.x_")), 3);
respcode[3] = '\0';
} else {
strcpy(respcode, "500");
}
if (respcode[0] != '2') {
err_response++;
if (verbosity >= 2)
printf("WARNING: Response code not 2xx (%s)\n", respcode);
} else if (verbosity >= 3) {
printf("LOG: Response code = %s\n", respcode);
}
c->gotheader = 1;
*s = 0;
if (keepalive && xstrcasestr(c->cbuff, "Keep-Alive")) {
char *cl;
c->keepalive = 1;
cl = xstrcasestr(c->cbuff, "Content-Length:");
if (cl && method != HEAD) {
c->length = atoi(cl + 16);
} else {
c->length = 0;
}
}
c->bread += c->cbx - (s + l - c->cbuff) + r - tocopy;
totalbread += c->bread;
}
} else {
c->bread += r;
totalbread += r;
}
if (c->keepalive && (c->bread >= c->length)) {
good++;
if (good == 1) {
doclen = c->bread;
} else if ((c->bread != doclen) && !nolength) {
bad++;
err_length++;
}
if (done < requests) {
struct data *s = &stats[done++];
doneka++;
c->done = apr_time_now();
s->starttime = c->start;
s->ctime = ap_max(0, c->connect - c->start);
s->time = ap_max(0, c->done - c->start);
s->waittime = ap_max(0, c->beginread - c->endwrite);
if (heartbeatres && !(done % heartbeatres)) {
fprintf(stderr, "Completed %d requests\n", done);
fflush(stderr);
}
}
c->keepalive = 0;
c->length = 0;
c->gotheader = 0;
c->cbx = 0;
c->read = c->bread = 0;
c->start = c->connect = lasttime = apr_time_now();
write_request(c);
}
}
static void test(void) {
apr_time_t stoptime;
apr_int16_t rtnev;
apr_status_t rv;
int i;
apr_status_t status;
int snprintf_res = 0;
#if defined(NOT_ASCII)
apr_size_t inbytes_left, outbytes_left;
#endif
if (isproxy) {
connecthost = apr_pstrdup(cntxt, proxyhost);
connectport = proxyport;
} else {
connecthost = apr_pstrdup(cntxt, hostname);
connectport = port;
}
if (!use_html) {
printf("Benchmarking %s ", hostname);
if (isproxy)
printf("[through %s:%d] ", proxyhost, proxyport);
printf("(be patient)%s",
(heartbeatres ? "\n" : "..."));
fflush(stdout);
}
con = xcalloc(concurrency, sizeof(struct connection));
stats = xcalloc(requests, sizeof(struct data));
if ((status = apr_pollset_create(&readbits, concurrency, cntxt,
APR_POLLSET_NOCOPY)) != APR_SUCCESS) {
apr_err("apr_pollset_create failed", status);
}
if (!opt_host) {
hdrs = apr_pstrcat(cntxt, hdrs, "Host: ", host_field, colonhost, "\r\n", NULL);
} else {
}
#if defined(HAVE_TLSEXT)
if (is_ssl && tls_use_sni) {
apr_ipsubnet_t *ip;
if (((tls_sni = opt_host) || (tls_sni = hostname)) &&
(!*tls_sni || apr_ipsubnet_create(&ip, tls_sni, NULL,
cntxt) == APR_SUCCESS)) {
tls_sni = NULL;
}
}
#endif
if (!opt_useragent) {
hdrs = apr_pstrcat(cntxt, hdrs, "User-Agent: ApacheBench/", AP_AB_BASEREVISION, "\r\n", NULL);
} else {
}
if (!opt_accept) {
hdrs = apr_pstrcat(cntxt, hdrs, "Accept: */*\r\n", NULL);
} else {
}
if (!send_body) {
snprintf_res = apr_snprintf(request, sizeof(_request),
"%s %s HTTP/1.0\r\n"
"%s" "%s" "%s"
"%s" "\r\n",
method_str[method],
(isproxy) ? fullurl : path,
keepalive ? "Connection: Keep-Alive\r\n" : "",
cookie, auth, hdrs);
} else {
snprintf_res = apr_snprintf(request, sizeof(_request),
"%s %s HTTP/1.0\r\n"
"%s" "%s" "%s"
"Content-length: %" APR_SIZE_T_FMT "\r\n"
"Content-type: %s\r\n"
"%s"
"\r\n",
method_str[method],
(isproxy) ? fullurl : path,
keepalive ? "Connection: Keep-Alive\r\n" : "",
cookie, auth,
postlen,
(content_type != NULL) ? content_type : "text/plain", hdrs);
}
if (snprintf_res >= sizeof(_request)) {
err("Request too long\n");
}
if (verbosity >= 2)
printf("INFO: %s header == \n---\n%s\n---\n",
method_str[method], request);
reqlen = strlen(request);
if (send_body) {
char *buff = xmalloc(postlen + reqlen + 1);
strcpy(buff, request);
memcpy(buff + reqlen, postdata, postlen);
request = buff;
}
#if defined(NOT_ASCII)
inbytes_left = outbytes_left = reqlen;
status = apr_xlate_conv_buffer(to_ascii, request, &inbytes_left,
request, &outbytes_left);
if (status || inbytes_left || outbytes_left) {
fprintf(stderr, "only simple translation is supported (%d/%"
APR_SIZE_T_FMT "/%" APR_SIZE_T_FMT ")\n",
status, inbytes_left, outbytes_left);
exit(1);
}
#endif
if (myhost) {
if ((rv = apr_sockaddr_info_get(&mysa, myhost, APR_UNSPEC, 0, 0, cntxt)) != APR_SUCCESS) {
char buf[120];
apr_snprintf(buf, sizeof(buf),
"apr_sockaddr_info_get() for %s", myhost);
apr_err(buf, rv);
}
}
if ((rv = apr_sockaddr_info_get(&destsa, connecthost,
myhost ? mysa->family : APR_UNSPEC,
connectport, 0, cntxt))
!= APR_SUCCESS) {
char buf[120];
apr_snprintf(buf, sizeof(buf),
"apr_sockaddr_info_get() for %s", connecthost);
apr_err(buf, rv);
}
start = lasttime = apr_time_now();
stoptime = tlimit ? (start + apr_time_from_sec(tlimit)) : AB_MAX;
#if defined(SIGINT)
apr_signal(SIGINT, output_results);
#endif
for (i = 0; i < concurrency; i++) {
con[i].socknum = i;
start_connect(&con[i]);
}
do {
apr_int32_t n;
const apr_pollfd_t *pollresults, *pollfd;
n = concurrency;
do {
status = apr_pollset_poll(readbits, aprtimeout, &n, &pollresults);
} while (APR_STATUS_IS_EINTR(status));
if (status != APR_SUCCESS)
apr_err("apr_pollset_poll", status);
for (i = 0, pollfd = pollresults; i < n; i++, pollfd++) {
struct connection *c;
c = pollfd->client_data;
if (c->state == STATE_UNCONNECTED)
continue;
rtnev = pollfd->rtnevents;
#if defined(USE_SSL)
if (c->state == STATE_CONNECTED && c->ssl && SSL_in_init(c->ssl)) {
ssl_proceed_handshake(c);
continue;
}
#endif
if ((rtnev & APR_POLLIN) || (rtnev & APR_POLLPRI) || (rtnev & APR_POLLHUP))
read_connection(c);
if ((rtnev & APR_POLLERR) || (rtnev & APR_POLLNVAL)) {
bad++;
err_except++;
if (c->state == STATE_CONNECTING) {
read_connection(c);
} else {
start_connect(c);
}
continue;
}
if (rtnev & APR_POLLOUT) {
if (c->state == STATE_CONNECTING) {
rv = apr_socket_connect(c->aprsock, destsa);
if (rv != APR_SUCCESS) {
set_conn_state(c, STATE_UNCONNECTED);
apr_socket_close(c->aprsock);
err_conn++;
if (bad++ > 10) {
fprintf(stderr,
"\nTest aborted after 10 failures\n\n");
apr_err("apr_socket_connect()", rv);
}
start_connect(c);
continue;
} else {
set_conn_state(c, STATE_CONNECTED);
#if defined(USE_SSL)
if (c->ssl)
ssl_proceed_handshake(c);
else
#endif
write_request(c);
}
} else {
write_request(c);
}
}
}
} while (lasttime < stoptime && done < requests);
if (heartbeatres)
fprintf(stderr, "Finished %d requests\n", done);
else
printf("..done\n");
if (use_html)
output_html_results();
else
output_results(0);
}
static void copyright(void) {
if (!use_html) {
printf("This is ApacheBench, Version %s\n", AP_AB_BASEREVISION " <$Revision: 1807734 $>");
printf("Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/\n");
printf("Licensed to The Apache Software Foundation, http://www.apache.org/\n");
printf("\n");
} else {
printf("<p>\n");
printf(" This is ApacheBench, Version %s <i>&lt;%s&gt;</i><br>\n", AP_AB_BASEREVISION, "$Revision: 1807734 $");
printf(" Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/<br>\n");
printf(" Licensed to The Apache Software Foundation, http://www.apache.org/<br>\n");
printf("</p>\n<p>\n");
}
}
static void usage(const char *progname) {
fprintf(stderr, "Usage: %s [options] [http"
#if defined(USE_SSL)
"[s]"
#endif
"://]hostname[:port]/path\n", progname);
fprintf(stderr, "Options are:\n");
fprintf(stderr, " -n requests Number of requests to perform\n");
fprintf(stderr, " -c concurrency Number of multiple requests to make at a time\n");
fprintf(stderr, " -t timelimit Seconds to max. to spend on benchmarking\n");
fprintf(stderr, " This implies -n 50000\n");
fprintf(stderr, " -s timeout Seconds to max. wait for each response\n");
fprintf(stderr, " Default is 30 seconds\n");
fprintf(stderr, " -b windowsize Size of TCP send/receive buffer, in bytes\n");
fprintf(stderr, " -B address Address to bind to when making outgoing connections\n");
fprintf(stderr, " -p postfile File containing data to POST. Remember also to set -T\n");
fprintf(stderr, " -u putfile File containing data to PUT. Remember also to set -T\n");
fprintf(stderr, " -T content-type Content-type header to use for POST/PUT data, eg.\n");
fprintf(stderr, " 'application/x-www-form-urlencoded'\n");
fprintf(stderr, " Default is 'text/plain'\n");
fprintf(stderr, " -v verbosity How much troubleshooting info to print\n");
fprintf(stderr, " -w Print out results in HTML tables\n");
fprintf(stderr, " -i Use HEAD instead of GET\n");
fprintf(stderr, " -x attributes String to insert as table attributes\n");
fprintf(stderr, " -y attributes String to insert as tr attributes\n");
fprintf(stderr, " -z attributes String to insert as td or th attributes\n");
fprintf(stderr, " -C attribute Add cookie, eg. 'Apache=1234'. (repeatable)\n");
fprintf(stderr, " -H attribute Add Arbitrary header line, eg. 'Accept-Encoding: gzip'\n");
fprintf(stderr, " Inserted after all normal header lines. (repeatable)\n");
fprintf(stderr, " -A attribute Add Basic WWW Authentication, the attributes\n");
fprintf(stderr, " are a colon separated username and password.\n");
fprintf(stderr, " -P attribute Add Basic Proxy Authentication, the attributes\n");
fprintf(stderr, " are a colon separated username and password.\n");
fprintf(stderr, " -X proxy:port Proxyserver and port number to use\n");
fprintf(stderr, " -V Print version number and exit\n");
fprintf(stderr, " -k Use HTTP KeepAlive feature\n");
fprintf(stderr, " -d Do not show percentiles served table.\n");
fprintf(stderr, " -S Do not show confidence estimators and warnings.\n");
fprintf(stderr, " -q Do not show progress when doing more than 150 requests\n");
fprintf(stderr, " -l Accept variable document length (use this for dynamic pages)\n");
fprintf(stderr, " -g filename Output collected data to gnuplot format file.\n");
fprintf(stderr, " -e filename Output CSV file with percentages served\n");
fprintf(stderr, " -r Don't exit on socket receive errors.\n");
fprintf(stderr, " -m method Method name\n");
fprintf(stderr, " -h Display usage information (this message)\n");
#if defined(USE_SSL)
#if !defined(OPENSSL_NO_SSL2)
#define SSL2_HELP_MSG "SSL2, "
#else
#define SSL2_HELP_MSG ""
#endif
#if !defined(OPENSSL_NO_SSL3)
#define SSL3_HELP_MSG "SSL3, "
#else
#define SSL3_HELP_MSG ""
#endif
#if defined(HAVE_TLSV1_X)
#define TLS1_X_HELP_MSG ", TLS1.1, TLS1.2"
#else
#define TLS1_X_HELP_MSG ""
#endif
#if defined(HAVE_TLSEXT)
fprintf(stderr, " -I Disable TLS Server Name Indication (SNI) extension\n");
#endif
fprintf(stderr, " -Z ciphersuite Specify SSL/TLS cipher suite (See openssl ciphers)\n");
fprintf(stderr, " -f protocol Specify SSL/TLS protocol\n");
fprintf(stderr, " (" SSL2_HELP_MSG SSL3_HELP_MSG "TLS1" TLS1_X_HELP_MSG " or ALL)\n");
#endif
exit(EINVAL);
}
static int parse_url(const char *url) {
char *cp;
char *h;
char *scope_id;
apr_status_t rv;
fullurl = apr_pstrdup(cntxt, url);
if (strlen(url) > 7 && strncmp(url, "http://", 7) == 0) {
url += 7;
#if defined(USE_SSL)
is_ssl = 0;
#endif
} else
#if defined(USE_SSL)
if (strlen(url) > 8 && strncmp(url, "https://", 8) == 0) {
url += 8;
is_ssl = 1;
}
#else
if (strlen(url) > 8 && strncmp(url, "https://", 8) == 0) {
fprintf(stderr, "SSL not compiled in; no https support\n");
exit(1);
}
#endif
if ((cp = strchr(url, '/')) == NULL)
return 1;
h = apr_pstrmemdup(cntxt, url, cp - url);
rv = apr_parse_addr_port(&hostname, &scope_id, &port, h, cntxt);
if (rv != APR_SUCCESS || !hostname || scope_id) {
return 1;
}
path = apr_pstrdup(cntxt, cp);
*cp = '\0';
if (*url == '[') {
host_field = apr_psprintf(cntxt, "[%s]", hostname);
} else {
host_field = hostname;
}
if (port == 0) {
#if defined(USE_SSL)
if (is_ssl)
port = 443;
else
#endif
port = 80;
}
if ((
#if defined(USE_SSL)
is_ssl && (port != 443)) || (!is_ssl &&
#endif
(port != 80))) {
colonhost = apr_psprintf(cntxt,":%d",port);
} else
colonhost = "";
return 0;
}
static apr_status_t open_postfile(const char *pfile) {
apr_file_t *postfd;
apr_finfo_t finfo;
apr_status_t rv;
char errmsg[120];
rv = apr_file_open(&postfd, pfile, APR_READ, APR_OS_DEFAULT, cntxt);
if (rv != APR_SUCCESS) {
fprintf(stderr, "ab: Could not open POST data file (%s): %s\n", pfile,
apr_strerror(rv, errmsg, sizeof errmsg));
return rv;
}
rv = apr_file_info_get(&finfo, APR_FINFO_NORM, postfd);
if (rv != APR_SUCCESS) {
fprintf(stderr, "ab: Could not stat POST data file (%s): %s\n", pfile,
apr_strerror(rv, errmsg, sizeof errmsg));
return rv;
}
postlen = (apr_size_t)finfo.size;
postdata = xmalloc(postlen);
rv = apr_file_read_full(postfd, postdata, postlen, NULL);
if (rv != APR_SUCCESS) {
fprintf(stderr, "ab: Could not read POST data file: %s\n",
apr_strerror(rv, errmsg, sizeof errmsg));
return rv;
}
apr_file_close(postfd);
return APR_SUCCESS;
}
int main(int argc, const char * const argv[]) {
int l;
char tmp[1024];
apr_status_t status;
apr_getopt_t *opt;
const char *opt_arg;
char c;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
int max_prot = TLS1_2_VERSION;
#if !defined(OPENSSL_NO_SSL3)
int min_prot = SSL3_VERSION;
#else
int min_prot = TLS1_VERSION;
#endif
#endif
#if defined(USE_SSL)
AB_SSL_METHOD_CONST SSL_METHOD *meth = SSLv23_client_method();
#endif
tablestring = "";
trstring = "";
tdstring = "bgcolor=white";
cookie = "";
auth = "";
proxyhost = "";
hdrs = "";
apr_app_initialize(&argc, &argv, NULL);
atexit(apr_terminate);
apr_pool_create(&cntxt, NULL);
apr_pool_abort_set(abort_on_oom, cntxt);
#if defined(NOT_ASCII)
status = apr_xlate_open(&to_ascii, "ISO-8859-1", APR_DEFAULT_CHARSET, cntxt);
if (status) {
fprintf(stderr, "apr_xlate_open(to ASCII)->%d\n", status);
exit(1);
}
status = apr_xlate_open(&from_ascii, APR_DEFAULT_CHARSET, "ISO-8859-1", cntxt);
if (status) {
fprintf(stderr, "apr_xlate_open(from ASCII)->%d\n", status);
exit(1);
}
status = apr_base64init_ebcdic(to_ascii, from_ascii);
if (status) {
fprintf(stderr, "apr_base64init_ebcdic()->%d\n", status);
exit(1);
}
#endif
myhost = NULL;
apr_getopt_init(&opt, cntxt, argc, argv);
while ((status = apr_getopt(opt, "n:c:t:s:b:T:p:u:v:lrkVhwiIx:y:z:C:H:P:A:g:X:de:SqB:m:"
#if defined(USE_SSL)
"Z:f:"
#endif
,&c, &opt_arg)) == APR_SUCCESS) {
switch (c) {
case 'n':
requests = atoi(opt_arg);
if (requests <= 0) {
err("Invalid number of requests\n");
}
break;
case 'k':
keepalive = 1;
break;
case 'q':
heartbeatres = 0;
break;
case 'c':
concurrency = atoi(opt_arg);
break;
case 'b':
windowsize = atoi(opt_arg);
break;
case 'i':
if (method != NO_METH)
err("Cannot mix HEAD with other methods\n");
method = HEAD;
break;
case 'g':
gnuplot = xstrdup(opt_arg);
break;
case 'd':
percentile = 0;
break;
case 'e':
csvperc = xstrdup(opt_arg);
break;
case 'S':
confidence = 0;
break;
case 's':
aprtimeout = apr_time_from_sec(atoi(opt_arg));
break;
case 'p':
if (method != NO_METH)
err("Cannot mix POST with other methods\n");
if (open_postfile(opt_arg) != APR_SUCCESS) {
exit(1);
}
method = POST;
send_body = 1;
break;
case 'u':
if (method != NO_METH)
err("Cannot mix PUT with other methods\n");
if (open_postfile(opt_arg) != APR_SUCCESS) {
exit(1);
}
method = PUT;
send_body = 1;
break;
case 'l':
nolength = 1;
break;
case 'r':
recverrok = 1;
break;
case 'v':
verbosity = atoi(opt_arg);
break;
case 't':
tlimit = atoi(opt_arg);
requests = MAX_REQUESTS;
break;
case 'T':
content_type = apr_pstrdup(cntxt, opt_arg);
break;
case 'C':
cookie = apr_pstrcat(cntxt, "Cookie: ", opt_arg, "\r\n", NULL);
break;
case 'A':
while (apr_isspace(*opt_arg))
opt_arg++;
if (apr_base64_encode_len(strlen(opt_arg)) > sizeof(tmp)) {
err("Authentication credentials too long\n");
}
l = apr_base64_encode(tmp, opt_arg, strlen(opt_arg));
tmp[l] = '\0';
auth = apr_pstrcat(cntxt, auth, "Authorization: Basic ", tmp,
"\r\n", NULL);
break;
case 'P':
while (apr_isspace(*opt_arg))
opt_arg++;
if (apr_base64_encode_len(strlen(opt_arg)) > sizeof(tmp)) {
err("Proxy credentials too long\n");
}
l = apr_base64_encode(tmp, opt_arg, strlen(opt_arg));
tmp[l] = '\0';
auth = apr_pstrcat(cntxt, auth, "Proxy-Authorization: Basic ",
tmp, "\r\n", NULL);
break;
case 'H':
hdrs = apr_pstrcat(cntxt, hdrs, opt_arg, "\r\n", NULL);
if (strncasecmp(opt_arg, "Host:", 5) == 0) {
char *host;
apr_size_t len;
opt_arg += 5;
while (apr_isspace(*opt_arg))
opt_arg++;
len = strlen(opt_arg);
host = strdup(opt_arg);
while (len && apr_isspace(host[len-1]))
host[--len] = '\0';
opt_host = host;
} else if (strncasecmp(opt_arg, "Accept:", 7) == 0) {
opt_accept = 1;
} else if (strncasecmp(opt_arg, "User-Agent:", 11) == 0) {
opt_useragent = 1;
}
break;
case 'w':
use_html = 1;
break;
case 'x':
use_html = 1;
tablestring = opt_arg;
break;
case 'X': {
char *p;
if ((p = strchr(opt_arg, ':'))) {
*p = '\0';
p++;
proxyport = atoi(p);
}
proxyhost = apr_pstrdup(cntxt, opt_arg);
isproxy = 1;
}
break;
case 'y':
use_html = 1;
trstring = opt_arg;
break;
case 'z':
use_html = 1;
tdstring = opt_arg;
break;
case 'h':
usage(argv[0]);
break;
case 'V':
copyright();
return 0;
case 'B':
myhost = apr_pstrdup(cntxt, opt_arg);
break;
case 'm':
method = CUSTOM_METHOD;
method_str[CUSTOM_METHOD] = strdup(opt_arg);
break;
#if defined(USE_SSL)
case 'Z':
ssl_cipher = strdup(opt_arg);
break;
case 'f':
#if OPENSSL_VERSION_NUMBER < 0x10100000L
if (strncasecmp(opt_arg, "ALL", 3) == 0) {
meth = SSLv23_client_method();
#if !defined(OPENSSL_NO_SSL2)
} else if (strncasecmp(opt_arg, "SSL2", 4) == 0) {
meth = SSLv2_client_method();
#if defined(HAVE_TLSEXT)
tls_use_sni = 0;
#endif
#endif
#if !defined(OPENSSL_NO_SSL3)
} else if (strncasecmp(opt_arg, "SSL3", 4) == 0) {
meth = SSLv3_client_method();
#if defined(HAVE_TLSEXT)
tls_use_sni = 0;
#endif
#endif
#if defined(HAVE_TLSV1_X)
} else if (strncasecmp(opt_arg, "TLS1.1", 6) == 0) {
meth = TLSv1_1_client_method();
} else if (strncasecmp(opt_arg, "TLS1.2", 6) == 0) {
meth = TLSv1_2_client_method();
#endif
} else if (strncasecmp(opt_arg, "TLS1", 4) == 0) {
meth = TLSv1_client_method();
}
#else
meth = TLS_client_method();
if (strncasecmp(opt_arg, "ALL", 3) == 0) {
max_prot = TLS1_2_VERSION;
#if !defined(OPENSSL_NO_SSL3)
min_prot = SSL3_VERSION;
#else
min_prot = TLS1_VERSION;
#endif
#if !defined(OPENSSL_NO_SSL3)
} else if (strncasecmp(opt_arg, "SSL3", 4) == 0) {
max_prot = SSL3_VERSION;
min_prot = SSL3_VERSION;
#endif
} else if (strncasecmp(opt_arg, "TLS1.1", 6) == 0) {
max_prot = TLS1_1_VERSION;
min_prot = TLS1_1_VERSION;
} else if (strncasecmp(opt_arg, "TLS1.2", 6) == 0) {
max_prot = TLS1_2_VERSION;
min_prot = TLS1_2_VERSION;
} else if (strncasecmp(opt_arg, "TLS1", 4) == 0) {
max_prot = TLS1_VERSION;
min_prot = TLS1_VERSION;
}
#endif
break;
#if defined(HAVE_TLSEXT)
case 'I':
tls_use_sni = 0;
break;
#endif
#endif
}
}
if (opt->ind != argc - 1) {
fprintf(stderr, "%s: wrong number of arguments\n", argv[0]);
usage(argv[0]);
}
if (method == NO_METH) {
method = GET;
}
if (parse_url(apr_pstrdup(cntxt, opt->argv[opt->ind++]))) {
fprintf(stderr, "%s: invalid URL\n", argv[0]);
usage(argv[0]);
}
if ((concurrency < 0) || (concurrency > MAX_CONCURRENCY)) {
fprintf(stderr, "%s: Invalid Concurrency [Range 0..%d]\n",
argv[0], MAX_CONCURRENCY);
usage(argv[0]);
}
if (concurrency > requests) {
fprintf(stderr, "%s: Cannot use concurrency level greater than "
"total number of requests\n", argv[0]);
usage(argv[0]);
}
if ((heartbeatres) && (requests > 150)) {
heartbeatres = requests / 10;
if (heartbeatres < 100)
heartbeatres = 100;
} else
heartbeatres = 0;
#if defined(USE_SSL)
#if defined(RSAREF)
R_malloc_init();
#else
#if OPENSSL_VERSION_NUMBER < 0x10100000L
CRYPTO_malloc_init();
#endif
#endif
SSL_load_error_strings();
SSL_library_init();
bio_out=BIO_new_fp(stdout,BIO_NOCLOSE);
bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);
if (!(ssl_ctx = SSL_CTX_new(meth))) {
BIO_printf(bio_err, "Could not initialize SSL Context.\n");
ERR_print_errors(bio_err);
exit(1);
}
SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
SSL_CTX_set_max_proto_version(ssl_ctx, max_prot);
SSL_CTX_set_min_proto_version(ssl_ctx, min_prot);
#endif
#if defined(SSL_MODE_RELEASE_BUFFERS)
SSL_CTX_set_mode (ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
#endif
if (ssl_cipher != NULL) {
if (!SSL_CTX_set_cipher_list(ssl_ctx, ssl_cipher)) {
fprintf(stderr, "error setting cipher list [%s]\n", ssl_cipher);
ERR_print_errors_fp(stderr);
exit(1);
}
}
if (verbosity >= 3) {
SSL_CTX_set_info_callback(ssl_ctx, ssl_state_cb);
}
#endif
#if defined(SIGPIPE)
apr_signal(SIGPIPE, SIG_IGN);
#endif
copyright();
test();
apr_pool_destroy(cntxt);
return 0;
}
