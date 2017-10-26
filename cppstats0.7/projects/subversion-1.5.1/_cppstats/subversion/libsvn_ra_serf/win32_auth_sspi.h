#if !defined(SVN_LIBSVN_RA_SERF_WIN32_AUTH_SSPI_H)
#define SVN_LIBSVN_RA_SERF_WIN32_AUTH_SSPI_H
#if defined(SVN_RA_SERF_SSPI_ENABLED)
#if !defined(__SSPI_H__)
#define SECURITY_WIN32
#include <sspi.h>
#endif
#include "svn_error.h"
#include "ra_serf.h"
typedef enum {
sspi_auth_not_started,
sspi_auth_in_progress,
sspi_auth_completed,
} sspi_auth_state;
struct serf_sspi_context_t {
CtxtHandle ctx;
sspi_auth_state state;
};
svn_error_t *
handle_sspi_auth(svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
serf_request_t *request,
serf_bucket_t *response,
char *auth_hdr,
char *auth_attr,
apr_pool_t *pool);
svn_error_t *
init_sspi_connection(svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
apr_pool_t *pool);
svn_error_t *
setup_request_sspi_auth(svn_ra_serf__connection_t *conn,
serf_bucket_t *hdrs_bkt);
svn_error_t *
sspi_get_credentials(char *token, apr_size_t token_len, const char **buf,
apr_size_t *buf_len, serf_sspi_context_t *sspi_ctx);
#endif
#endif
