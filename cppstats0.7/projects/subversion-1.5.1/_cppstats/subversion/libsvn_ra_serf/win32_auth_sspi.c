#if defined(WIN32)
#if defined(APR_HAVE_IPV6)
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <Wspiapi.h>
#endif
#include <windows.h>
#endif
#include <string.h>
#include <apr_base64.h>
#include "svn_error.h"
#include "ra_serf.h"
#include "win32_auth_sspi.h"
#if defined(SVN_RA_SERF_SSPI_ENABLED)
HANDLE security_dll = INVALID_HANDLE_VALUE;
INIT_SECURITY_INTERFACE InitSecurityInterface_;
static PSecurityFunctionTable sspi = NULL;
static unsigned int ntlm_maxtokensize = 0;
#define SECURITY_DLL "security.dll"
static svn_error_t *
load_security_dll() {
if (security_dll != INVALID_HANDLE_VALUE)
return SVN_NO_ERROR;
security_dll = LoadLibrary(SECURITY_DLL);
if (security_dll != INVALID_HANDLE_VALUE) {
InitSecurityInterface_ =
(INIT_SECURITY_INTERFACE)GetProcAddress(security_dll,
"InitSecurityInterfaceA");
sspi = InitSecurityInterface_();
if (sspi)
return SVN_NO_ERROR;
}
if (security_dll)
FreeLibrary(security_dll);
return svn_error_createf
(SVN_ERR_RA_SERF_SSPI_INITIALISATION_FAILED, NULL,
"SSPI Initialization failed.");
}
static svn_error_t *
sspi_maxtokensize(char *auth_pkg, unsigned int *maxtokensize) {
SECURITY_STATUS status;
SecPkgInfo *sec_pkg_info = NULL;
status = sspi->QuerySecurityPackageInfo(auth_pkg,
&sec_pkg_info);
if (status == SEC_E_OK) {
*maxtokensize = sec_pkg_info->cbMaxToken;
sspi->FreeContextBuffer(sec_pkg_info);
} else
return svn_error_createf
(SVN_ERR_RA_SERF_SSPI_INITIALISATION_FAILED, NULL,
"SSPI Initialization failed.");
return SVN_NO_ERROR;
}
svn_error_t *
init_sspi_connection(svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
apr_pool_t *pool) {
const char *tmp;
apr_size_t tmp_len;
SVN_ERR(load_security_dll());
conn->sspi_context = (serf_sspi_context_t*)
apr_palloc(pool, sizeof(serf_sspi_context_t));
conn->sspi_context->ctx.dwLower = 0;
conn->sspi_context->ctx.dwUpper = 0;
conn->sspi_context->state = sspi_auth_not_started;
SVN_ERR(sspi_get_credentials(NULL, 0, &tmp, &tmp_len,
conn->sspi_context));
svn_ra_serf__encode_auth_header("NTLM", &conn->auth_value, tmp, tmp_len,
pool);
conn->auth_header = "Authorization";
serf_connection_set_max_outstanding_requests(conn->conn, 1);
return SVN_NO_ERROR;
}
svn_error_t *
handle_sspi_auth(svn_ra_serf__session_t *session,
svn_ra_serf__connection_t *conn,
serf_request_t *request,
serf_bucket_t *response,
char *auth_hdr,
char *auth_attr,
apr_pool_t *pool) {
const char *tmp;
char *base64_token, *token = NULL, *last;
apr_size_t tmp_len, token_len = 0;
base64_token = apr_strtok(auth_attr, " ", &last);
if (base64_token) {
token_len = apr_base64_decode_len(base64_token);
token = apr_palloc(pool, token_len);
apr_base64_decode(token, base64_token);
}
if (!token && conn->sspi_context->state != sspi_auth_not_started)
return SVN_NO_ERROR;
SVN_ERR(sspi_get_credentials(token, token_len, &tmp, &tmp_len,
conn->sspi_context));
svn_ra_serf__encode_auth_header(session->auth_protocol->auth_name,
&conn->auth_value, tmp, tmp_len, pool);
conn->auth_header = "Authorization";
if (conn->sspi_context->state == sspi_auth_completed)
serf_connection_set_max_outstanding_requests(conn->conn, 0);
return SVN_NO_ERROR;
}
svn_error_t *
setup_request_sspi_auth(svn_ra_serf__connection_t *conn,
serf_bucket_t *hdrs_bkt) {
if (conn->auth_header && conn->auth_value) {
serf_bucket_headers_setn(hdrs_bkt, conn->auth_header, conn->auth_value);
conn->auth_header = NULL;
conn->auth_value = NULL;
}
return SVN_NO_ERROR;
}
svn_error_t *
sspi_get_credentials(char *token, apr_size_t token_len, const char **buf,
apr_size_t *buf_len, serf_sspi_context_t *sspi_ctx) {
SecBuffer in_buf, out_buf;
SecBufferDesc in_buf_desc, out_buf_desc;
SECURITY_STATUS status;
DWORD ctx_attr;
TimeStamp expires;
CredHandle creds;
char *target = NULL;
CtxtHandle *ctx = &(sspi_ctx->ctx);
if (ntlm_maxtokensize == 0)
sspi_maxtokensize("NTLM", &ntlm_maxtokensize);
in_buf.BufferType = SECBUFFER_TOKEN;
in_buf.cbBuffer = token_len;
in_buf.pvBuffer = token;
in_buf_desc.cBuffers = 1;
in_buf_desc.ulVersion = SECBUFFER_VERSION;
in_buf_desc.pBuffers = &in_buf;
out_buf.BufferType = SECBUFFER_TOKEN;
out_buf.cbBuffer = ntlm_maxtokensize;
out_buf.pvBuffer = (char*)malloc(ntlm_maxtokensize);
out_buf_desc.cBuffers = 1;
out_buf_desc.ulVersion = SECBUFFER_VERSION;
out_buf_desc.pBuffers = &out_buf;
status = sspi->AcquireCredentialsHandle(NULL,
"NTLM",
SECPKG_CRED_OUTBOUND,
NULL, NULL,
NULL, NULL,
&creds,
&expires);
if (status != SEC_E_OK)
return svn_error_createf
(SVN_ERR_RA_SERF_SSPI_INITIALISATION_FAILED, NULL,
"SSPI Initialization failed.");
status = sspi->InitializeSecurityContext(&creds,
ctx != NULL && ctx->dwLower != 0
? ctx
: NULL,
target,
ISC_REQ_REPLAY_DETECT |
ISC_REQ_SEQUENCE_DETECT |
ISC_REQ_CONFIDENTIALITY |
ISC_REQ_DELEGATE,
0,
SECURITY_NATIVE_DREP,
&in_buf_desc,
0,
ctx,
&out_buf_desc,
&ctx_attr,
&expires);
if (status == SEC_I_COMPLETE_NEEDED
|| status == SEC_I_COMPLETE_AND_CONTINUE) {
if (sspi->CompleteAuthToken != NULL)
sspi->CompleteAuthToken(ctx, &out_buf_desc);
}
*buf = out_buf.pvBuffer;
*buf_len = out_buf.cbBuffer;
switch (status) {
case SEC_E_OK:
case SEC_I_COMPLETE_NEEDED:
sspi_ctx->state = sspi_auth_completed;
break;
case SEC_I_CONTINUE_NEEDED:
case SEC_I_COMPLETE_AND_CONTINUE:
sspi_ctx->state = sspi_auth_in_progress;
break;
default:
return svn_error_createf(SVN_ERR_AUTHN_FAILED, NULL,
"Authentication failed with error 0x%x.", status);
}
return SVN_NO_ERROR;
}
#endif