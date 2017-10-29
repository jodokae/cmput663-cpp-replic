#if !defined(MOD_ISAPI_H)
#define MOD_ISAPI_H
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct HSE_VERSION_INFO {
apr_uint32_t dwExtensionVersion;
char lpszExtensionDesc[256];
} HSE_VERSION_INFO;
int APR_THREAD_FUNC GetExtensionVersion(HSE_VERSION_INFO *ver_info);
typedef int (APR_THREAD_FUNC *PFN_GETEXTENSIONVERSION)(HSE_VERSION_INFO *ver_info);
typedef struct isapi_cid isapi_cid;
typedef struct isapi_cid *HCONN;
typedef int (APR_THREAD_FUNC
*PFN_GETSERVERVARIABLE)(HCONN cid,
char *variable_name,
void *buf_data,
apr_uint32_t *buf_size);
typedef int (APR_THREAD_FUNC
*PFN_WRITECLIENT)(HCONN cid,
void *buf_data,
apr_uint32_t *buf_size,
apr_uint32_t flags);
typedef int (APR_THREAD_FUNC
*PFN_READCLIENT)(HCONN cid,
void *buf_data,
apr_uint32_t *buf_size);
typedef int (APR_THREAD_FUNC
*PFN_SERVERSUPPORTFUNCTION)(HCONN cid,
apr_uint32_t HSE_code,
void *buf_data,
apr_uint32_t *buf_size,
apr_uint32_t *flags);
typedef struct EXTENSION_CONTROL_BLOCK {
apr_uint32_t cbSize;
apr_uint32_t dwVersion;
HCONN ConnID;
apr_uint32_t dwHttpStatusCode;
char lpszLogData[80];
char *lpszMethod;
char *lpszQueryString;
char *lpszPathInfo;
char *lpszPathTranslated;
apr_uint32_t cbTotalBytes;
apr_uint32_t cbAvailable;
unsigned char *lpbData;
char *lpszContentType;
PFN_GETSERVERVARIABLE GetServerVariable;
PFN_WRITECLIENT WriteClient;
PFN_READCLIENT ReadClient;
PFN_SERVERSUPPORTFUNCTION ServerSupportFunction;
} EXTENSION_CONTROL_BLOCK;
typedef struct HSE_SEND_HEADER_EX_INFO {
const char * pszStatus;
const char * pszHeader;
apr_uint32_t cchStatus;
apr_uint32_t cchHeader;
int fKeepConn;
} HSE_SEND_HEADER_EX_INFO;
#define HSE_IO_SEND_HEADERS 8
#define HSE_IO_SYNC 1
#define HSE_IO_ASYNC 2
#define HSE_IO_DISCONNECT_AFTER_SEND 4
#define HSE_IO_NODELAY 4096
typedef void (APR_THREAD_FUNC *PFN_HSE_IO_COMPLETION)
(EXTENSION_CONTROL_BLOCK *ecb,
void *ctxt,
apr_uint32_t cbIO,
apr_uint32_t dwError);
typedef struct HSE_TF_INFO {
PFN_HSE_IO_COMPLETION pfnHseIO;
void *pContext;
apr_os_file_t hFile;
const char *pszStatusCode;
apr_uint32_t BytesToWrite;
apr_uint32_t Offset;
void *pHead;
apr_uint32_t HeadLength;
void *pTail;
apr_uint32_t TailLength;
apr_uint32_t dwFlags;
} HSE_TF_INFO;
typedef struct HSE_URL_MAPEX_INFO {
char lpszPath[260];
apr_uint32_t dwFlags;
apr_uint32_t cchMatchingPath;
apr_uint32_t cchMatchingURL;
apr_uint32_t dwReserved1;
apr_uint32_t dwReserved2;
} HSE_URL_MAPEX_INFO;
#define HSE_REQ_SEND_URL_REDIRECT_RESP 1
#define HSE_REQ_SEND_URL 2
#define HSE_REQ_SEND_RESPONSE_HEADER 3
#define HSE_REQ_DONE_WITH_SESSION 4
#define HSE_REQ_MAP_URL_TO_PATH 1001
#define HSE_REQ_GET_SSPI_INFO 1002
#define HSE_APPEND_LOG_PARAMETER 1003
#define HSE_REQ_IO_COMPLETION 1005
#define HSE_REQ_TRANSMIT_FILE 1006
#define HSE_REQ_REFRESH_ISAPI_ACL 1007
#define HSE_REQ_IS_KEEP_CONN 1008
#define HSE_REQ_ASYNC_READ_CLIENT 1010
#define HSE_REQ_GET_IMPERSONATION_TOKEN 1011
#define HSE_REQ_MAP_URL_TO_PATH_EX 1012
#define HSE_REQ_ABORTIVE_CLOSE 1014
#define HSE_REQ_GET_CERT_INFO_EX 1015
#define HSE_REQ_SEND_RESPONSE_HEADER_EX 1016
#define HSE_REQ_CLOSE_CONNECTION 1017
#define HSE_REQ_IS_CONNECTED 1018
#define HSE_REQ_EXTENSION_TRIGGER 1020
apr_uint32_t APR_THREAD_FUNC HttpExtensionProc(EXTENSION_CONTROL_BLOCK *ecb);
typedef apr_uint32_t (APR_THREAD_FUNC
*PFN_HTTPEXTENSIONPROC)(EXTENSION_CONTROL_BLOCK *ecb);
#define HSE_STATUS_SUCCESS 1
#define HSE_STATUS_SUCCESS_AND_KEEP_CONN 2
#define HSE_STATUS_PENDING 3
#define HSE_STATUS_ERROR 4
#if !defined(ERROR_INSUFFICIENT_BUFFER)
#define ERROR_INSUFFICIENT_BUFFER ENOBUFS
#endif
#if !defined(ERROR_INVALID_INDEX)
#define ERROR_INVALID_INDEX EINVAL
#endif
#if !defined(ERROR_INVALID_PARAMETER)
#define ERROR_INVALID_PARAMETER EINVAL
#endif
#if !defined(ERROR_READ_FAULT)
#define ERROR_READ_FAULT EIO
#endif
#if !defined(ERROR_WRITE_FAULT)
#define ERROR_WRITE_FAULT EIO
#endif
#if !defined(ERROR_SUCCESS)
#define ERROR_SUCCESS 0
#endif
#define HSE_TERM_MUST_UNLOAD 1
#define HSE_TERM_ADVISORY_UNLOAD 2
int APR_THREAD_FUNC TerminateExtension(apr_uint32_t flags);
typedef int (APR_THREAD_FUNC *PFN_TERMINATEEXTENSION)(apr_uint32_t flags);
#define HSE_TERM_MUST_UNLOAD 1
#define HSE_TERM_ADVISORY_UNLOAD 2
#if defined(__cplusplus)
}
#endif
#endif
