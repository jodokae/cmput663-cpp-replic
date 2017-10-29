#include <assert.h>
#include <ctype.h>
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_general.h>
#include <apr_xml.h>
#include <ne_auth.h>
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_ra.h"
#include "../libsvn_ra/ra_loader.h"
#include "svn_config.h"
#include "svn_delta.h"
#include "svn_version.h"
#include "svn_path.h"
#include "svn_time.h"
#include "svn_xml.h"
#include "svn_private_config.h"
#if defined(SVN_NEON_0_28)
#include <ne_pkcs11.h>
#endif
#include "ra_neon.h"
#define DEFAULT_HTTP_TIMEOUT 3600
static apr_status_t cleanup_session(void *sess) {
ne_session_destroy(sess);
return APR_SUCCESS;
}
static apr_status_t cleanup_uri(void *uri) {
ne_uri_free(uri);
return APR_SUCCESS;
}
#if defined(SVN_NEON_0_28)
static apr_status_t cleanup_p11provider(void *provider) {
ne_ssl_pkcs11_provider *prov = provider;
ne_ssl_pkcs11_provider_destroy(prov);
return APR_SUCCESS;
}
#endif
static int request_auth(void *userdata, const char *realm, int attempt,
char *username, char *password) {
svn_error_t *err;
svn_ra_neon__session_t *ras = userdata;
void *creds;
svn_auth_cred_simple_t *simple_creds;
ras->auth_username = NULL;
if (! ras->callbacks->auth_baton)
return -1;
if (attempt == 0 || ras->auth_iterstate == NULL) {
const char *realmstring;
realmstring = apr_psprintf(ras->pool, "<%s://%s:%d> %s",
ras->root.scheme, ras->root.host,
ras->root.port, realm);
err = svn_auth_first_credentials(&creds,
&(ras->auth_iterstate),
SVN_AUTH_CRED_SIMPLE,
realmstring,
ras->callbacks->auth_baton,
ras->pool);
}
else
err = svn_auth_next_credentials(&creds,
ras->auth_iterstate,
ras->pool);
if (err || ! creds) {
svn_error_clear(err);
return -1;
}
simple_creds = creds;
apr_cpystrn(username, simple_creds->username, NE_ABUFSIZ);
apr_cpystrn(password, simple_creds->password, NE_ABUFSIZ);
ras->auth_username = apr_pstrdup(ras->pool, simple_creds->username);
return 0;
}
static const apr_uint32_t neon_failure_map[][2] = {
{ NE_SSL_NOTYETVALID, SVN_AUTH_SSL_NOTYETVALID },
{ NE_SSL_EXPIRED, SVN_AUTH_SSL_EXPIRED },
{ NE_SSL_IDMISMATCH, SVN_AUTH_SSL_CNMISMATCH },
{ NE_SSL_UNTRUSTED, SVN_AUTH_SSL_UNKNOWNCA }
};
static apr_uint32_t
convert_neon_failures(int neon_failures) {
apr_uint32_t svn_failures = 0;
apr_size_t i;
for (i = 0; i < sizeof(neon_failure_map) / (2 * sizeof(int)); ++i) {
if (neon_failures & neon_failure_map[i][0]) {
svn_failures |= neon_failure_map[i][1];
neon_failures &= ~neon_failure_map[i][0];
}
}
if (neon_failures) {
svn_failures |= SVN_AUTH_SSL_OTHER;
}
return svn_failures;
}
static int
server_ssl_callback(void *userdata,
int failures,
const ne_ssl_certificate *cert) {
svn_ra_neon__session_t *ras = userdata;
svn_auth_cred_ssl_server_trust_t *server_creds = NULL;
void *creds;
svn_auth_iterstate_t *state;
apr_pool_t *pool;
svn_error_t *error;
char *ascii_cert = ne_ssl_cert_export(cert);
char *issuer_dname = ne_ssl_readable_dname(ne_ssl_cert_issuer(cert));
svn_auth_ssl_server_cert_info_t cert_info;
char fingerprint[NE_SSL_DIGESTLEN];
char valid_from[NE_SSL_VDATELEN], valid_until[NE_SSL_VDATELEN];
const char *realmstring;
apr_uint32_t *svn_failures = apr_palloc(ras->pool, sizeof(*svn_failures));
realmstring = apr_psprintf(ras->pool, "%s://%s:%d", ras->root.scheme,
ras->root.host, ras->root.port);
*svn_failures = convert_neon_failures(failures);
svn_auth_set_parameter(ras->callbacks->auth_baton,
SVN_AUTH_PARAM_SSL_SERVER_FAILURES,
svn_failures);
cert_info.hostname = ne_ssl_cert_identity(cert);
if (ne_ssl_cert_digest(cert, fingerprint) != 0) {
strcpy(fingerprint, "<unknown>");
}
cert_info.fingerprint = fingerprint;
ne_ssl_cert_validity(cert, valid_from, valid_until);
cert_info.valid_from = valid_from;
cert_info.valid_until = valid_until;
cert_info.issuer_dname = issuer_dname;
cert_info.ascii_cert = ascii_cert;
svn_auth_set_parameter(ras->callbacks->auth_baton,
SVN_AUTH_PARAM_SSL_SERVER_CERT_INFO,
&cert_info);
apr_pool_create(&pool, ras->pool);
error = svn_auth_first_credentials(&creds, &state,
SVN_AUTH_CRED_SSL_SERVER_TRUST,
realmstring,
ras->callbacks->auth_baton,
pool);
if (error || ! creds) {
svn_error_clear(error);
} else {
server_creds = creds;
error = svn_auth_save_credentials(state, pool);
if (error) {
svn_error_clear(error);
}
}
free(issuer_dname);
free(ascii_cert);
svn_auth_set_parameter(ras->callbacks->auth_baton,
SVN_AUTH_PARAM_SSL_SERVER_CERT_INFO, NULL);
svn_pool_destroy(pool);
return ! server_creds;
}
static svn_boolean_t
client_ssl_decrypt_cert(svn_ra_neon__session_t *ras,
const char *cert_file,
ne_ssl_client_cert *clicert) {
svn_auth_iterstate_t *state;
svn_error_t *error;
apr_pool_t *pool;
svn_boolean_t ok = FALSE;
void *creds;
int try;
apr_pool_create(&pool, ras->pool);
for (try = 0; TRUE; ++try) {
if (try == 0) {
error = svn_auth_first_credentials(&creds, &state,
SVN_AUTH_CRED_SSL_CLIENT_CERT_PW,
cert_file,
ras->callbacks->auth_baton,
pool);
}
else {
error = svn_auth_next_credentials(&creds, state, pool);
}
if (error || ! creds) {
svn_error_clear(error);
break;
} else {
svn_auth_cred_ssl_client_cert_pw_t *pw_creds = creds;
if (ne_ssl_clicert_decrypt(clicert, pw_creds->password) == 0) {
ok = TRUE;
break;
}
}
}
svn_pool_destroy(pool);
return ok;
}
#if defined(SVN_NEON_0_28)
static int
client_ssl_pkcs11_pin_entry(void *userdata,
int attempt,
const char *slot_descr,
const char *token_label,
unsigned int flags,
char *pin) {
svn_ra_neon__session_t *ras = userdata;
svn_error_t *err;
void *creds;
svn_auth_cred_ssl_client_cert_pw_t *pw_creds;
svn_auth_set_parameter
(ras->callbacks->auth_baton, SVN_AUTH_PARAM_NO_AUTH_CACHE, "");
if (attempt == 0) {
const char *realmstring;
realmstring = apr_psprintf(ras->pool,
_("PIN for token \"%s\" in slot \"%s\""),
token_label, slot_descr);
err = svn_auth_first_credentials(&creds,
&(ras->auth_iterstate),
SVN_AUTH_CRED_SSL_CLIENT_CERT_PW,
realmstring,
ras->callbacks->auth_baton,
ras->pool);
} else
err = svn_auth_next_credentials(&creds,
ras->auth_iterstate,
ras->pool);
if (err || ! creds) {
svn_error_clear(err);
return -1;
}
pw_creds = creds;
apr_cpystrn(pin, pw_creds->password, NE_SSL_P11PINLEN);
return 0;
}
#endif
static void
client_ssl_callback(void *userdata, ne_session *sess,
const ne_ssl_dname *const *dnames,
int dncount) {
svn_ra_neon__session_t *ras = userdata;
ne_ssl_client_cert *clicert = NULL;
void *creds;
svn_auth_iterstate_t *state;
const char *realmstring;
apr_pool_t *pool;
svn_error_t *error;
int try;
apr_pool_create(&pool, ras->pool);
realmstring = apr_psprintf(pool, "%s://%s:%d", ras->root.scheme,
ras->root.host, ras->root.port);
for (try = 0; TRUE; ++try) {
if (try == 0) {
error = svn_auth_first_credentials(&creds, &state,
SVN_AUTH_CRED_SSL_CLIENT_CERT,
realmstring,
ras->callbacks->auth_baton,
pool);
}
else {
error = svn_auth_next_credentials(&creds, state, pool);
}
if (error || ! creds) {
svn_error_clear(error);
break;
} else {
svn_auth_cred_ssl_client_cert_t *client_creds = creds;
clicert = ne_ssl_clicert_read(client_creds->cert_file);
if (clicert) {
if (! ne_ssl_clicert_encrypted(clicert) ||
client_ssl_decrypt_cert(ras, client_creds->cert_file,
clicert)) {
ne_ssl_set_clicert(sess, clicert);
}
break;
}
}
}
svn_pool_destroy(pool);
}
static svn_error_t *get_server_settings(const char **proxy_host,
unsigned int *proxy_port,
const char **proxy_username,
const char **proxy_password,
int *timeout_seconds,
int *neon_debug,
svn_boolean_t *compression,
unsigned int *neon_auth_types,
const char **pk11_provider,
svn_config_t *cfg,
const char *requested_host,
apr_pool_t *pool) {
const char *exceptions, *port_str, *timeout_str, *server_group;
const char *debug_str, *http_auth_types;
svn_boolean_t is_exception = FALSE;
*proxy_host = NULL;
*proxy_port = (unsigned int) -1;
*proxy_username = NULL;
*proxy_password = NULL;
port_str = NULL;
timeout_str = NULL;
debug_str = NULL;
http_auth_types = NULL;
*pk11_provider = NULL;
svn_config_get(cfg, &exceptions, SVN_CONFIG_SECTION_GLOBAL,
SVN_CONFIG_OPTION_HTTP_PROXY_EXCEPTIONS, NULL);
if (exceptions) {
apr_array_header_t *l = svn_cstring_split(exceptions, ",", TRUE, pool);
is_exception = svn_cstring_match_glob_list(requested_host, l);
}
if (! is_exception) {
svn_config_get(cfg, proxy_host, SVN_CONFIG_SECTION_GLOBAL,
SVN_CONFIG_OPTION_HTTP_PROXY_HOST, NULL);
svn_config_get(cfg, &port_str, SVN_CONFIG_SECTION_GLOBAL,
SVN_CONFIG_OPTION_HTTP_PROXY_PORT, NULL);
svn_config_get(cfg, proxy_username, SVN_CONFIG_SECTION_GLOBAL,
SVN_CONFIG_OPTION_HTTP_PROXY_USERNAME, NULL);
svn_config_get(cfg, proxy_password, SVN_CONFIG_SECTION_GLOBAL,
SVN_CONFIG_OPTION_HTTP_PROXY_PASSWORD, NULL);
}
svn_config_get(cfg, &timeout_str, SVN_CONFIG_SECTION_GLOBAL,
SVN_CONFIG_OPTION_HTTP_TIMEOUT, NULL);
SVN_ERR(svn_config_get_bool(cfg, compression, SVN_CONFIG_SECTION_GLOBAL,
SVN_CONFIG_OPTION_HTTP_COMPRESSION, TRUE));
svn_config_get(cfg, &debug_str, SVN_CONFIG_SECTION_GLOBAL,
SVN_CONFIG_OPTION_NEON_DEBUG_MASK, NULL);
#if defined(SVN_NEON_0_26)
svn_config_get(cfg, &http_auth_types, SVN_CONFIG_SECTION_GLOBAL,
SVN_CONFIG_OPTION_HTTP_AUTH_TYPES, NULL);
#endif
svn_config_get(cfg, pk11_provider, SVN_CONFIG_SECTION_GLOBAL,
SVN_CONFIG_OPTION_SSL_PKCS11_PROVIDER, NULL);
if (cfg)
server_group = svn_config_find_group(cfg, requested_host,
SVN_CONFIG_SECTION_GROUPS, pool);
else
server_group = NULL;
if (server_group) {
svn_config_get(cfg, proxy_host, server_group,
SVN_CONFIG_OPTION_HTTP_PROXY_HOST, *proxy_host);
svn_config_get(cfg, &port_str, server_group,
SVN_CONFIG_OPTION_HTTP_PROXY_PORT, port_str);
svn_config_get(cfg, proxy_username, server_group,
SVN_CONFIG_OPTION_HTTP_PROXY_USERNAME, *proxy_username);
svn_config_get(cfg, proxy_password, server_group,
SVN_CONFIG_OPTION_HTTP_PROXY_PASSWORD, *proxy_password);
svn_config_get(cfg, &timeout_str, server_group,
SVN_CONFIG_OPTION_HTTP_TIMEOUT, timeout_str);
SVN_ERR(svn_config_get_bool(cfg, compression, server_group,
SVN_CONFIG_OPTION_HTTP_COMPRESSION,
*compression));
svn_config_get(cfg, &debug_str, server_group,
SVN_CONFIG_OPTION_NEON_DEBUG_MASK, debug_str);
#if defined(SVN_NEON_0_26)
svn_config_get(cfg, &http_auth_types, server_group,
SVN_CONFIG_OPTION_HTTP_AUTH_TYPES, http_auth_types);
#endif
svn_config_get(cfg, pk11_provider, server_group,
SVN_CONFIG_OPTION_SSL_PKCS11_PROVIDER, *pk11_provider);
}
if (port_str) {
char *endstr;
const long int port = strtol(port_str, &endstr, 10);
if (*endstr)
return svn_error_create(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("Invalid URL: illegal character in proxy "
"port number"));
if (port < 0)
return svn_error_create(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("Invalid URL: negative proxy port number"));
if (port > 65535)
return svn_error_create(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("Invalid URL: proxy port number greater "
"than maximum TCP port number 65535"));
*proxy_port = port;
} else
*proxy_port = 80;
if (timeout_str) {
char *endstr;
const long int timeout = strtol(timeout_str, &endstr, 10);
if (*endstr)
return svn_error_create(SVN_ERR_RA_DAV_INVALID_CONFIG_VALUE, NULL,
_("Invalid config: illegal character in "
"timeout value"));
if (timeout < 0)
return svn_error_create(SVN_ERR_RA_DAV_INVALID_CONFIG_VALUE, NULL,
_("Invalid config: negative timeout value"));
*timeout_seconds = timeout;
} else
*timeout_seconds = 0;
if (debug_str) {
char *endstr;
const long int debug = strtol(debug_str, &endstr, 10);
if (*endstr)
return svn_error_create(SVN_ERR_RA_DAV_INVALID_CONFIG_VALUE, NULL,
_("Invalid config: illegal character in "
"debug mask value"));
*neon_debug = debug;
} else
*neon_debug = 0;
#if defined(SVN_NEON_0_26)
if (http_auth_types) {
char *token, *last;
char *auth_types_list = apr_palloc(pool, strlen(http_auth_types) + 1);
apr_collapse_spaces(auth_types_list, http_auth_types);
while ((token = apr_strtok(auth_types_list, ";", &last)) != NULL) {
auth_types_list = NULL;
if (svn_cstring_casecmp("basic", token) == 0)
*neon_auth_types |= NE_AUTH_BASIC;
else if (svn_cstring_casecmp("digest", token) == 0)
*neon_auth_types |= NE_AUTH_DIGEST;
else if (svn_cstring_casecmp("negotiate", token) == 0)
*neon_auth_types |= NE_AUTH_NEGOTIATE;
else
return svn_error_createf(SVN_ERR_RA_DAV_INVALID_CONFIG_VALUE, NULL,
_("Invalid config: unknown http auth"
"type '%s'"), token);
}
}
#endif
return SVN_NO_ERROR;
}
struct proxy_auth_baton {
const char *username;
const char *password;
};
static int proxy_auth(void *userdata,
const char *realm,
int attempt,
char *username,
char *password) {
struct proxy_auth_baton *pab = userdata;
if (attempt >= 10)
return 1;
apr_cpystrn(username, pab->username, NE_ABUFSIZ);
apr_cpystrn(password, pab->password, NE_ABUFSIZ);
return 0;
}
#define RA_NEON_DESCRIPTION N_("Module for accessing a repository via WebDAV protocol using Neon.")
static const char *
ra_neon_get_description(void) {
return _(RA_NEON_DESCRIPTION);
}
static const char * const *
ra_neon_get_schemes(apr_pool_t *pool) {
static const char *schemes_no_ssl[] = { "http", NULL };
static const char *schemes_ssl[] = { "http", "https", NULL };
return ne_has_support(NE_FEATURE_SSL) ? schemes_ssl : schemes_no_ssl;
}
typedef struct neonprogress_baton_t {
svn_ra_progress_notify_func_t progress_func;
void *progress_baton;
apr_pool_t *pool;
} neonprogress_baton_t;
static void
#if defined(SVN_NEON_0_27)
ra_neon_neonprogress(void *baton, ne_off_t progress, ne_off_t total)
#else
ra_neon_neonprogress(void *baton, off_t progress, off_t total)
#endif
{
const neonprogress_baton_t *neonprogress_baton = baton;
if (neonprogress_baton->progress_func) {
neonprogress_baton->progress_func(progress, total,
neonprogress_baton->progress_baton,
neonprogress_baton->pool);
}
}
static const char *capability_yes = "yes";
static const char *capability_no = "no";
static const char *capability_server_yes = "server-yes";
static void
parse_capabilities(ne_request *req,
svn_ra_neon__session_t *ras,
apr_pool_t *pool) {
const char *header_value;
apr_hash_set(ras->capabilities, SVN_RA_CAPABILITY_DEPTH,
APR_HASH_KEY_STRING, capability_no);
apr_hash_set(ras->capabilities, SVN_RA_CAPABILITY_MERGEINFO,
APR_HASH_KEY_STRING, capability_no);
apr_hash_set(ras->capabilities, SVN_RA_CAPABILITY_LOG_REVPROPS,
APR_HASH_KEY_STRING, capability_no);
header_value = ne_get_response_header(req, "dav");
if (header_value) {
apr_array_header_t *vals =
svn_cstring_split(header_value, ",", TRUE, pool);
if (svn_cstring_match_glob_list(SVN_DAV_NS_DAV_SVN_DEPTH, vals))
apr_hash_set(ras->capabilities, SVN_RA_CAPABILITY_DEPTH,
APR_HASH_KEY_STRING, capability_yes);
if (svn_cstring_match_glob_list(SVN_DAV_NS_DAV_SVN_MERGEINFO, vals))
apr_hash_set(ras->capabilities, SVN_RA_CAPABILITY_MERGEINFO,
APR_HASH_KEY_STRING, capability_server_yes);
if (svn_cstring_match_glob_list(SVN_DAV_NS_DAV_SVN_LOG_REVPROPS, vals))
apr_hash_set(ras->capabilities, SVN_RA_CAPABILITY_LOG_REVPROPS,
APR_HASH_KEY_STRING, capability_yes);
if (svn_cstring_match_glob_list(SVN_DAV_NS_DAV_SVN_PARTIAL_REPLAY,
vals))
apr_hash_set(ras->capabilities, SVN_RA_CAPABILITY_PARTIAL_REPLAY,
APR_HASH_KEY_STRING, capability_yes);
}
}
static svn_error_t *
exchange_capabilities(svn_ra_neon__session_t *ras, apr_pool_t *pool) {
int http_ret_code;
svn_ra_neon__request_t *rar;
svn_error_t *err = SVN_NO_ERROR;
rar = svn_ra_neon__request_create(ras, "OPTIONS", ras->url->data, pool);
ne_add_request_header(rar->ne_req, "DAV", SVN_DAV_NS_DAV_SVN_DEPTH);
ne_add_request_header(rar->ne_req, "DAV", SVN_DAV_NS_DAV_SVN_MERGEINFO);
ne_add_request_header(rar->ne_req, "DAV", SVN_DAV_NS_DAV_SVN_LOG_REVPROPS);
err = svn_ra_neon__request_dispatch(&http_ret_code, rar,
NULL, NULL, 200, 0, pool);
if (err)
goto cleanup;
if (http_ret_code == 200) {
parse_capabilities(rar->ne_req, ras, pool);
} else {
return svn_error_createf
(SVN_ERR_RA_DAV_OPTIONS_REQ_FAILED, NULL,
_("OPTIONS request (for capabilities) got HTTP response code %d"),
http_ret_code);
}
cleanup:
svn_ra_neon__request_destroy(rar);
return err;
}
svn_error_t *
svn_ra_neon__has_capability(svn_ra_session_t *session,
svn_boolean_t *has,
const char *capability,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
const char *cap_result;
if (strcmp(capability, SVN_RA_CAPABILITY_COMMIT_REVPROPS) == 0) {
*has = TRUE;
return SVN_NO_ERROR;
}
cap_result = apr_hash_get(ras->capabilities,
capability,
APR_HASH_KEY_STRING);
if (cap_result == NULL)
SVN_ERR(exchange_capabilities(ras, pool));
cap_result = apr_hash_get(ras->capabilities,
capability, APR_HASH_KEY_STRING);
if (cap_result == capability_server_yes) {
if (strcmp(capability, SVN_RA_CAPABILITY_MERGEINFO) == 0) {
svn_mergeinfo_catalog_t ignored;
svn_error_t *err;
apr_array_header_t *paths = apr_array_make(pool, 1,
sizeof(char *));
APR_ARRAY_PUSH(paths, const char *) = "";
err = svn_ra_neon__get_mergeinfo(session, &ignored, paths, 0,
FALSE, FALSE, pool);
if (err) {
if (err->apr_err == SVN_ERR_UNSUPPORTED_FEATURE) {
svn_error_clear(err);
cap_result = capability_no;
} else if (err->apr_err == SVN_ERR_FS_NOT_FOUND
|| err->apr_err == SVN_ERR_RA_DAV_PATH_NOT_FOUND) {
svn_error_clear(err);
cap_result = capability_yes;
} else
return err;
} else
cap_result = capability_yes;
apr_hash_set(ras->capabilities,
SVN_RA_CAPABILITY_MERGEINFO, APR_HASH_KEY_STRING,
cap_result);
} else {
return svn_error_createf
(SVN_ERR_UNKNOWN_CAPABILITY, NULL,
_("Don't know how to handle '%s' for capability '%s'"),
capability_server_yes, capability);
}
}
if (cap_result == capability_yes) {
*has = TRUE;
} else if (cap_result == capability_no) {
*has = FALSE;
} else if (cap_result == NULL) {
return svn_error_createf
(SVN_ERR_UNKNOWN_CAPABILITY, NULL,
_("Don't know anything about capability '%s'"), capability);
} else {
return svn_error_createf
(SVN_ERR_RA_DAV_OPTIONS_REQ_FAILED, NULL,
_("Attempt to fetch capability '%s' resulted in '%s'"),
capability, cap_result);
}
return SVN_NO_ERROR;
}
static svn_error_t *
parse_url(ne_uri *uri, const char *url) {
if (ne_uri_parse(url, uri)
|| uri->host == NULL || uri->path == NULL || uri->scheme == NULL) {
ne_uri_free(uri);
return svn_error_create(SVN_ERR_RA_ILLEGAL_URL, NULL,
_("Malformed URL for repository"));
}
if (uri->port == 0)
uri->port = ne_uri_defaultport(uri->scheme);
return SVN_NO_ERROR;
}
static svn_error_t *
svn_ra_neon__open(svn_ra_session_t *session,
const char *repos_URL,
const svn_ra_callbacks2_t *callbacks,
void *callback_baton,
apr_hash_t *config,
apr_pool_t *pool) {
apr_size_t len;
ne_session *sess, *sess2;
ne_uri *uri = apr_pcalloc(pool, sizeof(*uri));
svn_ra_neon__session_t *ras;
int is_ssl_session;
svn_boolean_t compression;
svn_config_t *cfg;
const char *server_group;
char *itr;
unsigned int neon_auth_types = 0;
const char *pkcs11_provider;
neonprogress_baton_t *neonprogress_baton =
apr_pcalloc(pool, sizeof(*neonprogress_baton));
const char *useragent = NULL;
const char *client_string = NULL;
if (callbacks->get_client_string)
callbacks->get_client_string(callback_baton, &client_string, pool);
if (client_string)
useragent = apr_pstrcat(pool, "SVN/" SVN_VERSION "/", client_string, NULL);
else
useragent = "SVN/" SVN_VERSION;
SVN_ERR(parse_url(uri, repos_URL));
apr_pool_cleanup_register(pool, uri, cleanup_uri, apr_pool_cleanup_null);
if (ne_sock_init() != 0)
return svn_error_create(SVN_ERR_RA_DAV_SOCK_INIT, NULL,
_("Network socket initialization failed"));
for (itr = uri->scheme; *itr; ++itr)
*itr = tolower(*itr);
is_ssl_session = (svn_cstring_casecmp(uri->scheme, "https") == 0);
if (is_ssl_session) {
if (ne_has_support(NE_FEATURE_SSL) == 0)
return svn_error_create(SVN_ERR_RA_DAV_SOCK_INIT, NULL,
_("SSL is not supported"));
}
sess = ne_session_create(uri->scheme, uri->host, uri->port);
sess2 = ne_session_create(uri->scheme, uri->host, uri->port);
apr_pool_cleanup_register(pool, sess, cleanup_session, apr_pool_cleanup_null);
apr_pool_cleanup_register(pool, sess2, cleanup_session,
apr_pool_cleanup_null);
cfg = config ? apr_hash_get(config,
SVN_CONFIG_CATEGORY_SERVERS,
APR_HASH_KEY_STRING) : NULL;
if (cfg)
server_group = svn_config_find_group(cfg, uri->host,
SVN_CONFIG_SECTION_GROUPS, pool);
else
server_group = NULL;
{
const char *proxy_host;
unsigned int proxy_port;
const char *proxy_username;
const char *proxy_password;
int timeout;
int debug;
SVN_ERR(get_server_settings(&proxy_host,
&proxy_port,
&proxy_username,
&proxy_password,
&timeout,
&debug,
&compression,
&neon_auth_types,
&pkcs11_provider,
cfg,
uri->host,
pool));
#if defined(SVN_NEON_0_26)
if (neon_auth_types == 0) {
neon_auth_types = NE_AUTH_BASIC | NE_AUTH_DIGEST;
if (is_ssl_session)
neon_auth_types |= NE_AUTH_NEGOTIATE;
}
#endif
if (debug)
ne_debug_init(stderr, debug);
if (proxy_host) {
ne_session_proxy(sess, proxy_host, proxy_port);
ne_session_proxy(sess2, proxy_host, proxy_port);
if (proxy_username) {
struct proxy_auth_baton *pab = apr_palloc(pool, sizeof(*pab));
pab->username = proxy_username;
pab->password = proxy_password ? proxy_password : "";
ne_set_proxy_auth(sess, proxy_auth, pab);
ne_set_proxy_auth(sess2, proxy_auth, pab);
}
}
if (!timeout)
timeout = DEFAULT_HTTP_TIMEOUT;
ne_set_read_timeout(sess, timeout);
ne_set_read_timeout(sess2, timeout);
#if defined(SVN_NEON_0_27)
ne_set_connect_timeout(sess, timeout);
ne_set_connect_timeout(sess2, timeout);
#endif
}
if (useragent) {
ne_set_useragent(sess, useragent);
ne_set_useragent(sess2, useragent);
} else {
ne_set_useragent(sess, "SVN/" SVN_VERSION);
ne_set_useragent(sess2, "SVN/" SVN_VERSION);
}
len = strlen(uri->path);
if (len > 1 && (uri->path)[len - 1] == '/')
(uri->path)[len - 1] = '\0';
ras = apr_pcalloc(pool, sizeof(*ras));
ras->pool = pool;
ras->url = svn_stringbuf_create(repos_URL, pool);
ras->root = *uri;
ras->ne_sess = sess;
ras->ne_sess2 = sess2;
ras->callbacks = callbacks;
ras->callback_baton = callback_baton;
ras->compression = compression;
ras->progress_baton = callbacks->progress_baton;
ras->progress_func = callbacks->progress_func;
ras->capabilities = apr_hash_make(ras->pool);
ras->vcc = NULL;
ras->uuid = NULL;
svn_auth_set_parameter(ras->callbacks->auth_baton,
SVN_AUTH_PARAM_CONFIG, cfg);
svn_auth_set_parameter(ras->callbacks->auth_baton,
SVN_AUTH_PARAM_SERVER_GROUP, server_group);
#if defined(SVN_NEON_0_26)
ne_add_server_auth(sess, neon_auth_types, request_auth, ras);
ne_add_server_auth(sess2, neon_auth_types, request_auth, ras);
#else
ne_set_server_auth(sess, request_auth, ras);
ne_set_server_auth(sess2, request_auth, ras);
#endif
if (is_ssl_session) {
const char *authorities, *trust_default_ca;
authorities = svn_config_get_server_setting(
cfg, server_group,
SVN_CONFIG_OPTION_SSL_AUTHORITY_FILES,
NULL);
if (authorities != NULL) {
char *files, *file, *last;
files = apr_pstrdup(pool, authorities);
while ((file = apr_strtok(files, ";", &last)) != NULL) {
ne_ssl_certificate *ca_cert;
files = NULL;
ca_cert = ne_ssl_cert_read(file);
if (ca_cert == NULL) {
return svn_error_createf
(SVN_ERR_RA_DAV_INVALID_CONFIG_VALUE, NULL,
_("Invalid config: unable to load certificate file '%s'"),
svn_path_local_style(file, pool));
}
ne_ssl_trust_cert(sess, ca_cert);
ne_ssl_trust_cert(sess2, ca_cert);
}
}
ne_ssl_set_verify(sess, server_ssl_callback, ras);
ne_ssl_set_verify(sess2, server_ssl_callback, ras);
#if defined(SVN_NEON_0_28)
if (pkcs11_provider) {
ne_ssl_pkcs11_provider *provider;
int rv;
rv = ne_ssl_pkcs11_provider_init(&provider, pkcs11_provider);
if (rv != NE_PK11_OK) {
return svn_error_createf
(SVN_ERR_RA_DAV_INVALID_CONFIG_VALUE, NULL,
_("Invalid config: unable to load PKCS#11 provider '%s'"),
pkcs11_provider);
}
ne_ssl_set_pkcs11_provider(sess, provider);
ne_ssl_set_pkcs11_provider(sess2, provider);
ne_ssl_pkcs11_provider_pin(provider, client_ssl_pkcs11_pin_entry,
ras);
apr_pool_cleanup_register(pool, provider, cleanup_p11provider,
apr_pool_cleanup_null);
}
else
#endif
{
ne_ssl_provide_clicert(sess, client_ssl_callback, ras);
ne_ssl_provide_clicert(sess2, client_ssl_callback, ras);
}
trust_default_ca = svn_config_get_server_setting(
cfg, server_group,
SVN_CONFIG_OPTION_SSL_TRUST_DEFAULT_CA,
"true");
if (svn_cstring_casecmp(trust_default_ca, "true") == 0) {
ne_ssl_trust_default_ca(sess);
ne_ssl_trust_default_ca(sess2);
}
}
neonprogress_baton->pool = pool;
neonprogress_baton->progress_baton = callbacks->progress_baton;
neonprogress_baton->progress_func = callbacks->progress_func;
ne_set_progress(sess, ra_neon_neonprogress, neonprogress_baton);
ne_set_progress(sess2, ra_neon_neonprogress, neonprogress_baton);
session->priv = ras;
SVN_ERR(exchange_capabilities(ras, pool));
return SVN_NO_ERROR;
}
static svn_error_t *svn_ra_neon__reparent(svn_ra_session_t *session,
const char *url,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
ne_uri *uri = apr_pcalloc(session->pool, sizeof(*uri));
SVN_ERR(parse_url(uri, url));
apr_pool_cleanup_register(session->pool, uri, cleanup_uri,
apr_pool_cleanup_null);
ras->root = *uri;
svn_stringbuf_set(ras->url, url);
return SVN_NO_ERROR;
}
static svn_error_t *svn_ra_neon__get_session_url(svn_ra_session_t *session,
const char **url,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
*url = apr_pstrmemdup(pool, ras->url->data, ras->url->len);
return SVN_NO_ERROR;
}
static svn_error_t *svn_ra_neon__get_repos_root(svn_ra_session_t *session,
const char **url,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
if (! ras->repos_root) {
svn_string_t bc_relative;
svn_stringbuf_t *url_buf;
SVN_ERR(svn_ra_neon__get_baseline_info(NULL, NULL, &bc_relative,
NULL, ras, ras->url->data,
SVN_INVALID_REVNUM, pool));
url_buf = svn_stringbuf_dup(ras->url, pool);
svn_path_remove_components
(url_buf, svn_path_component_count(bc_relative.data));
ras->repos_root = apr_pstrdup(ras->pool, url_buf->data);
}
*url = ras->repos_root;
return SVN_NO_ERROR;
}
static svn_error_t *svn_ra_neon__do_get_uuid(svn_ra_session_t *session,
const char **uuid,
apr_pool_t *pool) {
svn_ra_neon__session_t *ras = session->priv;
if (! ras->uuid) {
svn_ra_neon__resource_t *rsrc;
const char *lopped_path;
SVN_ERR(svn_ra_neon__search_for_starting_props(&rsrc, &lopped_path,
ras, ras->url->data,
pool));
SVN_ERR(svn_ra_neon__maybe_store_auth_info(ras, pool));
if (! ras->uuid) {
return svn_error_create(APR_EGENERAL, NULL,
_("The UUID property was not found on the "
"resource or any of its parents"));
}
}
*uuid = ras->uuid;
return SVN_NO_ERROR;
}
static const svn_version_t *
ra_neon_version(void) {
SVN_VERSION_BODY;
}
static const svn_ra__vtable_t neon_vtable = {
ra_neon_version,
ra_neon_get_description,
ra_neon_get_schemes,
svn_ra_neon__open,
svn_ra_neon__reparent,
svn_ra_neon__get_session_url,
svn_ra_neon__get_latest_revnum,
svn_ra_neon__get_dated_revision,
svn_ra_neon__change_rev_prop,
svn_ra_neon__rev_proplist,
svn_ra_neon__rev_prop,
svn_ra_neon__get_commit_editor,
svn_ra_neon__get_file,
svn_ra_neon__get_dir,
svn_ra_neon__get_mergeinfo,
svn_ra_neon__do_update,
svn_ra_neon__do_switch,
svn_ra_neon__do_status,
svn_ra_neon__do_diff,
svn_ra_neon__get_log,
svn_ra_neon__do_check_path,
svn_ra_neon__do_stat,
svn_ra_neon__do_get_uuid,
svn_ra_neon__get_repos_root,
svn_ra_neon__get_locations,
svn_ra_neon__get_location_segments,
svn_ra_neon__get_file_revs,
svn_ra_neon__lock,
svn_ra_neon__unlock,
svn_ra_neon__get_lock,
svn_ra_neon__get_locks,
svn_ra_neon__replay,
svn_ra_neon__has_capability,
svn_ra_neon__replay_range
};
svn_error_t *
svn_ra_neon__init(const svn_version_t *loader_version,
const svn_ra__vtable_t **vtable,
apr_pool_t *pool) {
static const svn_version_checklist_t checklist[] = {
{ "svn_subr", svn_subr_version },
{ "svn_delta", svn_delta_version },
{ NULL, NULL }
};
SVN_ERR(svn_ver_check_list(ra_neon_version(), checklist));
if (loader_version->major != SVN_VER_MAJOR) {
return svn_error_createf
(SVN_ERR_VERSION_MISMATCH, NULL,
_("Unsupported RA loader version (%d) for ra_neon"),
loader_version->major);
}
*vtable = &neon_vtable;
return SVN_NO_ERROR;
}
#define NAME "ra_neon"
#define DESCRIPTION RA_NEON_DESCRIPTION
#define VTBL neon_vtable
#define INITFUNC svn_ra_neon__init
#define COMPAT_INITFUNC svn_ra_dav_init
#include "../libsvn_ra/wrapper_template.h"