#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "mod_optional_fn_export.h"
static int TestOptionalFn(const char *szStr) {
ap_log_error(APLOG_MARK,APLOG_ERR,OK,NULL, APLOGNO(01871)
"Optional function test said: %s",szStr);
return OK;
}
static void ExportRegisterHooks(apr_pool_t *p) {
APR_REGISTER_OPTIONAL_FN(TestOptionalFn);
}
AP_DECLARE_MODULE(optional_fn_export) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
ExportRegisterHooks
};