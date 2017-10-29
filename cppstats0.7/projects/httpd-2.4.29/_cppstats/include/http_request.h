#if !defined(APACHE_HTTP_REQUEST_H)
#define APACHE_HTTP_REQUEST_H
#include "apr_optional.h"
#include "util_filter.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define AP_SUBREQ_NO_ARGS 0
#define AP_SUBREQ_MERGE_ARGS 1
AP_DECLARE(int) ap_process_request_internal(request_rec *r);
AP_DECLARE(request_rec *) ap_sub_req_lookup_uri(const char *new_uri,
const request_rec *r,
ap_filter_t *next_filter);
AP_DECLARE(request_rec *) ap_sub_req_lookup_file(const char *new_file,
const request_rec *r,
ap_filter_t *next_filter);
AP_DECLARE(request_rec *) ap_sub_req_lookup_dirent(const apr_finfo_t *finfo,
const request_rec *r,
int subtype,
ap_filter_t *next_filter);
AP_DECLARE(request_rec *) ap_sub_req_method_uri(const char *method,
const char *new_uri,
const request_rec *r,
ap_filter_t *next_filter);
AP_CORE_DECLARE_NONSTD(apr_status_t) ap_sub_req_output_filter(ap_filter_t *f,
apr_bucket_brigade *bb);
AP_DECLARE(int) ap_run_sub_req(request_rec *r);
AP_DECLARE(void) ap_destroy_sub_req(request_rec *r);
AP_DECLARE(void) ap_internal_redirect(const char *new_uri, request_rec *r);
AP_DECLARE(void) ap_internal_redirect_handler(const char *new_uri, request_rec *r);
AP_DECLARE(void) ap_internal_fast_redirect(request_rec *sub_req, request_rec *r);
AP_DECLARE(int) ap_some_auth_required(request_rec *r);
#define AP_AUTH_INTERNAL_PER_URI 0
#define AP_AUTH_INTERNAL_PER_CONF 1
#define AP_AUTH_INTERNAL_MASK 0x000F
AP_DECLARE(void) ap_clear_auth_internal(void);
AP_DECLARE(void) ap_setup_auth_internal(apr_pool_t *ptemp);
AP_DECLARE(apr_status_t) ap_register_auth_provider(apr_pool_t *pool,
const char *provider_group,
const char *provider_name,
const char *provider_version,
const void *provider,
int type);
APR_DECLARE_OPTIONAL_FN(apr_array_header_t *, authn_ap_list_provider_names,
(apr_pool_t *ptemp));
APR_DECLARE_OPTIONAL_FN(apr_array_header_t *, authz_ap_list_provider_names,
(apr_pool_t *ptemp));
AP_DECLARE(int) ap_is_initial_req(request_rec *r);
AP_DECLARE(void) ap_update_mtime(request_rec *r, apr_time_t dependency_mtime);
AP_DECLARE(void) ap_allow_methods(request_rec *r, int reset, ...)
AP_FN_ATTR_SENTINEL;
AP_DECLARE(void) ap_allow_standard_methods(request_rec *r, int reset, ...);
#define MERGE_ALLOW 0
#define REPLACE_ALLOW 1
AP_DECLARE(void) ap_process_request(request_rec *r);
AP_DECLARE(void) ap_process_request_after_handler(request_rec *r);
void ap_process_async_request(request_rec *r);
AP_DECLARE(void) ap_die(int type, request_rec *r);
AP_DECLARE(apr_status_t) ap_check_pipeline(conn_rec *c, apr_bucket_brigade *bb,
unsigned int max_blank_lines);
AP_DECLARE_HOOK(int,create_request,(request_rec *r))
AP_DECLARE_HOOK(int,translate_name,(request_rec *r))
AP_DECLARE_HOOK(int,map_to_storage,(request_rec *r))
AP_DECLARE_HOOK(int,check_user_id,(request_rec *r))
AP_DECLARE_HOOK(int,fixups,(request_rec *r))
AP_DECLARE_HOOK(int,type_checker,(request_rec *r))
AP_DECLARE_HOOK(int,access_checker,(request_rec *r))
AP_DECLARE_HOOK(int,access_checker_ex,(request_rec *r))
AP_DECLARE_HOOK(int,auth_checker,(request_rec *r))
AP_DECLARE(void) ap_hook_check_access(ap_HOOK_access_checker_t *pf,
const char * const *aszPre,
const char * const *aszSucc,
int nOrder, int type);
AP_DECLARE(void) ap_hook_check_access_ex(ap_HOOK_access_checker_ex_t *pf,
const char * const *aszPre,
const char * const *aszSucc,
int nOrder, int type);
AP_DECLARE(void) ap_hook_check_authn(ap_HOOK_check_user_id_t *pf,
const char * const *aszPre,
const char * const *aszSucc,
int nOrder, int type);
AP_DECLARE(void) ap_hook_check_authz(ap_HOOK_auth_checker_t *pf,
const char * const *aszPre,
const char * const *aszSucc,
int nOrder, int type);
AP_DECLARE_HOOK(void,insert_filter,(request_rec *r))
AP_DECLARE_HOOK(int,post_perdir_config,(request_rec *r))
AP_DECLARE_HOOK(int,force_authn,(request_rec *r))
AP_DECLARE_HOOK(apr_status_t,dirwalk_stat,(apr_finfo_t *finfo, request_rec *r, apr_int32_t wanted))
AP_DECLARE(int) ap_location_walk(request_rec *r);
AP_DECLARE(int) ap_directory_walk(request_rec *r);
AP_DECLARE(int) ap_file_walk(request_rec *r);
AP_DECLARE(int) ap_if_walk(request_rec *r);
AP_DECLARE_DATA extern const apr_bucket_type_t ap_bucket_type_eor;
#define AP_BUCKET_IS_EOR(e) (e->type == &ap_bucket_type_eor)
AP_DECLARE(apr_bucket *) ap_bucket_eor_make(apr_bucket *b, request_rec *r);
AP_DECLARE(apr_bucket *) ap_bucket_eor_create(apr_bucket_alloc_t *list,
request_rec *r);
AP_DECLARE(int) ap_some_authn_required(request_rec *r);
#if defined(__cplusplus)
}
#endif
#endif
