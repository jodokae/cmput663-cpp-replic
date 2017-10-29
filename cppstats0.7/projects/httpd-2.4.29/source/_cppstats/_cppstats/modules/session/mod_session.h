#if !defined(MOD_SESSION_H)
#define MOD_SESSION_H
#if !defined(WIN32)
#define SESSION_DECLARE(type) type
#define SESSION_DECLARE_NONSTD(type) type
#define SESSION_DECLARE_DATA
#elif defined(SESSION_DECLARE_STATIC)
#define SESSION_DECLARE(type) type __stdcall
#define SESSION_DECLARE_NONSTD(type) type
#define SESSION_DECLARE_DATA
#elif defined(SESSION_DECLARE_EXPORT)
#define SESSION_DECLARE(type) __declspec(dllexport) type __stdcall
#define SESSION_DECLARE_NONSTD(type) __declspec(dllexport) type
#define SESSION_DECLARE_DATA __declspec(dllexport)
#else
#define SESSION_DECLARE(type) __declspec(dllimport) type __stdcall
#define SESSION_DECLARE_NONSTD(type) __declspec(dllimport) type
#define SESSION_DECLARE_DATA __declspec(dllimport)
#endif
#include "apr_hooks.h"
#include "apr_optional.h"
#include "apr_tables.h"
#include "apr_uuid.h"
#include "apr_pools.h"
#include "apr_time.h"
#include "httpd.h"
#include "http_config.h"
#include "ap_config.h"
#define MOD_SESSION_NOTES_KEY "mod_session_key"
#define MOD_SESSION_USER "user"
#define MOD_SESSION_PW "pw"
typedef struct {
apr_pool_t *pool;
apr_uuid_t *uuid;
const char *remote_user;
apr_table_t *entries;
const char *encoded;
apr_time_t expiry;
long maxage;
int dirty;
int cached;
int written;
} session_rec;
typedef struct {
int enabled;
int enabled_set;
long maxage;
int maxage_set;
const char *header;
int header_set;
int env;
int env_set;
apr_array_header_t *includes;
apr_array_header_t *excludes;
} session_dir_conf;
APR_DECLARE_EXTERNAL_HOOK(ap, SESSION, apr_status_t, session_load,
(request_rec * r, session_rec ** z))
APR_DECLARE_EXTERNAL_HOOK(ap, SESSION, apr_status_t, session_save,
(request_rec * r, session_rec * z))
APR_DECLARE_EXTERNAL_HOOK(ap, SESSION, apr_status_t, session_encode,
(request_rec * r, session_rec * z))
APR_DECLARE_EXTERNAL_HOOK(ap, SESSION, apr_status_t, session_decode,
(request_rec * r, session_rec * z))
APR_DECLARE_OPTIONAL_FN(
apr_status_t,
ap_session_get,
(request_rec * r, session_rec * z, const char *key, const char **value));
APR_DECLARE_OPTIONAL_FN(apr_status_t, ap_session_set,
(request_rec * r, session_rec * z, const char *key, const char *value));
APR_DECLARE_OPTIONAL_FN(apr_status_t, ap_session_load,
(request_rec *, session_rec **));
APR_DECLARE_OPTIONAL_FN(apr_status_t, ap_session_save,
(request_rec *, session_rec *));
extern module AP_MODULE_DECLARE_DATA session_module;
#endif