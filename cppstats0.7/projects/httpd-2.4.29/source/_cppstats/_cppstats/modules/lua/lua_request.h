#if !defined(_LUA_REQUEST_H_)
#define _LUA_REQUEST_H_
#include "mod_lua.h"
#include "util_varbuf.h"
void ap_lua_load_request_lmodule(lua_State *L, apr_pool_t *p);
void ap_lua_push_connection(lua_State *L, conn_rec *r);
void ap_lua_push_server(lua_State *L, server_rec *r);
void ap_lua_push_request(lua_State *L, request_rec *r);
#define APL_REQ_FUNTYPE_STRING 1
#define APL_REQ_FUNTYPE_INT 2
#define APL_REQ_FUNTYPE_TABLE 3
#define APL_REQ_FUNTYPE_LUACFUN 4
#define APL_REQ_FUNTYPE_BOOLEAN 5
typedef struct {
const void *fun;
int type;
} req_fun_t;
typedef struct {
request_rec *r;
apr_table_t *t;
const char *n;
} req_table_t;
typedef struct {
int type;
size_t size;
size_t vb_size;
lua_Number number;
struct ap_varbuf vb;
} lua_ivm_object;
#endif
