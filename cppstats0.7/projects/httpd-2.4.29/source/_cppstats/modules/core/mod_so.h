#if !defined(MOD_SO_H)
#define MOD_SO_H 1
#include "apr_optional.h"
#include "httpd.h"
APR_DECLARE_OPTIONAL_FN(module *, ap_find_loaded_module_symbol,
(server_rec *s, const char *modname));
#endif