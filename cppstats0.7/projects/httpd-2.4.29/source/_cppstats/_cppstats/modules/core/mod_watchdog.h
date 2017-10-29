#if !defined(MOD_WATCHDOG_H)
#define MOD_WATCHDOG_H
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "ap_provider.h"
#include "apr.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "apr_shm.h"
#include "apr_hash.h"
#include "apr_hooks.h"
#include "apr_optional.h"
#include "apr_file_io.h"
#include "apr_time.h"
#include "apr_thread_proc.h"
#include "apr_global_mutex.h"
#include "apr_thread_mutex.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define AP_WATCHDOG_SINGLETON "_singleton_"
#define AP_WATCHDOG_DEFAULT "_default_"
#define AP_WD_TM_INTERVAL APR_TIME_C(1000000)
#define AP_WD_TM_SLICE APR_TIME_C(100000)
#define AP_WATCHDOG_STATE_STARTING 1
#define AP_WATCHDOG_STATE_RUNNING 2
#define AP_WATCHDOG_STATE_STOPPING 3
typedef struct ap_watchdog_t ap_watchdog_t;
#if !defined(AP_WD_DECLARE)
#if !defined(WIN32)
#define AP_WD_DECLARE(type) type
#define AP_WD_DECLARE_NONSTD(type) type
#define AP_WD_DECLARE_DATA
#elif defined(AP_WD_DECLARE_STATIC)
#define AP_WD_DECLARE(type) type __stdcall
#define AP_WD_DECLARE_NONSTD(type) type
#define AP_WD_DECLARE_DATA
#elif defined(AP_WD_DECLARE_EXPORT)
#define AP_WD_DECLARE(type) __declspec(dllexport) type __stdcall
#define AP_WD_DECLARE_NONSTD(type) __declspec(dllexport) type
#define AP_WD_DECLARE_DATA __declspec(dllexport)
#else
#define AP_WD_DECLARE(type) __declspec(dllimport) type __stdcall
#define AP_WD_DECLARE_NONSTD(type) __declspec(dllimport) type
#define AP_WD_DECLARE_DATA __declspec(dllimport)
#endif
#endif
typedef apr_status_t ap_watchdog_callback_fn_t(int state, void *data,
apr_pool_t *pool);
APR_DECLARE_OPTIONAL_FN(apr_status_t, ap_watchdog_get_instance,
(ap_watchdog_t **watchdog, const char *name, int parent,
int singleton, apr_pool_t *p));
APR_DECLARE_OPTIONAL_FN(apr_status_t, ap_watchdog_register_callback,
(ap_watchdog_t *watchdog, apr_interval_time_t interval,
const void *data, ap_watchdog_callback_fn_t *callback));
APR_DECLARE_OPTIONAL_FN(apr_status_t, ap_watchdog_set_callback_interval,
(ap_watchdog_t *w, apr_interval_time_t interval,
const void *data, ap_watchdog_callback_fn_t *callback));
APR_DECLARE_EXTERNAL_HOOK(ap, AP_WD, int, watchdog_need, (server_rec *s,
const char *name,
int parent, int singleton))
APR_DECLARE_EXTERNAL_HOOK(ap, AP_WD, int, watchdog_init, (
server_rec *s,
const char *name,
apr_pool_t *pool))
APR_DECLARE_EXTERNAL_HOOK(ap, AP_WD, int, watchdog_exit, (
server_rec *s,
const char *name,
apr_pool_t *pool))
APR_DECLARE_EXTERNAL_HOOK(ap, AP_WD, int, watchdog_step, (
server_rec *s,
const char *name,
apr_pool_t *pool))
#if defined(__cplusplus)
}
#endif
#endif
