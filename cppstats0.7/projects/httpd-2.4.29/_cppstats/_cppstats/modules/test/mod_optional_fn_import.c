#include "httpd.h"
#include "http_config.h"
#include "mod_optional_fn_export.h"
#include "http_protocol.h"
static APR_OPTIONAL_FN_TYPE(TestOptionalFn) *pfn;
static int ImportLogTransaction(request_rec *r) {
if (pfn)
return pfn(r->the_request);
return DECLINED;
}
static void ImportFnRetrieve(void) {
pfn = APR_RETRIEVE_OPTIONAL_FN(TestOptionalFn);
}
static void ImportRegisterHooks(apr_pool_t *p) {
ap_hook_log_transaction(ImportLogTransaction,NULL,NULL,APR_HOOK_MIDDLE);
ap_hook_optional_fn_retrieve(ImportFnRetrieve,NULL,NULL,APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(optional_fn_import) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
NULL,
ImportRegisterHooks
};