#if !defined(__mod_h2__h2_alt_svc__)
#define __mod_h2__h2_alt_svc__
typedef struct h2_alt_svc h2_alt_svc;
struct h2_alt_svc {
const char *alpn;
const char *host;
int port;
};
void h2_alt_svc_register_hooks(void);
h2_alt_svc *h2_alt_svc_parse(const char *s, apr_pool_t *pool);
#endif
