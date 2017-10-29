#include "httpd.h"
#include "http_connection.h"
static apr_status_t eoc_bucket_read(apr_bucket *b, const char **str,
apr_size_t *len, apr_read_type_e block) {
*str = NULL;
*len = 0;
return APR_SUCCESS;
}
AP_DECLARE(apr_bucket *) ap_bucket_eoc_make(apr_bucket *b) {
b->length = 0;
b->start = 0;
b->data = NULL;
b->type = &ap_bucket_type_eoc;
return b;
}
AP_DECLARE(apr_bucket *) ap_bucket_eoc_create(apr_bucket_alloc_t *list) {
apr_bucket *b = apr_bucket_alloc(sizeof(*b), list);
APR_BUCKET_INIT(b);
b->free = apr_bucket_free;
b->list = list;
return ap_bucket_eoc_make(b);
}
AP_DECLARE_DATA const apr_bucket_type_t ap_bucket_type_eoc = {
"EOC", 5, APR_BUCKET_METADATA,
apr_bucket_destroy_noop,
eoc_bucket_read,
apr_bucket_setaside_noop,
apr_bucket_split_notimpl,
apr_bucket_simple_copy
};