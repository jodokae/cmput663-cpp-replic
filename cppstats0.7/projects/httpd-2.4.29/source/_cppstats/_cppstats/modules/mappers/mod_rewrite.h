#if !defined(MOD_REWRITE_H)
#define MOD_REWRITE_H 1
#include "apr_optional.h"
#include "httpd.h"
#define REWRITE_REDIRECT_HANDLER_NAME "redirect-handler"
typedef char *(rewrite_mapfunc_t)(request_rec *r, char *key);
APR_DECLARE_OPTIONAL_FN(void, ap_register_rewrite_mapfunc,
(char *name, rewrite_mapfunc_t *func));
#endif