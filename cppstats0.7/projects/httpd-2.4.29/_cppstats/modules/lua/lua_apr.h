#if !defined(_LUA_APR_H_)
#define _LUA_APR_H_
#include "scoreboard.h"
#include "http_main.h"
#include "ap_mpm.h"
#include "apr_md5.h"
#include "apr_sha1.h"
#include "apr_poll.h"
#include "apr.h"
#include "apr_tables.h"
#include "apr_base64.h"
int ap_lua_init(lua_State *L, apr_pool_t * p);
req_table_t *ap_lua_check_apr_table(lua_State *L, int index);
void ap_lua_push_apr_table(lua_State *L, req_table_t *t);
#endif