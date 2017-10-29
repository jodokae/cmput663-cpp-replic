#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "httpd.h"
#include "apr_thread_rwlock.h"
#include "apr_strings.h"
#include "apr_tables.h"
#include "apr_hash.h"
#include "apr_buckets.h"
#include "apr_file_info.h"
#include "apr_time.h"
#include "apr_pools.h"
#include "apr_reslist.h"
#if !defined(VMPREP_H)
#define VMPREP_H
#define AP_LUA_SCOPE_UNSET 0
#define AP_LUA_SCOPE_ONCE 1
#define AP_LUA_SCOPE_REQUEST 2
#define AP_LUA_SCOPE_CONN 3
#define AP_LUA_SCOPE_THREAD 4
#define AP_LUA_SCOPE_SERVER 5
#define AP_LUA_CACHE_UNSET 0
#define AP_LUA_CACHE_NEVER 1
#define AP_LUA_CACHE_STAT 2
#define AP_LUA_CACHE_FOREVER 3
#define AP_LUA_FILTER_INPUT 1
#define AP_LUA_FILTER_OUTPUT 2
typedef void (*ap_lua_state_open_callback) (lua_State *L, apr_pool_t *p,
void *ctx);
typedef struct {
apr_array_header_t *package_paths;
apr_array_header_t *package_cpaths;
const char *file;
int scope;
unsigned int vm_min;
unsigned int vm_max;
ap_lua_state_open_callback cb;
void* cb_arg;
apr_pool_t *pool;
const char *bytecode;
apr_size_t bytecode_len;
int codecache;
} ap_lua_vm_spec;
typedef struct {
const char *function_name;
const char *file_name;
int scope;
ap_regex_t *uri_pattern;
const char *bytecode;
apr_size_t bytecode_len;
int codecache;
} ap_lua_mapped_handler_spec;
typedef struct {
const char *function_name;
const char *file_name;
const char* filter_name;
int direction;
} ap_lua_filter_handler_spec;
typedef struct {
apr_size_t runs;
apr_time_t modified;
apr_off_t size;
} ap_lua_finfo;
typedef struct {
lua_State* L;
ap_lua_finfo* finfo;
} ap_lua_server_spec;
void ap_lua_load_apache2_lmodule(lua_State *L);
lua_State *ap_lua_get_lua_state(apr_pool_t *lifecycle_pool,
ap_lua_vm_spec *spec, request_rec* r);
#if APR_HAS_THREADS || defined(DOXYGEN)
void ap_lua_init_mutex(apr_pool_t *pool, server_rec *s);
#endif
#endif
