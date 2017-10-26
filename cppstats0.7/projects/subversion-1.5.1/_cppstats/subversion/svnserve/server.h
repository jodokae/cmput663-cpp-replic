#if !defined(SERVER_H)
#define SERVER_H
#include <apr_network_io.h>
#if defined(__cplusplus)
extern "C" {
#endif
#include "svn_repos.h"
typedef struct server_baton_t {
svn_repos_t *repos;
svn_fs_t *fs;
svn_config_t *cfg;
svn_config_t *pwdb;
svn_authz_t *authzdb;
const char *authz_repos_name;
const char *realm;
const char *repos_url;
svn_stringbuf_t *fs_path;
const char *user;
svn_boolean_t tunnel;
const char *tunnel_user;
svn_boolean_t read_only;
#if defined(SVN_HAVE_SASL)
svn_boolean_t use_sasl;
#endif
apr_pool_t *pool;
} server_baton_t;
enum authn_type { UNAUTHENTICATED, AUTHENTICATED };
enum access_type { NO_ACCESS, READ_ACCESS, WRITE_ACCESS };
enum access_type get_access(server_baton_t *b, enum authn_type auth);
typedef struct serve_params_t {
const char *root;
svn_boolean_t tunnel;
const char *tunnel_user;
svn_boolean_t read_only;
svn_config_t *cfg;
svn_config_t *pwdb;
svn_authz_t *authzdb;
} serve_params_t;
svn_error_t *serve(svn_ra_svn_conn_t *conn, serve_params_t *params,
apr_pool_t *pool);
svn_error_t *load_configs(svn_config_t **cfg,
svn_config_t **pwdb,
svn_authz_t **authzdb,
const char *filename,
svn_boolean_t must_exist,
const char *base,
apr_pool_t *pool);
svn_error_t *cyrus_init(apr_pool_t *pool);
svn_error_t *cyrus_auth_request(svn_ra_svn_conn_t *conn,
apr_pool_t *pool,
server_baton_t *b,
enum access_type required,
svn_boolean_t needs_username);
#if defined(__cplusplus)
}
#endif
#endif
