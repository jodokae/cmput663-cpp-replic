#if !defined(DBD_H)
#define DBD_H
#if !defined(WIN32)
#define DBD_DECLARE(type) type
#define DBD_DECLARE_NONSTD(type) type
#define DBD_DECLARE_DATA
#elif defined(DBD_DECLARE_STATIC)
#define DBD_DECLARE(type) type __stdcall
#define DBD_DECLARE_NONSTD(type) type
#define DBD_DECLARE_DATA
#elif defined(DBD_DECLARE_EXPORT)
#define DBD_DECLARE(type) __declspec(dllexport) type __stdcall
#define DBD_DECLARE_NONSTD(type) __declspec(dllexport) type
#define DBD_DECLARE_DATA __declspec(dllexport)
#else
#define DBD_DECLARE(type) __declspec(dllimport) type __stdcall
#define DBD_DECLARE_NONSTD(type) __declspec(dllimport) type
#define DBD_DECLARE_DATA __declspec(dllimport)
#endif
#include <httpd.h>
#include <apr_optional.h>
#include <apr_hash.h>
#include <apr_hooks.h>
typedef struct {
server_rec *server;
const char *name;
const char *params;
int persist;
#if APR_HAS_THREADS
int nmin;
int nkeep;
int nmax;
int exptime;
int set;
#endif
apr_hash_t *queries;
apr_array_header_t *init_queries;
} dbd_cfg_t;
typedef struct {
apr_dbd_t *handle;
const apr_dbd_driver_t *driver;
apr_hash_t *prepared;
apr_pool_t *pool;
} ap_dbd_t;
DBD_DECLARE_NONSTD(ap_dbd_t*) ap_dbd_open(apr_pool_t*, server_rec*);
DBD_DECLARE_NONSTD(void) ap_dbd_close(server_rec*, ap_dbd_t*);
DBD_DECLARE_NONSTD(ap_dbd_t*) ap_dbd_acquire(request_rec*);
DBD_DECLARE_NONSTD(ap_dbd_t*) ap_dbd_cacquire(conn_rec*);
DBD_DECLARE_NONSTD(void) ap_dbd_prepare(server_rec*, const char*, const char*);
APR_DECLARE_OPTIONAL_FN(ap_dbd_t*, ap_dbd_open, (apr_pool_t*, server_rec*));
APR_DECLARE_OPTIONAL_FN(void, ap_dbd_close, (server_rec*, ap_dbd_t*));
APR_DECLARE_OPTIONAL_FN(ap_dbd_t*, ap_dbd_acquire, (request_rec*));
APR_DECLARE_OPTIONAL_FN(ap_dbd_t*, ap_dbd_cacquire, (conn_rec*));
APR_DECLARE_OPTIONAL_FN(void, ap_dbd_prepare, (server_rec*, const char*, const char*));
APR_DECLARE_EXTERNAL_HOOK(dbd, DBD, apr_status_t, post_connect,
(apr_pool_t *, dbd_cfg_t *, ap_dbd_t *))
#endif
