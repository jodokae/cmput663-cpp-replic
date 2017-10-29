#include "apr_strings.h"
#include "apr_lib.h"
#include "apr_hash.h"
#include "apr_optional.h"
#include "apr_anylock.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "ap_config.h"
#include "mod_log_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "util_time.h"
#include "ap_mpm.h"
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(HAVE_LIMITS_H)
#include <limits.h>
#endif
#define DEFAULT_LOG_FORMAT "%h %l %u %t \"%r\" %>s %b"
module AP_MODULE_DECLARE_DATA log_config_module;
static int xfer_flags = (APR_WRITE | APR_APPEND | APR_CREATE | APR_LARGEFILE);
static apr_fileperms_t xfer_perms = APR_OS_DEFAULT;
static apr_hash_t *log_hash;
static apr_status_t ap_default_log_writer(request_rec *r,
void *handle,
const char **strs,
int *strl,
int nelts,
apr_size_t len);
static apr_status_t ap_buffered_log_writer(request_rec *r,
void *handle,
const char **strs,
int *strl,
int nelts,
apr_size_t len);
static void *ap_default_log_writer_init(apr_pool_t *p, server_rec *s,
const char* name);
static void *ap_buffered_log_writer_init(apr_pool_t *p, server_rec *s,
const char* name);
static ap_log_writer_init *ap_log_set_writer_init(ap_log_writer_init *handle);
static ap_log_writer *ap_log_set_writer(ap_log_writer *handle);
static ap_log_writer *log_writer = ap_default_log_writer;
static ap_log_writer_init *log_writer_init = ap_default_log_writer_init;
static int buffered_logs = 0;
static apr_array_header_t *all_buffered_logs = NULL;
#if defined(PIPE_BUF)
#define LOG_BUFSIZE PIPE_BUF
#else
#define LOG_BUFSIZE (512)
#endif
typedef struct {
const char *default_format_string;
apr_array_header_t *default_format;
apr_array_header_t *config_logs;
apr_array_header_t *server_config_logs;
apr_table_t *formats;
} multi_log_state;
typedef struct {
apr_file_t *handle;
apr_size_t outcnt;
char outbuf[LOG_BUFSIZE];
apr_anylock_t mutex;
} buffered_log;
typedef struct {
const char *fname;
const char *format_string;
apr_array_header_t *format;
void *log_writer;
char *condition_var;
int inherit;
ap_expr_info_t *condition_expr;
const ap_directive_t *directive;
} config_log_state;
typedef struct {
apr_time_t request_end_time;
} log_request_state;
typedef struct {
ap_log_handler_fn_t *func;
char *arg;
int condition_sense;
int want_orig;
apr_array_header_t *conditions;
} log_format_item;
static char *pfmt(apr_pool_t *p, int i) {
if (i <= 0) {
return "-";
} else {
return apr_itoa(p, i);
}
}
static const char *constant_item(request_rec *dummy, char *stuff) {
return stuff;
}
static const char *log_remote_host(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, ap_get_remote_host(r->connection,
r->per_dir_config,
REMOTE_NAME, NULL));
}
static const char *log_remote_address(request_rec *r, char *a) {
if (a && !strcmp(a, "c")) {
return r->connection->client_ip;
} else {
return r->useragent_ip;
}
}
static const char *log_local_address(request_rec *r, char *a) {
return r->connection->local_ip;
}
static const char *log_remote_logname(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, ap_get_remote_logname(r));
}
static const char *log_remote_user(request_rec *r, char *a) {
char *rvalue = r->user;
if (rvalue == NULL) {
rvalue = "-";
} else if (strlen(rvalue) == 0) {
rvalue = "\"\"";
} else {
rvalue = ap_escape_logitem(r->pool, rvalue);
}
return rvalue;
}
static const char *log_request_line(request_rec *r, char *a) {
return ap_escape_logitem(r->pool,
(r->parsed_uri.password)
? apr_pstrcat(r->pool, r->method, " ",
apr_uri_unparse(r->pool,
&r->parsed_uri, 0),
r->assbackwards ? NULL : " ",
r->protocol, NULL)
: r->the_request);
}
static const char *log_request_file(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, r->filename);
}
static const char *log_request_uri(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, r->uri);
}
static const char *log_request_method(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, r->method);
}
static const char *log_log_id(request_rec *r, char *a) {
if (a && !strcmp(a, "c")) {
return r->connection->log_id ? r->connection->log_id : "-";
} else {
return r->log_id ? r->log_id : "-";
}
}
static const char *log_request_protocol(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, r->protocol);
}
static const char *log_request_query(request_rec *r, char *a) {
return (r->args) ? apr_pstrcat(r->pool, "?",
ap_escape_logitem(r->pool, r->args), NULL)
: "";
}
static const char *log_status(request_rec *r, char *a) {
return pfmt(r->pool, r->status);
}
static const char *log_handler(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, r->handler);
}
static const char *clf_log_bytes_sent(request_rec *r, char *a) {
if (!r->sent_bodyct || !r->bytes_sent) {
return "-";
} else {
return apr_off_t_toa(r->pool, r->bytes_sent);
}
}
static const char *log_bytes_sent(request_rec *r, char *a) {
if (!r->sent_bodyct || !r->bytes_sent) {
return "0";
} else {
return apr_off_t_toa(r->pool, r->bytes_sent);
}
}
static const char *log_header_in(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, apr_table_get(r->headers_in, a));
}
static const char *log_trailer_in(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, apr_table_get(r->trailers_in, a));
}
static APR_INLINE char *find_multiple_headers(apr_pool_t *pool,
const apr_table_t *table,
const char *key) {
const apr_array_header_t *elts;
const apr_table_entry_t *t_elt;
const apr_table_entry_t *t_end;
apr_size_t len;
struct sle {
struct sle *next;
const char *value;
apr_size_t len;
} *result_list, *rp;
elts = apr_table_elts(table);
if (!elts->nelts) {
return NULL;
}
t_elt = (const apr_table_entry_t *)elts->elts;
t_end = t_elt + elts->nelts;
len = 1;
result_list = rp = NULL;
do {
if (!strcasecmp(t_elt->key, key)) {
if (!result_list) {
result_list = rp = apr_palloc(pool, sizeof(*rp));
} else {
rp = rp->next = apr_palloc(pool, sizeof(*rp));
len += 2;
}
rp->next = NULL;
rp->value = t_elt->val;
rp->len = strlen(rp->value);
len += rp->len;
}
++t_elt;
} while (t_elt < t_end);
if (result_list) {
char *result = apr_palloc(pool, len);
char *cp = result;
rp = result_list;
while (rp) {
if (rp != result_list) {
*cp++ = ',';
*cp++ = ' ';
}
memcpy(cp, rp->value, rp->len);
cp += rp->len;
rp = rp->next;
}
*cp = '\0';
return result;
}
return NULL;
}
static const char *log_header_out(request_rec *r, char *a) {
const char *cp = NULL;
if (!strcasecmp(a, "Content-type") && r->content_type) {
cp = ap_field_noparam(r->pool, r->content_type);
} else if (!strcasecmp(a, "Set-Cookie")) {
cp = find_multiple_headers(r->pool, r->headers_out, a);
} else {
cp = apr_table_get(r->headers_out, a);
}
return ap_escape_logitem(r->pool, cp);
}
static const char *log_trailer_out(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, apr_table_get(r->trailers_out, a));
}
static const char *log_note(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, apr_table_get(r->notes, a));
}
static const char *log_env_var(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, apr_table_get(r->subprocess_env, a));
}
static const char *log_cookie(request_rec *r, char *a) {
const char *cookies_entry;
if ((cookies_entry = apr_table_get(r->headers_in, "Cookie"))) {
char *cookie, *last1, *last2;
char *cookies = apr_pstrdup(r->pool, cookies_entry);
while ((cookie = apr_strtok(cookies, ";", &last1))) {
char *name = apr_strtok(cookie, "=", &last2);
char *value = last2;
if (name && *name && value && *value) {
char *last = value - 2;
name += strspn(name, " \t");
while (last >= name && apr_isspace(*last)) {
*last = '\0';
--last;
}
if (!strcasecmp(name, a)) {
last = last1 - (*last1 ? 2 : 1);
value += strspn(value, " \t");
while (last >= value && apr_isspace(*last)) {
*last = '\0';
--last;
}
return ap_escape_logitem(r->pool, value);
}
}
cookies = NULL;
}
}
return NULL;
}
static const char *log_request_time_custom(request_rec *r, char *a,
apr_time_exp_t *xt) {
apr_size_t retcode;
char tstr[MAX_STRING_LEN];
apr_strftime(tstr, &retcode, sizeof(tstr), a, xt);
return apr_pstrdup(r->pool, tstr);
}
#define DEFAULT_REQUEST_TIME_SIZE 32
typedef struct {
unsigned t;
char timestr[DEFAULT_REQUEST_TIME_SIZE];
unsigned t_validate;
} cached_request_time;
#define TIME_FMT_CUSTOM 0
#define TIME_FMT_CLF 1
#define TIME_FMT_ABS_SEC 2
#define TIME_FMT_ABS_MSEC 3
#define TIME_FMT_ABS_USEC 4
#define TIME_FMT_ABS_MSEC_FRAC 5
#define TIME_FMT_ABS_USEC_FRAC 6
#define TIME_CACHE_SIZE 4
#define TIME_CACHE_MASK 3
static cached_request_time request_time_cache[TIME_CACHE_SIZE];
static apr_time_t get_request_end_time(request_rec *r) {
log_request_state *state = (log_request_state *)ap_get_module_config(r->request_config,
&log_config_module);
if (!state) {
state = apr_pcalloc(r->pool, sizeof(log_request_state));
ap_set_module_config(r->request_config, &log_config_module, state);
}
if (state->request_end_time == 0) {
state->request_end_time = apr_time_now();
}
return state->request_end_time;
}
static const char *log_request_time(request_rec *r, char *a) {
apr_time_exp_t xt;
apr_time_t request_time = r->request_time;
int fmt_type = TIME_FMT_CUSTOM;
char *fmt = a;
if (fmt && *fmt) {
if (!strncmp(fmt, "begin", 5)) {
fmt += 5;
if (!*fmt) {
fmt_type = TIME_FMT_CLF;
} else if (*fmt == ':') {
fmt++;
a = fmt;
}
} else if (!strncmp(fmt, "end", 3)) {
fmt += 3;
if (!*fmt) {
request_time = get_request_end_time(r);
fmt_type = TIME_FMT_CLF;
} else if (*fmt == ':') {
fmt++;
a = fmt;
request_time = get_request_end_time(r);
}
}
if (!strncmp(fmt, "msec", 4)) {
fmt += 4;
if (!*fmt) {
fmt_type = TIME_FMT_ABS_MSEC;
} else if (!strcmp(fmt, "_frac")) {
fmt_type = TIME_FMT_ABS_MSEC_FRAC;
}
} else if (!strncmp(fmt, "usec", 4)) {
fmt += 4;
if (!*fmt) {
fmt_type = TIME_FMT_ABS_USEC;
} else if (!strcmp(fmt, "_frac")) {
fmt_type = TIME_FMT_ABS_USEC_FRAC;
}
} else if (!strcmp(fmt, "sec")) {
fmt_type = TIME_FMT_ABS_SEC;
} else if (!*fmt) {
fmt_type = TIME_FMT_CLF;
}
} else {
fmt_type = TIME_FMT_CLF;
}
if (fmt_type >= TIME_FMT_ABS_SEC) {
char* buf = apr_palloc(r->pool, 20);
switch (fmt_type) {
case TIME_FMT_ABS_SEC:
apr_snprintf(buf, 20, "%" APR_TIME_T_FMT, apr_time_sec(request_time));
break;
case TIME_FMT_ABS_MSEC:
apr_snprintf(buf, 20, "%" APR_TIME_T_FMT, apr_time_as_msec(request_time));
break;
case TIME_FMT_ABS_USEC:
apr_snprintf(buf, 20, "%" APR_TIME_T_FMT, request_time);
break;
case TIME_FMT_ABS_MSEC_FRAC:
apr_snprintf(buf, 20, "%03" APR_TIME_T_FMT, apr_time_msec(request_time));
break;
case TIME_FMT_ABS_USEC_FRAC:
apr_snprintf(buf, 20, "%06" APR_TIME_T_FMT, apr_time_usec(request_time));
break;
default:
return "-";
}
return buf;
} else if (fmt_type == TIME_FMT_CUSTOM) {
ap_explode_recent_localtime(&xt, request_time);
return log_request_time_custom(r, a, &xt);
} else {
cached_request_time* cached_time = apr_palloc(r->pool,
sizeof(*cached_time));
unsigned t_seconds = (unsigned)apr_time_sec(request_time);
unsigned i = t_seconds & TIME_CACHE_MASK;
*cached_time = request_time_cache[i];
if ((t_seconds != cached_time->t) ||
(t_seconds != cached_time->t_validate)) {
char sign;
int timz;
ap_explode_recent_localtime(&xt, request_time);
timz = xt.tm_gmtoff;
if (timz < 0) {
timz = -timz;
sign = '-';
} else {
sign = '+';
}
cached_time->t = t_seconds;
apr_snprintf(cached_time->timestr, DEFAULT_REQUEST_TIME_SIZE,
"[%02d/%s/%d:%02d:%02d:%02d %c%.2d%.2d]",
xt.tm_mday, apr_month_snames[xt.tm_mon],
xt.tm_year+1900, xt.tm_hour, xt.tm_min, xt.tm_sec,
sign, timz / (60*60), (timz % (60*60)) / 60);
cached_time->t_validate = t_seconds;
request_time_cache[i] = *cached_time;
}
return cached_time->timestr;
}
}
static const char *log_request_duration_microseconds(request_rec *r, char *a) {
return apr_psprintf(r->pool, "%" APR_TIME_T_FMT,
(get_request_end_time(r) - r->request_time));
}
static const char *log_request_duration_scaled(request_rec *r, char *a) {
apr_time_t duration = get_request_end_time(r) - r->request_time;
if (*a == '\0' || !strcasecmp(a, "s")) {
duration = apr_time_sec(duration);
} else if (!strcasecmp(a, "ms")) {
duration = apr_time_as_msec(duration);
} else if (!strcasecmp(a, "us")) {
} else {
return a;
}
return apr_psprintf(r->pool, "%" APR_TIME_T_FMT, duration);
}
static const char *log_virtual_host(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, r->server->server_hostname);
}
static const char *log_server_port(request_rec *r, char *a) {
apr_port_t port;
if (*a == '\0' || !strcasecmp(a, "canonical")) {
port = r->server->port ? r->server->port : ap_default_port(r);
} else if (!strcasecmp(a, "remote")) {
port = r->useragent_addr->port;
} else if (!strcasecmp(a, "local")) {
port = r->connection->local_addr->port;
} else {
return a;
}
return apr_itoa(r->pool, (int)port);
}
static const char *log_server_name(request_rec *r, char *a) {
return ap_escape_logitem(r->pool, ap_get_server_name(r));
}
static const char *log_pid_tid(request_rec *r, char *a) {
if (*a == '\0' || !strcasecmp(a, "pid")) {
return ap_append_pid(r->pool, "", "");
} else if (!strcasecmp(a, "tid") || !strcasecmp(a, "hextid")) {
#if APR_HAS_THREADS
apr_os_thread_t tid = apr_os_thread_current();
#else
int tid = 0;
#endif
return apr_psprintf(r->pool,
#if APR_MAJOR_VERSION > 1 || (APR_MAJOR_VERSION == 1 && APR_MINOR_VERSION >= 2)
*a == 'h' ? "%pt" : "%pT",
#else
"%pT",
#endif
&tid);
}
return a;
}
static const char *log_connection_status(request_rec *r, char *a) {
if (r->connection->aborted)
return "X";
if (r->connection->keepalive == AP_CONN_KEEPALIVE &&
(!r->server->keep_alive_max ||
(r->server->keep_alive_max - r->connection->keepalives) > 0)) {
return "+";
}
return "-";
}
static const char *log_requests_on_connection(request_rec *r, char *a) {
int num = r->connection->keepalives ? r->connection->keepalives - 1 : 0;
return apr_itoa(r->pool, num);
}
static char *parse_log_misc_string(apr_pool_t *p, log_format_item *it,
const char **sa) {
const char *s;
char *d;
it->func = constant_item;
it->conditions = NULL;
s = *sa;
while (*s && *s != '%') {
s++;
}
it->arg = apr_palloc(p, s - *sa + 1);
d = it->arg;
s = *sa;
while (*s && *s != '%') {
if (*s != '\\') {
*d++ = *s++;
} else {
s++;
switch (*s) {
case '\\':
*d++ = '\\';
s++;
break;
case 'r':
*d++ = '\r';
s++;
break;
case 'n':
*d++ = '\n';
s++;
break;
case 't':
*d++ = '\t';
s++;
break;
default:
*d++ = '\\';
break;
}
}
}
*d = '\0';
*sa = s;
return NULL;
}
static char *parse_log_item(apr_pool_t *p, log_format_item *it, const char **sa) {
const char *s = *sa;
ap_log_handler *handler = NULL;
if (*s != '%') {
return parse_log_misc_string(p, it, sa);
}
++s;
it->condition_sense = 0;
it->conditions = NULL;
if (*s == '%') {
it->arg = "%";
it->func = constant_item;
*sa = ++s;
return NULL;
}
it->want_orig = -1;
it->arg = "";
while (*s) {
int i;
switch (*s) {
case '!':
++s;
it->condition_sense = !it->condition_sense;
break;
case '<':
++s;
it->want_orig = 1;
break;
case '>':
++s;
it->want_orig = 0;
break;
case ',':
++s;
break;
case '{':
++s;
it->arg = ap_getword(p, &s, '}');
break;
case '0':
case '1':
case '2':
case '3':
case '4':
case '5':
case '6':
case '7':
case '8':
case '9':
i = *s - '0';
while (apr_isdigit(*++s)) {
i = i * 10 + (*s) - '0';
}
if (!it->conditions) {
it->conditions = apr_array_make(p, 4, sizeof(int));
}
*(int *) apr_array_push(it->conditions) = i;
break;
default:
if (*s == '^' && *(s+1) && *(s+2)) {
handler = (ap_log_handler *)apr_hash_get(log_hash, s, 3);
if (handler) {
s += 3;
}
}
if (!handler) {
handler = (ap_log_handler *)apr_hash_get(log_hash, s++, 1);
}
if (!handler) {
char dummy[2];
dummy[0] = s[-1];
dummy[1] = '\0';
return apr_pstrcat(p, "Unrecognized LogFormat directive %",
dummy, NULL);
}
it->func = handler->func;
if (it->want_orig == -1) {
it->want_orig = handler->want_orig_default;
}
*sa = s;
return NULL;
}
}
return "Ran off end of LogFormat parsing args to some directive";
}
static apr_array_header_t *parse_log_string(apr_pool_t *p, const char *s, const char **err) {
apr_array_header_t *a = apr_array_make(p, 30, sizeof(log_format_item));
char *res;
while (*s) {
if ((res = parse_log_item(p, (log_format_item *) apr_array_push(a), &s))) {
*err = res;
return NULL;
}
}
s = APR_EOL_STR;
parse_log_item(p, (log_format_item *) apr_array_push(a), &s);
return a;
}
static const char *process_item(request_rec *r, request_rec *orig,
log_format_item *item) {
const char *cp;
if (item->conditions && item->conditions->nelts != 0) {
int i;
int *conds = (int *) item->conditions->elts;
int in_list = 0;
for (i = 0; i < item->conditions->nelts; ++i) {
if (r->status == conds[i]) {
in_list = 1;
break;
}
}
if ((item->condition_sense && in_list)
|| (!item->condition_sense && !in_list)) {
return "-";
}
}
cp = (*item->func) (item->want_orig ? orig : r, item->arg);
return cp ? cp : "-";
}
static void flush_log(buffered_log *buf) {
if (buf->outcnt && buf->handle != NULL) {
apr_file_write(buf->handle, buf->outbuf, &buf->outcnt);
buf->outcnt = 0;
}
}
static int config_log_transaction(request_rec *r, config_log_state *cls,
apr_array_header_t *default_format) {
log_format_item *items;
const char **strs;
int *strl;
request_rec *orig;
int i;
apr_size_t len = 0;
apr_array_header_t *format;
char *envar;
apr_status_t rv;
if (cls->fname == NULL) {
return DECLINED;
}
if (cls->condition_var != NULL) {
envar = cls->condition_var;
if (*envar != '!') {
if (apr_table_get(r->subprocess_env, envar) == NULL) {
return DECLINED;
}
} else {
if (apr_table_get(r->subprocess_env, &envar[1]) != NULL) {
return DECLINED;
}
}
} else if (cls->condition_expr != NULL) {
const char *err;
int rc = ap_expr_exec(r, cls->condition_expr, &err);
if (rc < 0)
ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, APLOGNO(00644)
"Error evaluating log condition: %s", err);
if (rc <= 0)
return DECLINED;
}
format = cls->format ? cls->format : default_format;
strs = apr_palloc(r->pool, sizeof(char *) * (format->nelts));
strl = apr_palloc(r->pool, sizeof(int) * (format->nelts));
items = (log_format_item *) format->elts;
orig = r;
while (orig->prev) {
orig = orig->prev;
}
while (r->next) {
r = r->next;
}
for (i = 0; i < format->nelts; ++i) {
strs[i] = process_item(r, orig, &items[i]);
}
for (i = 0; i < format->nelts; ++i) {
len += strl[i] = strlen(strs[i]);
}
if (!log_writer) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00645)
"log writer isn't correctly setup");
return HTTP_INTERNAL_SERVER_ERROR;
}
rv = log_writer(r, cls->log_writer, strs, strl, format->nelts, len);
if (rv != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_WARNING, rv, r, APLOGNO(00646)
"Error writing to %s", cls->fname);
}
return OK;
}
static int multi_log_transaction(request_rec *r) {
multi_log_state *mls = ap_get_module_config(r->server->module_config,
&log_config_module);
config_log_state *clsarray;
int i;
log_request_state *state = apr_pcalloc(r->pool, sizeof(log_request_state));
ap_set_module_config(r->request_config, &log_config_module, state);
if (mls->config_logs->nelts) {
clsarray = (config_log_state *) mls->config_logs->elts;
for (i = 0; i < mls->config_logs->nelts; ++i) {
config_log_state *cls = &clsarray[i];
config_log_transaction(r, cls, mls->default_format);
}
}
if (mls->server_config_logs) {
clsarray = (config_log_state *) mls->server_config_logs->elts;
for (i = 0; i < mls->server_config_logs->nelts; ++i) {
config_log_state *cls = &clsarray[i];
if (cls->inherit || !mls->config_logs->nelts) {
config_log_transaction(r, cls, mls->default_format);
}
}
}
return OK;
}
static void *make_config_log_state(apr_pool_t *p, server_rec *s) {
multi_log_state *mls;
mls = (multi_log_state *) apr_palloc(p, sizeof(multi_log_state));
mls->config_logs = apr_array_make(p, 1, sizeof(config_log_state));
mls->default_format_string = NULL;
mls->default_format = NULL;
mls->server_config_logs = NULL;
mls->formats = apr_table_make(p, 4);
apr_table_setn(mls->formats, "CLF", DEFAULT_LOG_FORMAT);
return mls;
}
static void *merge_config_log_state(apr_pool_t *p, void *basev, void *addv) {
multi_log_state *base = (multi_log_state *) basev;
multi_log_state *add = (multi_log_state *) addv;
add->server_config_logs = base->config_logs;
if (!add->default_format) {
add->default_format_string = base->default_format_string;
add->default_format = base->default_format;
}
add->formats = apr_table_overlay(p, base->formats, add->formats);
return add;
}
static const char *log_format(cmd_parms *cmd, void *dummy, const char *fmt,
const char *name) {
const char *err_string = NULL;
multi_log_state *mls = ap_get_module_config(cmd->server->module_config,
&log_config_module);
if (name != NULL) {
parse_log_string(cmd->pool, fmt, &err_string);
if (err_string == NULL) {
apr_table_setn(mls->formats, name, fmt);
}
} else {
mls->default_format_string = fmt;
mls->default_format = parse_log_string(cmd->pool, fmt, &err_string);
}
return err_string;
}
static const char *add_custom_log(cmd_parms *cmd, void *dummy, const char *fn,
const char *fmt, const char *envclause) {
const char *err_string = NULL;
multi_log_state *mls = ap_get_module_config(cmd->server->module_config,
&log_config_module);
config_log_state *cls;
cls = (config_log_state *) apr_array_push(mls->config_logs);
cls->condition_var = NULL;
cls->condition_expr = NULL;
if (envclause != NULL) {
if (strncasecmp(envclause, "env=", 4) == 0) {
if ((envclause[4] == '\0')
|| ((envclause[4] == '!') && (envclause[5] == '\0'))) {
return "missing environment variable name";
}
cls->condition_var = apr_pstrdup(cmd->pool, &envclause[4]);
} else if (strncasecmp(envclause, "expr=", 5) == 0) {
const char *err;
if ((envclause[5] == '\0'))
return "missing condition";
cls->condition_expr = ap_expr_parse_cmd(cmd, &envclause[5],
AP_EXPR_FLAG_DONT_VARY,
&err, NULL);
if (err)
return err;
} else {
return "error in condition clause";
}
}
cls->fname = fn;
cls->format_string = fmt;
cls->directive = cmd->directive;
if (fmt == NULL) {
cls->format = NULL;
} else {
cls->format = parse_log_string(cmd->pool, fmt, &err_string);
}
cls->log_writer = NULL;
return err_string;
}
static const char *add_global_log(cmd_parms *cmd, void *dummy, const char *fn,
const char *fmt, const char *envclause) {
multi_log_state *mls = ap_get_module_config(cmd->server->module_config,
&log_config_module);
config_log_state *clsarray;
config_log_state *cls;
const char *ret;
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err) {
return err;
}
ret = add_custom_log(cmd, dummy, fn, fmt, envclause);
if (ret == NULL) {
clsarray = (config_log_state*)mls->config_logs->elts;
cls = &clsarray[mls->config_logs->nelts-1];
cls->inherit = 1;
}
return ret;
}
static const char *set_transfer_log(cmd_parms *cmd, void *dummy,
const char *fn) {
return add_custom_log(cmd, dummy, fn, NULL, NULL);
}
static const char *set_buffered_logs_on(cmd_parms *parms, void *dummy, int flag) {
buffered_logs = flag;
if (buffered_logs) {
ap_log_set_writer_init(ap_buffered_log_writer_init);
ap_log_set_writer(ap_buffered_log_writer);
} else {
ap_log_set_writer_init(ap_default_log_writer_init);
ap_log_set_writer(ap_default_log_writer);
}
return NULL;
}
static const command_rec config_log_cmds[] = {
AP_INIT_TAKE23("CustomLog", add_custom_log, NULL, RSRC_CONF,
"a file name, a custom log format string or format name, "
"and an optional \"env=\" or \"expr=\" clause (see docs)"),
AP_INIT_TAKE23("GlobalLog", add_global_log, NULL, RSRC_CONF,
"Same as CustomLog, but forces virtualhosts to inherit the log"),
AP_INIT_TAKE1("TransferLog", set_transfer_log, NULL, RSRC_CONF,
"the filename of the access log"),
AP_INIT_TAKE12("LogFormat", log_format, NULL, RSRC_CONF,
"a log format string (see docs) and an optional format name"),
AP_INIT_FLAG("BufferedLogs", set_buffered_logs_on, NULL, RSRC_CONF,
"Enable Buffered Logging (experimental)"),
{NULL}
};
static config_log_state *open_config_log(server_rec *s, apr_pool_t *p,
config_log_state *cls,
apr_array_header_t *default_format) {
if (cls->log_writer != NULL) {
return cls;
}
if (cls->fname == NULL) {
return cls;
}
cls->log_writer = log_writer_init(p, s, cls->fname);
if (cls->log_writer == NULL)
return NULL;
return cls;
}
static int open_multi_logs(server_rec *s, apr_pool_t *p) {
int i;
multi_log_state *mls = ap_get_module_config(s->module_config,
&log_config_module);
config_log_state *clsarray;
const char *dummy;
const char *format;
if (mls->default_format_string) {
format = apr_table_get(mls->formats, mls->default_format_string);
if (format) {
mls->default_format = parse_log_string(p, format, &dummy);
}
}
if (!mls->default_format) {
mls->default_format = parse_log_string(p, DEFAULT_LOG_FORMAT, &dummy);
}
if (mls->config_logs->nelts) {
clsarray = (config_log_state *) mls->config_logs->elts;
for (i = 0; i < mls->config_logs->nelts; ++i) {
config_log_state *cls = &clsarray[i];
if (cls->format_string) {
format = apr_table_get(mls->formats, cls->format_string);
if (format) {
cls->format = parse_log_string(p, format, &dummy);
}
}
if (!open_config_log(s, p, cls, mls->default_format)) {
return DONE;
}
}
} else if (mls->server_config_logs) {
clsarray = (config_log_state *) mls->server_config_logs->elts;
for (i = 0; i < mls->server_config_logs->nelts; ++i) {
config_log_state *cls = &clsarray[i];
if (cls->format_string) {
format = apr_table_get(mls->formats, cls->format_string);
if (format) {
cls->format = parse_log_string(p, format, &dummy);
}
}
if (!open_config_log(s, p, cls, mls->default_format)) {
return DONE;
}
}
}
return OK;
}
static apr_status_t flush_all_logs(void *data) {
server_rec *s = data;
multi_log_state *mls;
apr_array_header_t *log_list;
config_log_state *clsarray;
buffered_log *buf;
int i;
if (!buffered_logs)
return APR_SUCCESS;
for (; s; s = s->next) {
mls = ap_get_module_config(s->module_config, &log_config_module);
log_list = NULL;
if (mls->config_logs->nelts) {
log_list = mls->config_logs;
} else if (mls->server_config_logs) {
log_list = mls->server_config_logs;
}
if (log_list) {
clsarray = (config_log_state *) log_list->elts;
for (i = 0; i < log_list->nelts; ++i) {
buf = clsarray[i].log_writer;
flush_log(buf);
}
}
}
return APR_SUCCESS;
}
static int init_config_log(apr_pool_t *pc, apr_pool_t *p, apr_pool_t *pt, server_rec *s) {
int res;
if (buffered_logs) {
all_buffered_logs = apr_array_make(p, 5, sizeof(buffered_log *));
}
res = open_multi_logs(s, p);
for (s = s->next; (res == OK) && s; s = s->next) {
res = open_multi_logs(s, p);
}
return res;
}
static void init_child(apr_pool_t *p, server_rec *s) {
int mpm_threads;
ap_mpm_query(AP_MPMQ_MAX_THREADS, &mpm_threads);
if (buffered_logs) {
int i;
buffered_log **array = (buffered_log **)all_buffered_logs->elts;
apr_pool_cleanup_register(p, s, flush_all_logs, flush_all_logs);
for (i = 0; i < all_buffered_logs->nelts; i++) {
buffered_log *this = array[i];
#if APR_HAS_THREADS
if (mpm_threads > 1) {
apr_status_t rv;
this->mutex.type = apr_anylock_threadmutex;
rv = apr_thread_mutex_create(&this->mutex.lock.tm,
APR_THREAD_MUTEX_DEFAULT,
p);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, s, APLOGNO(00647)
"could not initialize buffered log mutex, "
"transfer log may become corrupted");
this->mutex.type = apr_anylock_none;
}
} else
#endif
{
this->mutex.type = apr_anylock_none;
}
}
}
}
static void ap_register_log_handler(apr_pool_t *p, char *tag,
ap_log_handler_fn_t *handler, int def) {
ap_log_handler *log_struct = apr_palloc(p, sizeof(*log_struct));
log_struct->func = handler;
log_struct->want_orig_default = def;
apr_hash_set(log_hash, tag, strlen(tag), (const void *)log_struct);
}
static ap_log_writer_init *ap_log_set_writer_init(ap_log_writer_init *handle) {
ap_log_writer_init *old = log_writer_init;
log_writer_init = handle;
return old;
}
static ap_log_writer *ap_log_set_writer(ap_log_writer *handle) {
ap_log_writer *old = log_writer;
log_writer = handle;
return old;
}
static apr_status_t ap_default_log_writer( request_rec *r,
void *handle,
const char **strs,
int *strl,
int nelts,
apr_size_t len) {
char *str;
char *s;
int i;
apr_status_t rv;
str = apr_palloc(r->pool, len + 1);
for (i = 0, s = str; i < nelts; ++i) {
memcpy(s, strs[i], strl[i]);
s += strl[i];
}
rv = apr_file_write((apr_file_t*)handle, str, &len);
return rv;
}
static void *ap_default_log_writer_init(apr_pool_t *p, server_rec *s,
const char* name) {
if (*name == '|') {
piped_log *pl;
pl = ap_open_piped_log(p, name + 1);
if (pl == NULL) {
return NULL;
}
return ap_piped_log_write_fd(pl);
} else {
const char *fname = ap_server_root_relative(p, name);
apr_file_t *fd;
apr_status_t rv;
if (!fname) {
ap_log_error(APLOG_MARK, APLOG_ERR, APR_EBADPATH, s, APLOGNO(00648)
"invalid transfer log path %s.", name);
return NULL;
}
rv = apr_file_open(&fd, fname, xfer_flags, xfer_perms, p);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, s, APLOGNO(00649)
"could not open transfer log file %s.", fname);
return NULL;
}
return fd;
}
}
static void *ap_buffered_log_writer_init(apr_pool_t *p, server_rec *s,
const char* name) {
buffered_log *b;
b = apr_pcalloc(p, sizeof(buffered_log));
b->handle = ap_default_log_writer_init(p, s, name);
if (b->handle) {
*(buffered_log **)apr_array_push(all_buffered_logs) = b;
return b;
} else
return NULL;
}
static apr_status_t ap_buffered_log_writer(request_rec *r,
void *handle,
const char **strs,
int *strl,
int nelts,
apr_size_t len) {
char *str;
char *s;
int i;
apr_status_t rv;
buffered_log *buf = (buffered_log*)handle;
if ((rv = APR_ANYLOCK_LOCK(&buf->mutex)) != APR_SUCCESS) {
return rv;
}
if (len + buf->outcnt > LOG_BUFSIZE) {
flush_log(buf);
}
if (len >= LOG_BUFSIZE) {
apr_size_t w;
str = apr_palloc(r->pool, len + 1);
for (i = 0, s = str; i < nelts; ++i) {
memcpy(s, strs[i], strl[i]);
s += strl[i];
}
w = len;
rv = apr_file_write(buf->handle, str, &w);
} else {
for (i = 0, s = &buf->outbuf[buf->outcnt]; i < nelts; ++i) {
memcpy(s, strs[i], strl[i]);
s += strl[i];
}
buf->outcnt += len;
rv = APR_SUCCESS;
}
APR_ANYLOCK_UNLOCK(&buf->mutex);
return rv;
}
static int log_pre_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp) {
static APR_OPTIONAL_FN_TYPE(ap_register_log_handler) *log_pfn_register;
log_pfn_register = APR_RETRIEVE_OPTIONAL_FN(ap_register_log_handler);
if (log_pfn_register) {
log_pfn_register(p, "h", log_remote_host, 0);
log_pfn_register(p, "a", log_remote_address, 0 );
log_pfn_register(p, "A", log_local_address, 0 );
log_pfn_register(p, "l", log_remote_logname, 0);
log_pfn_register(p, "u", log_remote_user, 0);
log_pfn_register(p, "t", log_request_time, 0);
log_pfn_register(p, "f", log_request_file, 0);
log_pfn_register(p, "b", clf_log_bytes_sent, 0);
log_pfn_register(p, "B", log_bytes_sent, 0);
log_pfn_register(p, "i", log_header_in, 0);
log_pfn_register(p, "o", log_header_out, 0);
log_pfn_register(p, "n", log_note, 0);
log_pfn_register(p, "L", log_log_id, 1);
log_pfn_register(p, "e", log_env_var, 0);
log_pfn_register(p, "V", log_server_name, 0);
log_pfn_register(p, "v", log_virtual_host, 0);
log_pfn_register(p, "p", log_server_port, 0);
log_pfn_register(p, "P", log_pid_tid, 0);
log_pfn_register(p, "H", log_request_protocol, 0);
log_pfn_register(p, "m", log_request_method, 0);
log_pfn_register(p, "q", log_request_query, 0);
log_pfn_register(p, "X", log_connection_status, 0);
log_pfn_register(p, "C", log_cookie, 0);
log_pfn_register(p, "k", log_requests_on_connection, 0);
log_pfn_register(p, "r", log_request_line, 1);
log_pfn_register(p, "D", log_request_duration_microseconds, 1);
log_pfn_register(p, "T", log_request_duration_scaled, 1);
log_pfn_register(p, "U", log_request_uri, 1);
log_pfn_register(p, "s", log_status, 1);
log_pfn_register(p, "R", log_handler, 1);
log_pfn_register(p, "^ti", log_trailer_in, 0);
log_pfn_register(p, "^to", log_trailer_out, 0);
}
ap_log_set_writer_init(ap_default_log_writer_init);
ap_log_set_writer(ap_default_log_writer);
buffered_logs = 0;
return OK;
}
static int check_log_dir(apr_pool_t *p, server_rec *s, config_log_state *cls) {
if (!cls->fname || cls->fname[0] == '|' || !cls->directive) {
return OK;
} else {
char *abs = ap_server_root_relative(p, cls->fname);
char *dir = ap_make_dirstr_parent(p, abs);
apr_finfo_t finfo;
const ap_directive_t *directive = cls->directive;
apr_status_t rv = apr_stat(&finfo, dir, APR_FINFO_TYPE, p);
cls->directive = NULL;
if (rv == APR_SUCCESS && finfo.filetype != APR_DIR)
rv = APR_ENOTDIR;
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_STARTUP|APLOG_EMERG, rv, s,
APLOGNO(02297)
"Cannot access directory '%s' for log file '%s' "
"defined at %s:%d", dir, cls->fname,
directive->filename, directive->line_num);
return !OK;
}
}
return OK;
}
static int log_check_config(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s) {
int rv = OK;
while (s) {
multi_log_state *mls = ap_get_module_config(s->module_config,
&log_config_module);
apr_array_header_t *log_list = mls->config_logs;
config_log_state *clsarray = (config_log_state *) log_list->elts;
int i;
for (i = 0; i < log_list->nelts; ++i) {
if (check_log_dir(ptemp, s, &clsarray[i]) != OK)
rv = !OK;
}
s = s->next;
}
return rv;
}
static void register_hooks(apr_pool_t *p) {
ap_hook_pre_config(log_pre_config,NULL,NULL,APR_HOOK_REALLY_FIRST);
ap_hook_check_config(log_check_config,NULL,NULL,APR_HOOK_MIDDLE);
ap_hook_child_init(init_child,NULL,NULL,APR_HOOK_MIDDLE);
ap_hook_open_logs(init_config_log,NULL,NULL,APR_HOOK_MIDDLE);
ap_hook_log_transaction(multi_log_transaction,NULL,NULL,APR_HOOK_MIDDLE);
log_hash = apr_hash_make(p);
APR_REGISTER_OPTIONAL_FN(ap_register_log_handler);
APR_REGISTER_OPTIONAL_FN(ap_log_set_writer_init);
APR_REGISTER_OPTIONAL_FN(ap_log_set_writer);
}
AP_DECLARE_MODULE(log_config) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
make_config_log_state,
merge_config_log_state,
config_log_cmds,
register_hooks
};