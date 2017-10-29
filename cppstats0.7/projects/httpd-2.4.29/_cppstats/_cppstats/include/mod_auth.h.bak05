#if !defined(APACHE_MOD_AUTH_H)
#define APACHE_MOD_AUTH_H
#include "apr_pools.h"
#include "apr_hash.h"
#include "apr_optional.h"
#include "httpd.h"
#include "http_config.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define AUTHN_PROVIDER_GROUP "authn"
#define AUTHZ_PROVIDER_GROUP "authz"
#define AUTHN_PROVIDER_VERSION "0"
#define AUTHZ_PROVIDER_VERSION "0"
#define AUTHN_DEFAULT_PROVIDER "file"
#define AUTHN_PROVIDER_NAME_NOTE "authn_provider_name"
#define AUTHZ_PROVIDER_NAME_NOTE "authz_provider_name"
#define AUTHN_PREFIX "AUTHENTICATE_"
#define AUTHZ_PREFIX "AUTHORIZE_"
#if !defined(SATISFY_ALL)
#define SATISFY_ALL 0
#endif
#if !defined(SATISFY_ANY)
#define SATISFY_ANY 1
#endif
#if !defined(SATISFY_NOSPEC)
#define SATISFY_NOSPEC 2
#endif
typedef enum {
AUTH_DENIED,
AUTH_GRANTED,
AUTH_USER_FOUND,
AUTH_USER_NOT_FOUND,
AUTH_GENERAL_ERROR
} authn_status;
typedef enum {
AUTHZ_DENIED,
AUTHZ_GRANTED,
AUTHZ_NEUTRAL,
AUTHZ_GENERAL_ERROR,
AUTHZ_DENIED_NO_USER
} authz_status;
typedef struct {
authn_status (*check_password)(request_rec *r, const char *user,
const char *password);
authn_status (*get_realm_hash)(request_rec *r, const char *user,
const char *realm, char **rethash);
} authn_provider;
typedef struct authn_provider_list authn_provider_list;
struct authn_provider_list {
const char *provider_name;
const authn_provider *provider;
authn_provider_list *next;
};
typedef struct {
authz_status (*check_authorization)(request_rec *r,
const char *require_line,
const void *parsed_require_line);
const char *(*parse_require_line)(cmd_parms *cmd, const char *require_line,
const void **parsed_require_line);
} authz_provider;
APR_DECLARE_OPTIONAL_FN(void, ap_authn_cache_store,
(request_rec*, const char*, const char*,
const char*, const char*));
#if defined(__cplusplus)
}
#endif
#endif
