#if !defined(UTIL_MUTEX_H)
#define UTIL_MUTEX_H
#include "httpd.h"
#include "http_config.h"
#include "apr_global_mutex.h"
#if APR_HAS_FLOCK_SERIALIZE
#define AP_LIST_FLOCK_SERIALIZE ", 'flock:/path/to/file'"
#else
#define AP_LIST_FLOCK_SERIALIZE
#endif
#if APR_HAS_FCNTL_SERIALIZE
#define AP_LIST_FCNTL_SERIALIZE ", 'fcntl:/path/to/file'"
#else
#define AP_LIST_FCNTL_SERIALIZE
#endif
#if APR_HAS_SYSVSEM_SERIALIZE
#define AP_LIST_SYSVSEM_SERIALIZE ", 'sysvsem'"
#else
#define AP_LIST_SYSVSEM_SERIALIZE
#endif
#if APR_HAS_POSIXSEM_SERIALIZE
#define AP_LIST_POSIXSEM_SERIALIZE ", 'posixsem'"
#else
#define AP_LIST_POSIXSEM_SERIALIZE
#endif
#if APR_HAS_PROC_PTHREAD_SERIALIZE
#define AP_LIST_PTHREAD_SERIALIZE ", 'pthread'"
#else
#define AP_LIST_PTHREAD_SERIALIZE
#endif
#if APR_HAS_FLOCK_SERIALIZE || APR_HAS_FCNTL_SERIALIZE
#define AP_LIST_FILE_SERIALIZE ", 'file:/path/to/file'"
#else
#define AP_LIST_FILE_SERIALIZE
#endif
#if APR_HAS_SYSVSEM_SERIALIZE || APR_HAS_POSIXSEM_SERIALIZE
#define AP_LIST_SEM_SERIALIZE ", 'sem'"
#else
#define AP_LIST_SEM_SERIALIZE
#endif
#define AP_ALL_AVAILABLE_MUTEXES_STRING "Mutex mechanisms are: 'none', 'default'" AP_LIST_FLOCK_SERIALIZE AP_LIST_FCNTL_SERIALIZE AP_LIST_FILE_SERIALIZE AP_LIST_PTHREAD_SERIALIZE AP_LIST_SYSVSEM_SERIALIZE AP_LIST_POSIXSEM_SERIALIZE AP_LIST_SEM_SERIALIZE
#define AP_AVAILABLE_MUTEXES_STRING "Mutex mechanisms are: 'default'" AP_LIST_FLOCK_SERIALIZE AP_LIST_FCNTL_SERIALIZE AP_LIST_FILE_SERIALIZE AP_LIST_PTHREAD_SERIALIZE AP_LIST_SYSVSEM_SERIALIZE AP_LIST_POSIXSEM_SERIALIZE AP_LIST_SEM_SERIALIZE
#if defined(__cplusplus)
extern "C" {
#endif
AP_DECLARE(apr_status_t) ap_parse_mutex(const char *arg, apr_pool_t *pool,
apr_lockmech_e *mutexmech,
const char **mutexfile);
AP_DECLARE_NONSTD(const char *) ap_set_mutex(cmd_parms *cmd, void *dummy,
const char *arg);
AP_DECLARE_NONSTD(void) ap_mutex_init(apr_pool_t *p);
#define AP_MUTEX_ALLOW_NONE 1
#define AP_MUTEX_DEFAULT_NONE 2
AP_DECLARE(apr_status_t) ap_mutex_register(apr_pool_t *pconf,
const char *type,
const char *default_dir,
apr_lockmech_e default_mech,
apr_int32_t options);
AP_DECLARE(apr_status_t) ap_global_mutex_create(apr_global_mutex_t **mutex,
const char **name,
const char *type,
const char *instance_id,
server_rec *server,
apr_pool_t *pool,
apr_int32_t options);
AP_DECLARE(apr_status_t) ap_proc_mutex_create(apr_proc_mutex_t **mutex,
const char **name,
const char *type,
const char *instance_id,
server_rec *server,
apr_pool_t *pool,
apr_int32_t options);
AP_CORE_DECLARE(void) ap_dump_mutexes(apr_pool_t *p, server_rec *s, apr_file_t *out);
#if defined(__cplusplus)
}
#endif
#endif
