#if !defined(RA_SVN_SASL_H)
#define RA_SVN_SASL_H
#if defined(WIN32)
#define STRUCT_IOVEC_DEFINED
#include <sasl.h>
#else
#include <sasl/sasl.h>
#endif
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_RA_SVN_SASL_NAME "svn"
extern volatile svn_atomic_t svn_ra_svn__sasl_status;
void svn_ra_svn__default_secprops(sasl_security_properties_t *secprops);
apr_status_t svn_ra_svn__sasl_common_init(apr_pool_t *pool);
svn_error_t *svn_ra_svn__get_addresses(const char **local_addrport,
const char **remote_addrport,
svn_ra_svn_conn_t *conn,
apr_pool_t *pool);
svn_error_t *svn_ra_svn__enable_sasl_encryption(svn_ra_svn_conn_t *conn,
sasl_conn_t *sasl_ctx,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif