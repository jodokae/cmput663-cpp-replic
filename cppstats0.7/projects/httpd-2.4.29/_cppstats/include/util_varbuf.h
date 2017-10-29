#if !defined(AP_VARBUF_H)
#define AP_VARBUF_H
#include "apr.h"
#include "apr_allocator.h"
#include "httpd.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define AP_VARBUF_UNKNOWN APR_SIZE_MAX
struct ap_varbuf_info;
struct ap_varbuf {
char *buf;
apr_size_t avail;
apr_size_t strlen;
apr_pool_t *pool;
struct ap_varbuf_info *info;
};
AP_DECLARE(void) ap_varbuf_init(apr_pool_t *pool, struct ap_varbuf *vb,
apr_size_t init_size);
AP_DECLARE(void) ap_varbuf_grow(struct ap_varbuf *vb, apr_size_t new_size);
AP_DECLARE(void) ap_varbuf_free(struct ap_varbuf *vb);
AP_DECLARE(void) ap_varbuf_strmemcat(struct ap_varbuf *vb, const char *str,
int len);
AP_DECLARE(char *) ap_varbuf_pdup(apr_pool_t *p, struct ap_varbuf *vb,
const char *prepend, apr_size_t prepend_len,
const char *append, apr_size_t append_len,
apr_size_t *new_len);
#define ap_varbuf_strcat(vb, str) ap_varbuf_strmemcat(vb, str, strlen(str))
AP_DECLARE(apr_status_t) ap_varbuf_regsub(struct ap_varbuf *vb,
const char *input,
const char *source,
apr_size_t nmatch,
ap_regmatch_t pmatch[],
apr_size_t maxlen);
AP_DECLARE(apr_status_t) ap_varbuf_cfg_getline(struct ap_varbuf *vb,
ap_configfile_t *cfp,
apr_size_t max_len);
#if defined(__cplusplus)
}
#endif
#endif
