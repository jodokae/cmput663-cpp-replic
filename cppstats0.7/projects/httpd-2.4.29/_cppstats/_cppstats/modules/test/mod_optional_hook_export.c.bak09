#include "httpd.h"
#include "http_config.h"
#include "mod_optional_hook_export.h"
#include "http_protocol.h"
AP_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(int,optional_hook_test,(const char *szStr),
(szStr),OK,DECLINED)
static int ExportLogTransaction(request_rec *r) {
return ap_run_optional_hook_test(r->the_request);
}
static void ExportRegisterHooks(apr_pool_t *p) {
ap_hook_log_transaction(ExportLogTransaction,NULL,NULL,APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(optional_hook_export) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
ExportRegisterHooks
};
