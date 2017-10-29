#include "apr.h"
#include "apr_network_io.h"
#include "apr_strings.h"
#include "apr_optional.h"
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "util_ebcdic.h"
#if !defined(DEFAULT_RFC1413)
#define DEFAULT_RFC1413 0
#endif
#define RFC1413_UNSET 2
#if !defined(RFC1413_TIMEOUT)
#define RFC1413_TIMEOUT 30
#endif
#define RFC1413_PORT 113
#define RFC1413_USERLEN 512
#define RFC1413_MAXDATA 1000
#define FROM_UNKNOWN "unknown"
typedef struct {
int do_rfc1413;
int timeout_unset;
apr_time_t timeout;
} ident_config_rec;
static apr_status_t rfc1413_connect(apr_socket_t **newsock, conn_rec *conn,
server_rec *srv, apr_time_t timeout) {
apr_status_t rv;
apr_sockaddr_t *localsa, *destsa;
if ((rv = apr_sockaddr_info_get(&localsa, conn->local_ip, APR_UNSPEC,
0,
0, conn->pool)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, srv, APLOGNO(01492)
"rfc1413: apr_sockaddr_info_get(%s) failed",
conn->local_ip);
return rv;
}
if ((rv = apr_sockaddr_info_get(&destsa, conn->client_ip,
localsa->family,
RFC1413_PORT, 0, conn->pool)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, srv, APLOGNO(01493)
"rfc1413: apr_sockaddr_info_get(%s) failed",
conn->client_ip);
return rv;
}
if ((rv = apr_socket_create(newsock,
localsa->family,
SOCK_STREAM, 0, conn->pool)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, srv, APLOGNO(01494)
"rfc1413: error creating query socket");
return rv;
}
if ((rv = apr_socket_timeout_set(*newsock, timeout)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, srv, APLOGNO(01495)
"rfc1413: error setting query socket timeout");
apr_socket_close(*newsock);
return rv;
}
if ((rv = apr_socket_bind(*newsock, localsa)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, srv, APLOGNO(01496)
"rfc1413: Error binding query socket to local port");
apr_socket_close(*newsock);
return rv;
}
if ((rv = apr_socket_connect(*newsock, destsa)) != APR_SUCCESS) {
apr_socket_close(*newsock);
return rv;
}
return APR_SUCCESS;
}
static apr_status_t rfc1413_query(apr_socket_t *sock, conn_rec *conn,
server_rec *srv) {
apr_port_t rmt_port, our_port;
apr_port_t sav_rmt_port, sav_our_port;
apr_size_t i;
char *cp;
char buffer[RFC1413_MAXDATA + 1];
char user[RFC1413_USERLEN + 1];
apr_size_t buflen;
sav_our_port = conn->local_addr->port;
sav_rmt_port = conn->client_addr->port;
buflen = apr_snprintf(buffer, sizeof(buffer), "%hu,%hu\r\n", sav_rmt_port,
sav_our_port);
ap_xlate_proto_to_ascii(buffer, buflen);
i = 0;
while (i < buflen) {
apr_size_t j = strlen(buffer + i);
apr_status_t status;
status = apr_socket_send(sock, buffer+i, &j);
if (status != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, status, srv, APLOGNO(01497)
"write: rfc1413: error sending request");
return status;
} else if (j > 0) {
i+=j;
}
}
i = 0;
memset(buffer, '\0', sizeof(buffer));
while ((cp = strchr(buffer, '\012')) == NULL && i < sizeof(buffer) - 1) {
apr_size_t j = sizeof(buffer) - 1 - i;
apr_status_t status;
status = apr_socket_recv(sock, buffer+i, &j);
if (status != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_CRIT, status, srv, APLOGNO(01498)
"read: rfc1413: error reading response");
return status;
} else if (j > 0) {
i+=j;
} else if (status == APR_SUCCESS && j == 0) {
return APR_EINVAL;
}
}
ap_xlate_proto_from_ascii(buffer, i);
if (sscanf(buffer, "%hu , %hu : USERID :%*[^:]:%512s", &rmt_port, &our_port,
user) != 3 || sav_rmt_port != rmt_port
|| sav_our_port != our_port)
return APR_EINVAL;
if ((cp = strchr(user, '\r')))
*cp = '\0';
conn->remote_logname = apr_pstrdup(conn->pool, user);
return APR_SUCCESS;
}
static const char *set_idcheck(cmd_parms *cmd, void *d_, int arg) {
ident_config_rec *d = d_;
d->do_rfc1413 = arg ? 1 : 0;
return NULL;
}
static const char *set_timeout(cmd_parms *cmd, void *d_, const char *arg) {
ident_config_rec *d = d_;
d->timeout = apr_time_from_sec(atoi(arg));
d->timeout_unset = 0;
return NULL;
}
static void *create_ident_dir_config(apr_pool_t *p, char *d) {
ident_config_rec *conf = apr_palloc(p, sizeof(*conf));
conf->do_rfc1413 = DEFAULT_RFC1413 | RFC1413_UNSET;
conf->timeout = apr_time_from_sec(RFC1413_TIMEOUT);
conf->timeout_unset = 1;
return (void *)conf;
}
static void *merge_ident_dir_config(apr_pool_t *p, void *old_, void *new_) {
ident_config_rec *conf = (ident_config_rec *)apr_pcalloc(p, sizeof(*conf));
ident_config_rec *old = (ident_config_rec *) old_;
ident_config_rec *new = (ident_config_rec *) new_;
conf->timeout = new->timeout_unset
? old->timeout
: new->timeout;
conf->do_rfc1413 = new->do_rfc1413 & RFC1413_UNSET
? old->do_rfc1413
: new->do_rfc1413;
return (void *)conf;
}
static const command_rec ident_cmds[] = {
AP_INIT_FLAG("IdentityCheck", set_idcheck, NULL, RSRC_CONF|ACCESS_CONF,
"Enable identd (RFC 1413) user lookups - SLOW"),
AP_INIT_TAKE1("IdentityCheckTimeout", set_timeout, NULL,
RSRC_CONF|ACCESS_CONF,
"Identity check (RFC 1413) timeout duration (sec)"),
{NULL}
};
module AP_MODULE_DECLARE_DATA ident_module;
static const char *ap_ident_lookup(request_rec *r) {
ident_config_rec *conf;
apr_socket_t *sock;
apr_status_t rv;
conn_rec *conn = r->connection;
server_rec *srv = r->server;
conf = ap_get_module_config(r->per_dir_config, &ident_module);
if (!(conf->do_rfc1413 & ~RFC1413_UNSET)) {
return NULL;
}
rv = rfc1413_connect(&sock, conn, srv, conf->timeout);
if (rv == APR_SUCCESS) {
rv = rfc1413_query(sock, conn, srv);
apr_socket_close(sock);
}
if (rv != APR_SUCCESS) {
conn->remote_logname = FROM_UNKNOWN;
}
return (const char *)conn->remote_logname;
}
static void register_hooks(apr_pool_t *p) {
APR_REGISTER_OPTIONAL_FN(ap_ident_lookup);
}
AP_DECLARE_MODULE(ident) = {
STANDARD20_MODULE_STUFF,
create_ident_dir_config,
merge_ident_dir_config,
NULL,
NULL,
ident_cmds,
register_hooks
};
