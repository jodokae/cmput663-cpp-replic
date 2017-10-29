#if !defined(PROXY_UTIL_H_)
#define PROXY_UTIL_H_
PROXY_DECLARE(int) ap_proxy_is_ipaddr(struct dirconn_entry *This, apr_pool_t *p);
PROXY_DECLARE(int) ap_proxy_is_domainname(struct dirconn_entry *This, apr_pool_t *p);
PROXY_DECLARE(int) ap_proxy_is_hostname(struct dirconn_entry *This, apr_pool_t *p);
PROXY_DECLARE(int) ap_proxy_is_word(struct dirconn_entry *This, apr_pool_t *p);
PROXY_DECLARE_DATA extern int proxy_lb_workers;
PROXY_DECLARE_DATA extern const apr_strmatch_pattern *ap_proxy_strmatch_path;
PROXY_DECLARE_DATA extern const apr_strmatch_pattern *ap_proxy_strmatch_domain;
void proxy_util_register_hooks(apr_pool_t *p);
#endif