#include "mod_lua.h"
#if !defined(_APL_CONFIG_H_)
#define _APL_CONFIG_H_
void ap_lua_load_config_lmodule(lua_State *L);
apr_status_t ap_lua_map_handler(ap_lua_dir_cfg *cfg,
const char *file,
const char *function,
const char *pattern,
const char *scope);
#endif