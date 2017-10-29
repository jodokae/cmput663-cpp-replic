#include "apr.h"
#include "apr_strings.h"
#include "apr_lib.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_vhost.h"
#include "http_protocol.h"
#include "http_core.h"
#if APR_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#undef APLOG_MODULE_INDEX
#define APLOG_MODULE_INDEX AP_CORE_MODULE_INDEX
typedef struct name_chain name_chain;
struct name_chain {
name_chain *next;
server_addr_rec *sar;
server_rec *server;
};
typedef struct ipaddr_chain ipaddr_chain;
struct ipaddr_chain {
ipaddr_chain *next;
server_addr_rec *sar;
server_rec *server;
name_chain *names;
name_chain *initialnames;
};
#if !defined(IPHASH_TABLE_SIZE)
#define IPHASH_TABLE_SIZE 256
#endif
static ipaddr_chain *iphash_table[IPHASH_TABLE_SIZE];
static ipaddr_chain *default_list;
static int config_error = 0;
static int vhost_check_config(apr_pool_t *p, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s);
AP_DECLARE(void) ap_init_vhost_config(apr_pool_t *p) {
memset(iphash_table, 0, sizeof(iphash_table));
default_list = NULL;
ap_hook_check_config(vhost_check_config, NULL, NULL, APR_HOOK_MIDDLE);
}
static const char *get_addresses(apr_pool_t *p, const char *w_,
server_addr_rec ***paddr,
apr_port_t default_port) {
apr_sockaddr_t *my_addr;
server_addr_rec *sar;
char *w, *host, *scope_id;
int wild_port;
apr_size_t wlen;
apr_port_t port;
apr_status_t rv;
if (*w_ == '\0')
return NULL;
wlen = strlen(w_);
w = apr_pstrmemdup(p, w_, wlen);
wild_port = 0;
if (w[wlen - 1] == '*') {
if (wlen < 2) {
wild_port = 1;
} else if (w[wlen - 2] == ':') {
w[wlen - 2] = '\0';
wild_port = 1;
}
}
rv = apr_parse_addr_port(&host, &scope_id, &port, w, p);
if (rv != APR_SUCCESS) {
return "The address or port is invalid";
}
if (!host) {
return "Missing address for VirtualHost";
}
if (scope_id) {
return "Scope ids are not supported";
}
if (!port && !wild_port) {
port = default_port;
}
if (strcmp(host, "*") == 0 || strcasecmp(host, "_default_") == 0) {
rv = apr_sockaddr_info_get(&my_addr, NULL, APR_UNSPEC, port, 0, p);
if (rv) {
return "Could not determine a wildcard address ('0.0.0.0') -- "
"check resolver configuration.";
}
} else {
rv = apr_sockaddr_info_get(&my_addr, host, APR_UNSPEC, port, 0, p);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, NULL, APLOGNO(00547)
"Could not resolve host name %s -- ignoring!", host);
return NULL;
}
}
do {
sar = apr_pcalloc(p, sizeof(server_addr_rec));
**paddr = sar;
*paddr = &sar->next;
sar->host_addr = my_addr;
sar->host_port = port;
sar->virthost = host;
my_addr = my_addr->next;
} while (my_addr);
return NULL;
}
const char *ap_parse_vhost_addrs(apr_pool_t *p,
const char *hostname,
server_rec *s) {
server_addr_rec **addrs;
const char *err;
addrs = &s->addrs;
while (hostname[0]) {
err = get_addresses(p, ap_getword_conf(p, &hostname), &addrs, s->port);
if (err) {
*addrs = NULL;
return err;
}
}
*addrs = NULL;
if (s->addrs) {
if (s->addrs->host_port) {
s->port = s->addrs->host_port;
}
}
return NULL;
}
AP_DECLARE_NONSTD(const char *)ap_set_name_virtual_host(cmd_parms *cmd,
void *dummy,
const char *arg) {
static int warnonce = 0;
if (++warnonce == 1) {
ap_log_error(APLOG_MARK, APLOG_NOTICE|APLOG_STARTUP, APR_SUCCESS, NULL, APLOGNO(00548)
"NameVirtualHost has no effect and will be removed in the "
"next release %s:%d",
cmd->directive->filename,
cmd->directive->line_num);
}
return NULL;
}
#if defined(IPHASH_STATISTICS)
static int iphash_compare(const void *a, const void *b) {
return (*(const int *) b - *(const int *) a);
}
static void dump_iphash_statistics(server_rec *main_s) {
unsigned count[IPHASH_TABLE_SIZE];
int i;
ipaddr_chain *src;
unsigned total;
char buf[HUGE_STRING_LEN];
char *p;
total = 0;
for (i = 0; i < IPHASH_TABLE_SIZE; ++i) {
count[i] = 0;
for (src = iphash_table[i]; src; src = src->next) {
++count[i];
if (i < IPHASH_TABLE_SIZE) {
++total;
}
}
}
qsort(count, IPHASH_TABLE_SIZE, sizeof(count[0]), iphash_compare);
p = buf + apr_snprintf(buf, sizeof(buf),
APLOGNO(03235) "iphash: total hashed = %u, avg chain = %u, "
"chain lengths (count x len):",
total, total / IPHASH_TABLE_SIZE);
total = 1;
for (i = 1; i < IPHASH_TABLE_SIZE; ++i) {
if (count[i - 1] != count[i]) {
p += apr_snprintf(p, sizeof(buf) - (p - buf), " %ux%u",
total, count[i - 1]);
total = 1;
} else {
++total;
}
}
p += apr_snprintf(p, sizeof(buf) - (p - buf), " %ux%u",
total, count[IPHASH_TABLE_SIZE - 1]);
ap_log_error(APLOG_MARK, APLOG_DEBUG, main_s, buf);
}
#endif
static APR_INLINE unsigned hash_inaddr(unsigned key) {
key ^= (key >> 16);
return ((key >> 8) ^ key) % IPHASH_TABLE_SIZE;
}
static APR_INLINE unsigned hash_addr(struct apr_sockaddr_t *sa) {
unsigned key;
key = *(unsigned *)((char *)sa->ipaddr_ptr + sa->ipaddr_len - 4);
return hash_inaddr(key);
}
static ipaddr_chain *new_ipaddr_chain(apr_pool_t *p,
server_rec *s, server_addr_rec *sar) {
ipaddr_chain *new;
new = apr_palloc(p, sizeof(*new));
new->names = NULL;
new->initialnames = NULL;
new->server = s;
new->sar = sar;
new->next = NULL;
return new;
}
static name_chain *new_name_chain(apr_pool_t *p,
server_rec *s, server_addr_rec *sar) {
name_chain *new;
new = apr_palloc(p, sizeof(*new));
new->server = s;
new->sar = sar;
new->next = NULL;
return new;
}
static APR_INLINE ipaddr_chain *find_ipaddr(apr_sockaddr_t *sa) {
unsigned bucket;
ipaddr_chain *trav = NULL;
ipaddr_chain *wild_match = NULL;
bucket = hash_addr(sa);
for (trav = iphash_table[bucket]; trav; trav = trav->next) {
server_addr_rec *sar = trav->sar;
apr_sockaddr_t *cur = sar->host_addr;
if (cur->port == sa->port) {
if (apr_sockaddr_equal(cur, sa)) {
return trav;
}
}
if (wild_match == NULL && (cur->port == 0 || sa->port == 0)) {
if (apr_sockaddr_equal(cur, sa)) {
wild_match = trav;
}
}
}
return wild_match;
}
static ipaddr_chain *find_default_server(apr_port_t port) {
server_addr_rec *sar;
ipaddr_chain *trav = NULL;
ipaddr_chain *wild_match = NULL;
for (trav = default_list; trav; trav = trav->next) {
sar = trav->sar;
if (sar->host_port == port) {
return trav;
}
if (wild_match == NULL && sar->host_port == 0) {
wild_match = trav;
}
}
return wild_match;
}
#if APR_HAVE_IPV6
#define IS_IN6_ANYADDR(ad) ((ad)->family == APR_INET6 && IN6_IS_ADDR_UNSPECIFIED(&(ad)->sa.sin6.sin6_addr))
#else
#define IS_IN6_ANYADDR(ad) (0)
#endif
static void dump_a_vhost(apr_file_t *f, ipaddr_chain *ic) {
name_chain *nc;
int len;
char buf[MAX_STRING_LEN];
apr_sockaddr_t *ha = ic->sar->host_addr;
if ((ha->family == APR_INET && ha->sa.sin.sin_addr.s_addr == INADDR_ANY)
|| IS_IN6_ANYADDR(ha)) {
len = apr_snprintf(buf, sizeof(buf), "*:%u",
ic->sar->host_port);
} else {
len = apr_snprintf(buf, sizeof(buf), "%pI", ha);
}
if (ic->sar->host_port == 0) {
buf[len-1] = '*';
}
if (ic->names == NULL) {
apr_file_printf(f, "%-22s %s (%s:%u)\n", buf,
ic->server->server_hostname,
ic->server->defn_name, ic->server->defn_line_number);
return;
}
apr_file_printf(f, "%-22s is a NameVirtualHost\n"
"%8s default server %s (%s:%u)\n",
buf, "", ic->server->server_hostname,
ic->server->defn_name, ic->server->defn_line_number);
for (nc = ic->names; nc; nc = nc->next) {
if (nc->sar->host_port) {
apr_file_printf(f, "%8s port %u ", "", nc->sar->host_port);
} else {
apr_file_printf(f, "%8s port * ", "");
}
apr_file_printf(f, "namevhost %s (%s:%u)\n",
nc->server->server_hostname,
nc->server->defn_name, nc->server->defn_line_number);
if (nc->server->names) {
apr_array_header_t *names = nc->server->names;
char **name = (char **)names->elts;
int i;
for (i = 0; i < names->nelts; ++i) {
if (name[i]) {
apr_file_printf(f, "%16s alias %s\n", "", name[i]);
}
}
}
if (nc->server->wild_names) {
apr_array_header_t *names = nc->server->wild_names;
char **name = (char **)names->elts;
int i;
for (i = 0; i < names->nelts; ++i) {
if (name[i]) {
apr_file_printf(f, "%16s wild alias %s\n", "", name[i]);
}
}
}
}
}
static void dump_vhost_config(apr_file_t *f) {
ipaddr_chain *ic;
int i;
apr_file_printf(f, "VirtualHost configuration:\n");
for (i = 0; i < IPHASH_TABLE_SIZE; ++i) {
for (ic = iphash_table[i]; ic; ic = ic->next) {
dump_a_vhost(f, ic);
}
}
for (ic = default_list; ic; ic = ic->next) {
dump_a_vhost(f, ic);
}
}
static void add_name_vhost_config(apr_pool_t *p, server_rec *main_s,
server_rec *s, server_addr_rec *sar,
ipaddr_chain *ic) {
name_chain *nc = new_name_chain(p, s, sar);
nc->next = ic->names;
ic->server = s;
if (ic->names == NULL) {
if (ic->initialnames == NULL) {
ic->initialnames = nc;
} else {
nc->next = ic->initialnames;
ic->names = nc;
ic->initialnames = NULL;
}
} else {
ic->names = nc;
}
}
AP_DECLARE(void) ap_fini_vhost_config(apr_pool_t *p, server_rec *main_s) {
server_addr_rec *sar;
int has_default_vhost_addr;
server_rec *s;
int i;
ipaddr_chain **iphash_table_tail[IPHASH_TABLE_SIZE];
s = main_s;
if (!s->server_hostname) {
s->server_hostname = ap_get_local_host(p);
}
for (i = 0; i < IPHASH_TABLE_SIZE; ++i) {
iphash_table_tail[i] = &iphash_table[i];
}
for (s = main_s->next; s; s = s->next) {
server_addr_rec *sar_prev = NULL;
has_default_vhost_addr = 0;
for (sar = s->addrs; sar; sar = sar->next) {
ipaddr_chain *ic;
char inaddr_any[16] = {0};
if (!memcmp(sar->host_addr->ipaddr_ptr, inaddr_any, sar->host_addr->ipaddr_len)) {
ic = find_default_server(sar->host_port);
if (ic && sar->host_port == ic->sar->host_port) {
if (!sar_prev || memcmp(sar_prev->host_addr->ipaddr_ptr, inaddr_any, sar_prev->host_addr->ipaddr_len)
|| sar_prev->host_port != sar->host_port) {
add_name_vhost_config(p, main_s, s, sar, ic);
}
} else {
ic = new_ipaddr_chain(p, s, sar);
ic->next = default_list;
default_list = ic;
add_name_vhost_config(p, main_s, s, sar, ic);
}
has_default_vhost_addr = 1;
} else {
ic = find_ipaddr(sar->host_addr);
if (!ic || sar->host_port != ic->sar->host_port) {
unsigned bucket = hash_addr(sar->host_addr);
ic = new_ipaddr_chain(p, s, sar);
ic->next = *iphash_table_tail[bucket];
*iphash_table_tail[bucket] = ic;
}
add_name_vhost_config(p, main_s, s, sar, ic);
}
sar_prev = sar;
}
if (!s->server_hostname) {
if (has_default_vhost_addr) {
s->server_hostname = main_s->server_hostname;
} else if (!s->addrs) {
s->server_hostname =
apr_pstrdup(p, "bogus_host_without_forward_dns");
} else {
apr_status_t rv;
char *hostname;
rv = apr_getnameinfo(&hostname, s->addrs->host_addr, 0);
if (rv == APR_SUCCESS) {
s->server_hostname = apr_pstrdup(p, hostname);
} else {
char *ipaddr_str;
apr_sockaddr_ip_get(&ipaddr_str, s->addrs->host_addr);
ap_log_error(APLOG_MARK, APLOG_ERR, rv, main_s, APLOGNO(00549)
"Failed to resolve server name "
"for %s (check DNS) -- or specify an explicit "
"ServerName",
ipaddr_str);
s->server_hostname =
apr_pstrdup(p, "bogus_host_without_reverse_dns");
}
}
}
}
#if defined(IPHASH_STATISTICS)
dump_iphash_statistics(main_s);
#endif
if (ap_exists_config_define("DUMP_VHOSTS")) {
apr_file_t *thefile = NULL;
apr_file_open_stdout(&thefile, p);
dump_vhost_config(thefile);
}
}
static int vhost_check_config(apr_pool_t *p, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s) {
return config_error ? !OK : OK;
}
static apr_status_t fix_hostname_v6_literal(request_rec *r, char *host) {
char *dst;
int double_colon = 0;
for (dst = host; *dst; dst++) {
if (apr_isxdigit(*dst)) {
if (apr_isupper(*dst)) {
*dst = apr_tolower(*dst);
}
} else if (*dst == ':') {
if (*(dst + 1) == ':') {
if (double_colon)
return APR_EINVAL;
double_colon = 1;
} else if (*(dst + 1) == '.') {
return APR_EINVAL;
}
} else if (*dst == '.') {
if (*(dst + 1) == ':' || *(dst + 1) == '.')
return APR_EINVAL;
} else {
return APR_EINVAL;
}
}
return APR_SUCCESS;
}
static apr_status_t fix_hostname_non_v6(request_rec *r, char *host) {
char *dst;
for (dst = host; *dst; dst++) {
if (apr_islower(*dst)) {
} else if (*dst == '.') {
if (*(dst + 1) == '.') {
return APR_EINVAL;
}
} else if (apr_isupper(*dst)) {
*dst = apr_tolower(*dst);
} else if (*dst == '/' || *dst == '\\') {
return APR_EINVAL;
}
}
if (dst > host && dst[-1] == '.') {
dst[-1] = '\0';
}
return APR_SUCCESS;
}
static apr_status_t strict_hostname_check(request_rec *r, char *host) {
char *ch;
int is_dotted_decimal = 1, leading_zeroes = 0, dots = 0;
for (ch = host; *ch; ch++) {
if (apr_isalpha(*ch) || *ch == '-') {
is_dotted_decimal = 0;
} else if (ch[0] == '.') {
dots++;
if (ch[1] == '0' && apr_isdigit(ch[2]))
leading_zeroes = 1;
} else if (!apr_isdigit(*ch)) {
goto bad;
}
}
if (is_dotted_decimal) {
if (host[0] == '.' || (host[0] == '0' && apr_isdigit(host[1])))
leading_zeroes = 1;
if (leading_zeroes || dots != 3) {
goto bad;
}
} else {
while (ch > host && *ch != '.')
ch--;
if (ch[0] == '.' && ch[1] != '\0' && !apr_isalpha(ch[1]))
goto bad;
}
return APR_SUCCESS;
bad:
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02415)
"[strict] Invalid host name '%s'%s%.6s",
host, *ch ? ", problem near: " : "", ch);
return APR_EINVAL;
}
static int fix_hostname(request_rec *r, const char *host_header,
unsigned http_conformance) {
const char *src;
char *host, *scope_id;
apr_port_t port;
apr_status_t rv;
const char *c;
int is_v6literal = 0;
int strict = (http_conformance != AP_HTTP_CONFORMANCE_UNSAFE);
src = host_header ? host_header : r->hostname;
if (!*src) {
return is_v6literal;
}
for (c = src; apr_isdigit(*c); ++c);
if (!*c) {
if (strict) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02416)
"[strict] purely numeric host names not allowed: %s",
src);
goto bad_nolog;
}
r->hostname = src;
return is_v6literal;
}
if (host_header) {
rv = apr_parse_addr_port(&host, &scope_id, &port, src, r->pool);
if (rv != APR_SUCCESS || scope_id)
goto bad;
if (port) {
r->parsed_uri.port = port;
r->parsed_uri.port_str = apr_itoa(r->pool, (int)port);
}
if (host_header[0] == '[')
is_v6literal = 1;
} else {
host = apr_pstrdup(r->pool, r->hostname);
if (ap_strchr(host, ':') != NULL)
is_v6literal = 1;
}
if (is_v6literal) {
rv = fix_hostname_v6_literal(r, host);
} else {
rv = fix_hostname_non_v6(r, host);
if (strict && rv == APR_SUCCESS)
rv = strict_hostname_check(r, host);
}
if (rv != APR_SUCCESS)
goto bad;
r->hostname = host;
return is_v6literal;
bad:
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00550)
"Client sent malformed Host header: %s",
src);
bad_nolog:
r->status = HTTP_BAD_REQUEST;
return is_v6literal;
}
static int matches_aliases(server_rec *s, const char *host) {
int i;
apr_array_header_t *names;
if (!strcasecmp(host, s->server_hostname)) {
return 1;
}
names = s->names;
if (names) {
char **name = (char **) names->elts;
for (i = 0; i < names->nelts; ++i) {
if (!name[i]) continue;
if (!strcasecmp(host, name[i]))
return 1;
}
}
names = s->wild_names;
if (names) {
char **name = (char **) names->elts;
for (i = 0; i < names->nelts; ++i) {
if (!name[i]) continue;
if (!ap_strcasecmp_match(host, name[i]))
return 1;
}
}
return 0;
}
AP_DECLARE(int) ap_matches_request_vhost(request_rec *r, const char *host,
apr_port_t port) {
server_rec *s;
server_addr_rec *sar;
s = r->server;
for (sar = s->addrs; sar; sar = sar->next) {
if ((sar->host_port == 0 || port == sar->host_port)
&& !strcasecmp(host, sar->virthost)) {
return 1;
}
}
if (port != s->port) {
return 0;
}
return matches_aliases(s, host);
}
static void check_hostalias(request_rec *r) {
const char *host = r->hostname;
apr_port_t port;
server_rec *s;
server_rec *virthost_s;
server_rec *last_s;
name_chain *src;
virthost_s = NULL;
last_s = NULL;
port = r->connection->local_addr->port;
for (src = r->connection->vhost_lookup_data; src; src = src->next) {
server_addr_rec *sar;
sar = src->sar;
if (sar->host_port != 0 && port != sar->host_port) {
continue;
}
s = src->server;
if (s != last_s) {
if (matches_aliases(s, host)) {
goto found;
}
}
last_s = s;
if (!strcasecmp(host, sar->virthost)) {
if (virthost_s == NULL) {
virthost_s = s;
}
}
}
if (virthost_s) {
s = virthost_s;
goto found;
}
return;
found:
r->server = s;
}
static void check_serverpath(request_rec *r) {
server_rec *s;
server_rec *last_s;
name_chain *src;
apr_port_t port;
port = r->connection->local_addr->port;
last_s = NULL;
for (src = r->connection->vhost_lookup_data; src; src = src->next) {
if (src->sar->host_port != 0 && port != src->sar->host_port) {
continue;
}
s = src->server;
if (s == last_s) {
continue;
}
last_s = s;
if (s->path && !strncmp(r->uri, s->path, s->pathlen) &&
(s->path[s->pathlen - 1] == '/' ||
r->uri[s->pathlen] == '/' ||
r->uri[s->pathlen] == '\0')) {
r->server = s;
return;
}
}
}
static APR_INLINE const char *construct_host_header(request_rec *r,
int is_v6literal) {
struct iovec iov[5];
apr_size_t nvec = 0;
if (is_v6literal) {
iov[nvec].iov_base = "[";
iov[nvec].iov_len = 1;
nvec++;
}
iov[nvec].iov_base = (void *)r->hostname;
iov[nvec].iov_len = strlen(r->hostname);
nvec++;
if (is_v6literal) {
iov[nvec].iov_base = "]";
iov[nvec].iov_len = 1;
nvec++;
}
if (r->parsed_uri.port_str) {
iov[nvec].iov_base = ":";
iov[nvec].iov_len = 1;
nvec++;
iov[nvec].iov_base = r->parsed_uri.port_str;
iov[nvec].iov_len = strlen(r->parsed_uri.port_str);
nvec++;
}
return apr_pstrcatv(r->pool, iov, nvec, NULL);
}
AP_DECLARE(void) ap_update_vhost_from_headers(request_rec *r) {
core_server_config *conf = ap_get_core_module_config(r->server->module_config);
const char *host_header = apr_table_get(r->headers_in, "Host");
int is_v6literal = 0;
int have_hostname_from_url = 0;
if (r->hostname) {
have_hostname_from_url = 1;
is_v6literal = fix_hostname(r, NULL, conf->http_conformance);
} else if (host_header != NULL) {
is_v6literal = fix_hostname(r, host_header, conf->http_conformance);
}
if (r->status != HTTP_OK)
return;
if (conf->http_conformance != AP_HTTP_CONFORMANCE_UNSAFE) {
if (have_hostname_from_url && host_header != NULL) {
const char *repl = construct_host_header(r, is_v6literal);
apr_table_setn(r->headers_in, "Host", repl);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(02417)
"Replacing host header '%s' with host '%s' given "
"in the request uri", host_header, repl);
}
}
if (r->connection->vhost_lookup_data) {
if (r->hostname)
check_hostalias(r);
else
check_serverpath(r);
}
}
AP_DECLARE(int) ap_vhost_iterate_given_conn(conn_rec *conn,
ap_vhost_iterate_conn_cb func_cb,
void* baton) {
server_rec *s;
server_rec *last_s;
name_chain *src;
apr_port_t port;
int rv = 0;
if (conn->vhost_lookup_data) {
last_s = NULL;
port = conn->local_addr->port;
for (src = conn->vhost_lookup_data; src; src = src->next) {
server_addr_rec *sar;
sar = src->sar;
if (sar->host_port != 0 && port != sar->host_port) {
continue;
}
s = src->server;
if (s == last_s) {
continue;
}
last_s = s;
rv = func_cb(baton, conn, s);
if (rv != 0) {
break;
}
}
} else {
rv = func_cb(baton, conn, conn->base_server);
}
return rv;
}
AP_DECLARE(void) ap_update_vhost_given_ip(conn_rec *conn) {
ipaddr_chain *trav;
apr_port_t port;
trav = find_ipaddr(conn->local_addr);
if (trav) {
conn->vhost_lookup_data = trav->names;
conn->base_server = trav->server;
return;
}
port = conn->local_addr->port;
trav = find_default_server(port);
if (trav) {
conn->vhost_lookup_data = trav->names;
conn->base_server = trav->server;
return;
}
conn->vhost_lookup_data = NULL;
}
