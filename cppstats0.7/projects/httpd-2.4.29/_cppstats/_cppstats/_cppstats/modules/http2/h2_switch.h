#if !defined(__mod_h2__h2_switch__)
#define __mod_h2__h2_switch__
apr_status_t h2_switch_init(apr_pool_t *pool, server_rec *s);
void h2_switch_register_hooks(void);
#endif
