#include "apr_network_io.h"
#include "apr_strings.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_main.h"
#include "http_config.h"
#include "http_core.h"
#include "ap_listen.h"
#include "http_log.h"
#include "mpm_common.h"
#include <stdlib.h>
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#undef APLOG_MODULE_INDEX
#define APLOG_MODULE_INDEX AP_CORE_MODULE_INDEX
AP_DECLARE_DATA ap_listen_rec *ap_listeners = NULL;
AP_DECLARE_DATA int ap_num_listen_buckets;
static ap_listen_rec **ap_listen_buckets;
AP_DECLARE_DATA int ap_have_so_reuseport = -1;
static ap_listen_rec *old_listeners;
static int ap_listenbacklog;
static int ap_listencbratio;
static int send_buffer_size;
static int receive_buffer_size;
static apr_status_t make_sock(apr_pool_t *p, ap_listen_rec *server) {
apr_socket_t *s = server->sd;
int one = 1;
#if APR_HAVE_IPV6
#if defined(AP_ENABLE_V4_MAPPED)
int v6only_setting = 0;
#else
int v6only_setting = 1;
#endif
#endif
apr_status_t stat;
#if !defined(WIN32)
stat = apr_socket_opt_set(s, APR_SO_REUSEADDR, one);
if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, stat, p, APLOGNO(00067)
"make_sock: for address %pI, apr_socket_opt_set: (SO_REUSEADDR)",
server->bind_addr);
apr_socket_close(s);
return stat;
}
#endif
stat = apr_socket_opt_set(s, APR_SO_KEEPALIVE, one);
if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, stat, p, APLOGNO(00068)
"make_sock: for address %pI, apr_socket_opt_set: (SO_KEEPALIVE)",
server->bind_addr);
apr_socket_close(s);
return stat;
}
#if APR_HAVE_IPV6
if (server->bind_addr->family == APR_INET6) {
stat = apr_socket_opt_set(s, APR_IPV6_V6ONLY, v6only_setting);
if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, stat, p, APLOGNO(00069)
"make_sock: for address %pI, apr_socket_opt_set: "
"(IPV6_V6ONLY)",
server->bind_addr);
apr_socket_close(s);
return stat;
}
}
#endif
if (send_buffer_size) {
stat = apr_socket_opt_set(s, APR_SO_SNDBUF, send_buffer_size);
if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
ap_log_perror(APLOG_MARK, APLOG_WARNING, stat, p, APLOGNO(00070)
"make_sock: failed to set SendBufferSize for "
"address %pI, using default",
server->bind_addr);
}
}
if (receive_buffer_size) {
stat = apr_socket_opt_set(s, APR_SO_RCVBUF, receive_buffer_size);
if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
ap_log_perror(APLOG_MARK, APLOG_WARNING, stat, p, APLOGNO(00071)
"make_sock: failed to set ReceiveBufferSize for "
"address %pI, using default",
server->bind_addr);
}
}
#if APR_TCP_NODELAY_INHERITED
ap_sock_disable_nagle(s);
#endif
#if defined(SO_REUSEPORT)
if (ap_have_so_reuseport && ap_listencbratio > 0) {
int thesock;
apr_os_sock_get(&thesock, s);
if (setsockopt(thesock, SOL_SOCKET, SO_REUSEPORT,
(void *)&one, sizeof(int)) < 0) {
stat = apr_get_netos_error();
ap_log_perror(APLOG_MARK, APLOG_CRIT, stat, p, APLOGNO(02638)
"make_sock: for address %pI, apr_socket_opt_set: "
"(SO_REUSEPORT)",
server->bind_addr);
apr_socket_close(s);
return stat;
}
}
#endif
if ((stat = apr_socket_bind(s, server->bind_addr)) != APR_SUCCESS) {
ap_log_perror(APLOG_MARK, APLOG_STARTUP|APLOG_CRIT, stat, p, APLOGNO(00072)
"make_sock: could not bind to address %pI",
server->bind_addr);
apr_socket_close(s);
return stat;
}
if ((stat = apr_socket_listen(s, ap_listenbacklog)) != APR_SUCCESS) {
ap_log_perror(APLOG_MARK, APLOG_STARTUP|APLOG_ERR, stat, p, APLOGNO(00073)
"make_sock: unable to listen for connections "
"on address %pI",
server->bind_addr);
apr_socket_close(s);
return stat;
}
#if defined(WIN32)
stat = apr_socket_opt_set(s, APR_SO_REUSEADDR, one);
if (stat != APR_SUCCESS && stat != APR_ENOTIMPL) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, stat, p, APLOGNO(00074)
"make_sock: for address %pI, apr_socket_opt_set: (SO_REUSEADDR)",
server->bind_addr);
apr_socket_close(s);
return stat;
}
#endif
server->sd = s;
server->active = 1;
server->accept_func = NULL;
return APR_SUCCESS;
}
static const char* find_accf_name(server_rec *s, const char *proto) {
const char* accf;
core_server_config *conf = ap_get_core_module_config(s->module_config);
if (!proto) {
return NULL;
}
accf = apr_table_get(conf->accf_map, proto);
if (accf && !strcmp("none", accf)) {
return NULL;
}
return accf;
}
static void ap_apply_accept_filter(apr_pool_t *p, ap_listen_rec *lis,
server_rec *server) {
apr_socket_t *s = lis->sd;
const char *accf;
apr_status_t rv;
const char *proto;
proto = lis->protocol;
if (!proto) {
proto = ap_get_server_protocol(server);
}
accf = find_accf_name(server, proto);
if (accf) {
#if APR_HAS_SO_ACCEPTFILTER
rv = apr_socket_accept_filter(s, apr_pstrdup(p, accf),
apr_pstrdup(p, ""));
if (rv != APR_SUCCESS && !APR_STATUS_IS_ENOTIMPL(rv)) {
ap_log_perror(APLOG_MARK, APLOG_WARNING, rv, p, APLOGNO(00075)
"Failed to enable the '%s' Accept Filter",
accf);
}
#else
rv = apr_socket_opt_set(s, APR_TCP_DEFER_ACCEPT, 30);
if (rv != APR_SUCCESS && !APR_STATUS_IS_ENOTIMPL(rv)) {
ap_log_perror(APLOG_MARK, APLOG_WARNING, rv, p, APLOGNO(00076)
"Failed to enable APR_TCP_DEFER_ACCEPT");
}
#endif
}
}
static apr_status_t close_listeners_on_exec(void *v) {
ap_close_listeners();
return APR_SUCCESS;
}
static int find_listeners(ap_listen_rec **from, ap_listen_rec **to,
const char *addr, apr_port_t port) {
int found = 0;
while (*from) {
apr_sockaddr_t *sa = (*from)->bind_addr;
if (sa) {
ap_listen_rec *new;
apr_port_t oldport;
oldport = sa->port;
if (port == oldport &&
((!addr && !sa->hostname) ||
((addr && sa->hostname) && !strcmp(sa->hostname, addr)))) {
found = 1;
if (!to) {
break;
}
new = *from;
*from = new->next;
new->next = *to;
*to = new;
continue;
}
}
from = &(*from)->next;
}
return found;
}
static const char *alloc_listener(process_rec *process, const char *addr,
apr_port_t port, const char* proto,
void *slave) {
ap_listen_rec *last;
apr_status_t status;
apr_sockaddr_t *sa;
if (find_listeners(&ap_listeners, NULL, addr, port)) {
return "Cannot define multiple Listeners on the same IP:port";
}
if (find_listeners(&old_listeners, &ap_listeners, addr, port)) {
if (ap_listeners->slave != slave) {
return "Cannot define a slave on the same IP:port as a Listener";
}
return NULL;
}
if ((status = apr_sockaddr_info_get(&sa, addr, APR_UNSPEC, port, 0,
process->pool))
!= APR_SUCCESS) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, status, process->pool, APLOGNO(00077)
"alloc_listener: failed to set up sockaddr for %s",
addr);
return "Listen setup failed";
}
last = ap_listeners;
while (last && last->next) {
last = last->next;
}
while (sa) {
ap_listen_rec *new;
new = apr_palloc(process->pool, sizeof(ap_listen_rec));
new->active = 0;
new->next = 0;
new->bind_addr = sa;
new->protocol = apr_pstrdup(process->pool, proto);
sa = sa->next;
status = apr_socket_create(&new->sd, new->bind_addr->family,
SOCK_STREAM, 0, process->pool);
#if APR_HAVE_IPV6
if (status != APR_SUCCESS && !addr &&
new->bind_addr->family == APR_INET6) {
continue;
}
#endif
if (status != APR_SUCCESS) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, status, process->pool, APLOGNO(00078)
"alloc_listener: failed to get a socket for %s",
addr);
return "Listen setup failed";
}
if (last == NULL) {
ap_listeners = last = new;
} else {
last->next = new;
last = new;
}
new->slave = slave;
}
return NULL;
}
#define IS_INADDR_ANY(addr) ((addr)->family == APR_INET && (addr)->sa.sin.sin_addr.s_addr == INADDR_ANY)
#define IS_IN6ADDR_ANY(addr) ((addr)->family == APR_INET6 && IN6_IS_ADDR_UNSPECIFIED(&(addr)->sa.sin6.sin6_addr))
static int open_listeners(apr_pool_t *pool) {
ap_listen_rec *lr;
ap_listen_rec *next;
ap_listen_rec *previous;
int num_open;
const char *userdata_key = "ap_open_listeners";
void *data;
#if AP_NONBLOCK_WHEN_MULTI_LISTEN
int use_nonblock;
#endif
num_open = 0;
previous = NULL;
for (lr = ap_listeners; lr; previous = lr, lr = lr->next) {
if (lr->active) {
++num_open;
} else {
#if APR_HAVE_IPV6
ap_listen_rec *cur;
int v6only_setting;
int skip = 0;
if (lr->next != NULL
&& IS_INADDR_ANY(lr->bind_addr)
&& lr->bind_addr->port == lr->next->bind_addr->port
&& IS_IN6ADDR_ANY(lr->next->bind_addr)) {
next = lr->next;
lr->next = next->next;
next->next = lr;
if (previous) {
previous->next = next;
} else {
ap_listeners = next;
}
lr = next;
}
if (IS_INADDR_ANY(lr->bind_addr) && previous) {
for (cur = ap_listeners; cur != lr; cur = cur->next) {
if (lr->bind_addr->port == cur->bind_addr->port
&& IS_IN6ADDR_ANY(cur->bind_addr)
&& apr_socket_opt_get(cur->sd, APR_IPV6_V6ONLY,
&v6only_setting) == APR_SUCCESS
&& v6only_setting == 0) {
previous->next = lr->next;
lr = previous;
skip = 1;
break;
}
}
if (skip) {
continue;
}
}
#endif
if (make_sock(pool, lr) == APR_SUCCESS) {
++num_open;
} else {
#if APR_HAVE_IPV6
if (lr->next != NULL
&& IS_IN6ADDR_ANY(lr->bind_addr)
&& lr->bind_addr->port == lr->next->bind_addr->port
&& IS_INADDR_ANY(lr->next->bind_addr)) {
if (previous) {
previous->next = lr->next;
} else {
ap_listeners = lr->next;
}
lr->bind_addr->family = AF_INET;
continue;
}
#endif
return -1;
}
}
}
ap_close_listeners_ex(old_listeners);
old_listeners = NULL;
#if AP_NONBLOCK_WHEN_MULTI_LISTEN
use_nonblock = (ap_listeners && ap_listeners->next);
for (lr = ap_listeners; lr; lr = lr->next) {
apr_status_t status;
status = apr_socket_opt_set(lr->sd, APR_SO_NONBLOCK, use_nonblock);
if (status != APR_SUCCESS) {
ap_log_perror(APLOG_MARK, APLOG_STARTUP|APLOG_ERR, status, pool, APLOGNO(00079)
"unable to control socket non-blocking status");
return -1;
}
}
#endif
apr_pool_userdata_get(&data, userdata_key, pool);
if (!data) {
apr_pool_userdata_set((const void *)1, userdata_key,
apr_pool_cleanup_null, pool);
apr_pool_cleanup_register(pool, NULL, apr_pool_cleanup_null,
close_listeners_on_exec);
}
return num_open ? 0 : -1;
}
AP_DECLARE(int) ap_setup_listeners(server_rec *s) {
server_rec *ls;
server_addr_rec *addr;
ap_listen_rec *lr;
int num_listeners = 0;
const char* proto;
int found;
for (ls = s; ls; ls = ls->next) {
proto = ap_get_server_protocol(ls);
if (!proto) {
found = 0;
for (addr = ls->addrs; addr && !found; addr = addr->next) {
for (lr = ap_listeners; lr; lr = lr->next) {
if (apr_sockaddr_equal(lr->bind_addr, addr->host_addr) &&
lr->bind_addr->port == addr->host_port) {
ap_set_server_protocol(ls, lr->protocol);
found = 1;
break;
}
}
}
if (!found) {
ap_set_server_protocol(ls, "http");
}
}
}
if (open_listeners(s->process->pool)) {
return 0;
}
for (lr = ap_listeners; lr; lr = lr->next) {
num_listeners++;
found = 0;
for (ls = s; ls && !found; ls = ls->next) {
for (addr = ls->addrs; addr && !found; addr = addr->next) {
if (apr_sockaddr_equal(lr->bind_addr, addr->host_addr) &&
lr->bind_addr->port == addr->host_port) {
found = 1;
ap_apply_accept_filter(s->process->pool, lr, ls);
}
}
}
if (!found) {
ap_apply_accept_filter(s->process->pool, lr, s);
}
}
return num_listeners;
}
AP_DECLARE(apr_status_t) ap_duplicate_listeners(apr_pool_t *p, server_rec *s,
ap_listen_rec ***buckets,
int *num_buckets) {
static int warn_once;
int i;
apr_status_t stat;
int use_nonblock = 0;
ap_listen_rec *lr;
if (*num_buckets < 1) {
*num_buckets = 1;
if (ap_listencbratio > 0) {
#if defined(_SC_NPROCESSORS_ONLN)
if (ap_have_so_reuseport) {
int num_online_cores = sysconf(_SC_NPROCESSORS_ONLN),
val = num_online_cores / ap_listencbratio;
if (val > 1) {
*num_buckets = val;
}
ap_log_perror(APLOG_MARK, APLOG_INFO, 0, p, APLOGNO(02819)
"Using %i listeners bucket(s) based on %i "
"online CPU cores and a ratio of %i",
*num_buckets, num_online_cores,
ap_listencbratio);
} else
#endif
if (!warn_once) {
ap_log_perror(APLOG_MARK, APLOG_WARNING, 0, p, APLOGNO(02820)
"ListenCoresBucketsRatio ignored without "
"SO_REUSEPORT and _SC_NPROCESSORS_ONLN "
"support: using a single listeners bucket");
warn_once = 1;
}
}
}
*buckets = apr_pcalloc(p, *num_buckets * sizeof(ap_listen_rec *));
(*buckets)[0] = ap_listeners;
for (i = 1; i < *num_buckets; i++) {
ap_listen_rec *last = NULL;
lr = ap_listeners;
while (lr) {
ap_listen_rec *duplr;
char *hostname;
apr_port_t port;
apr_sockaddr_t *sa;
duplr = apr_palloc(p, sizeof(ap_listen_rec));
duplr->slave = NULL;
duplr->protocol = apr_pstrdup(p, lr->protocol);
hostname = apr_pstrdup(p, lr->bind_addr->hostname);
port = lr->bind_addr->port;
apr_sockaddr_info_get(&sa, hostname, APR_UNSPEC, port, 0, p);
duplr->bind_addr = sa;
duplr->next = NULL;
stat = apr_socket_create(&duplr->sd, duplr->bind_addr->family,
SOCK_STREAM, 0, p);
if (stat != APR_SUCCESS) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, 0, p, APLOGNO(02640)
"ap_duplicate_listeners: for address %pI, "
"cannot duplicate a new socket!",
duplr->bind_addr);
return stat;
}
make_sock(p, duplr);
#if AP_NONBLOCK_WHEN_MULTI_LISTEN
use_nonblock = (ap_listeners && ap_listeners->next);
stat = apr_socket_opt_set(duplr->sd, APR_SO_NONBLOCK, use_nonblock);
if (stat != APR_SUCCESS) {
ap_log_perror(APLOG_MARK, APLOG_CRIT, stat, p, APLOGNO(02641)
"unable to control socket non-blocking status");
return stat;
}
#endif
ap_apply_accept_filter(p, duplr, s);
if (last == NULL) {
(*buckets)[i] = last = duplr;
} else {
last->next = duplr;
last = duplr;
}
lr = lr->next;
}
}
ap_listen_buckets = *buckets;
ap_num_listen_buckets = *num_buckets;
return APR_SUCCESS;
}
AP_DECLARE_NONSTD(void) ap_close_listeners(void) {
int i;
ap_close_listeners_ex(ap_listeners);
for (i = 1; i < ap_num_listen_buckets; i++) {
ap_close_listeners_ex(ap_listen_buckets[i]);
}
}
AP_DECLARE_NONSTD(void) ap_close_listeners_ex(ap_listen_rec *listeners) {
ap_listen_rec *lr;
for (lr = listeners; lr; lr = lr->next) {
apr_socket_close(lr->sd);
lr->active = 0;
}
}
AP_DECLARE_NONSTD(int) ap_close_selected_listeners(ap_slave_t *slave) {
ap_listen_rec *lr;
int n = 0;
for (lr = ap_listeners; lr; lr = lr->next) {
if (lr->slave != slave) {
apr_socket_close(lr->sd);
lr->active = 0;
} else {
++n;
}
}
return n;
}
AP_DECLARE(void) ap_listen_pre_config(void) {
old_listeners = ap_listeners;
ap_listeners = NULL;
ap_listen_buckets = NULL;
ap_num_listen_buckets = 0;
ap_listenbacklog = DEFAULT_LISTENBACKLOG;
ap_listencbratio = 0;
if (ap_have_so_reuseport < 0) {
#if defined(SO_REUSEPORT) && defined(__linux__)
apr_socket_t *sock;
if (apr_socket_create(&sock, APR_UNSPEC, SOCK_STREAM, 0,
ap_pglobal) == APR_SUCCESS) {
int thesock, on = 1;
apr_os_sock_get(&thesock, sock);
ap_have_so_reuseport = (setsockopt(thesock, SOL_SOCKET,
SO_REUSEPORT, (void *)&on,
sizeof(int)) == 0);
apr_socket_close(sock);
} else
#endif
ap_have_so_reuseport = 0;
}
}
AP_DECLARE_NONSTD(const char *) ap_set_listener(cmd_parms *cmd, void *dummy,
int argc, char *const argv[]) {
char *host, *scope_id, *proto;
apr_port_t port;
apr_status_t rv;
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
if (argc < 1 || argc > 2) {
return "Listen requires 1 or 2 arguments.";
}
rv = apr_parse_addr_port(&host, &scope_id, &port, argv[0], cmd->pool);
if (rv != APR_SUCCESS) {
return "Invalid address or port";
}
if (host && !strcmp(host, "*")) {
host = NULL;
}
if (scope_id) {
return "Scope id is not supported";
}
if (!port) {
return "Port must be specified";
}
if (argc != 2) {
if (port == 443) {
proto = "https";
} else {
proto = "http";
}
} else {
proto = apr_pstrdup(cmd->pool, argv[1]);
ap_str_tolower(proto);
}
return alloc_listener(cmd->server->process, host, port, proto, NULL);
}
AP_DECLARE_NONSTD(const char *) ap_set_listenbacklog(cmd_parms *cmd,
void *dummy,
const char *arg) {
int b;
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
b = atoi(arg);
if (b < 1) {
return "ListenBacklog must be > 0";
}
ap_listenbacklog = b;
return NULL;
}
AP_DECLARE_NONSTD(const char *) ap_set_listencbratio(cmd_parms *cmd,
void *dummy,
const char *arg) {
int b;
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
b = atoi(arg);
if (b < 1) {
return "ListenCoresBucketsRatio must be > 0";
}
ap_listencbratio = b;
return NULL;
}
AP_DECLARE_NONSTD(const char *) ap_set_send_buffer_size(cmd_parms *cmd,
void *dummy,
const char *arg) {
int s = atoi(arg);
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
if (s < 512 && s != 0) {
return "SendBufferSize must be >= 512 bytes, or 0 for system default.";
}
send_buffer_size = s;
return NULL;
}
AP_DECLARE_NONSTD(const char *) ap_set_receive_buffer_size(cmd_parms *cmd,
void *dummy,
const char *arg) {
int s = atoi(arg);
const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
if (err != NULL) {
return err;
}
if (s < 512 && s != 0) {
return "ReceiveBufferSize must be >= 512 bytes, or 0 for system default.";
}
receive_buffer_size = s;
return NULL;
}
