#if defined(WIN32)
#include "apr.h"
#include <process.h>
#include "httpd.h"
#include "http_main.h"
#include "http_log.h"
#include "http_config.h"
#include "http_core.h"
#include "http_connection.h"
#include "http_vhost.h"
#include "apr_portable.h"
#include "apr_thread_proc.h"
#include "apr_getopt.h"
#include "apr_strings.h"
#include "apr_lib.h"
#include "apr_shm.h"
#include "apr_thread_mutex.h"
#include "ap_mpm.h"
#include "ap_config.h"
#include "ap_listen.h"
#include "mpm_default.h"
#include "mpm_winnt.h"
#include "mpm_common.h"
#include <malloc.h>
#include "apr_atomic.h"
#include "apr_buckets.h"
#include "scoreboard.h"
#if defined(__MINGW32__)
#include <mswsock.h>
#if !defined(WSAID_ACCEPTEX)
#define WSAID_ACCEPTEX {0xb5367df1, 0xcbac, 0x11cf, {0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92}}
typedef BOOL (WINAPI *LPFN_ACCEPTEX)(SOCKET, SOCKET, PVOID, DWORD, DWORD, DWORD, LPDWORD, LPOVERLAPPED);
#endif
#if !defined(WSAID_GETACCEPTEXSOCKADDRS)
#define WSAID_GETACCEPTEXSOCKADDRS {0xb5367df2, 0xcbac, 0x11cf, {0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92}}
typedef VOID (WINAPI *LPFN_GETACCEPTEXSOCKADDRS)(PVOID, DWORD, DWORD, DWORD,
struct sockaddr **, LPINT,
struct sockaddr **, LPINT);
#endif
#endif
#if defined(CONTAINING_RECORD)
#undef CONTAINING_RECORD
#endif
#define CONTAINING_RECORD(address, type, field) ((type *)( (char *)(address) - (char *)(&((type *)0)->field)))
#if APR_HAVE_IPV6
#define PADDED_ADDR_SIZE (sizeof(SOCKADDR_IN6)+16)
#else
#define PADDED_ADDR_SIZE (sizeof(SOCKADDR_IN)+16)
#endif
APLOG_USE_MODULE(mpm_winnt);
typedef struct winnt_conn_ctx_t_s {
struct winnt_conn_ctx_t_s *next;
OVERLAPPED overlapped;
apr_socket_t *sock;
SOCKET accept_socket;
char buff[2*PADDED_ADDR_SIZE];
struct sockaddr *sa_server;
int sa_server_len;
struct sockaddr *sa_client;
int sa_client_len;
apr_pool_t *ptrans;
apr_bucket_alloc_t *ba;
apr_bucket *data;
#if APR_HAVE_IPV6
short socket_family;
#endif
} winnt_conn_ctx_t;
typedef enum {
IOCP_CONNECTION_ACCEPTED = 1,
IOCP_WAIT_FOR_RECEIVE = 2,
IOCP_WAIT_FOR_TRANSMITFILE = 3,
IOCP_SHUTDOWN = 4
} io_state_e;
static apr_pool_t *pchild;
static int shutdown_in_progress = 0;
static int workers_may_exit = 0;
static unsigned int g_blocked_threads = 0;
static HANDLE max_requests_per_child_event;
static apr_thread_mutex_t *child_lock;
static apr_thread_mutex_t *qlock;
static winnt_conn_ctx_t *qhead = NULL;
static winnt_conn_ctx_t *qtail = NULL;
static apr_uint32_t num_completion_contexts = 0;
static apr_uint32_t max_num_completion_contexts = 0;
static HANDLE ThreadDispatchIOCP = NULL;
static HANDLE qwait_event = NULL;
static void mpm_recycle_completion_context(winnt_conn_ctx_t *context) {
if (context) {
HANDLE saved_event;
apr_pool_clear(context->ptrans);
context->ba = apr_bucket_alloc_create(context->ptrans);
context->next = NULL;
saved_event = context->overlapped.hEvent;
memset(&context->overlapped, 0, sizeof(context->overlapped));
context->overlapped.hEvent = saved_event;
ResetEvent(context->overlapped.hEvent);
apr_thread_mutex_lock(qlock);
if (qtail) {
qtail->next = context;
} else {
qhead = context;
SetEvent(qwait_event);
}
qtail = context;
apr_thread_mutex_unlock(qlock);
}
}
static winnt_conn_ctx_t *mpm_get_completion_context(int *timeout) {
apr_status_t rv;
winnt_conn_ctx_t *context = NULL;
*timeout = 0;
while (1) {
apr_thread_mutex_lock(qlock);
if (qhead) {
context = qhead;
qhead = qhead->next;
if (!qhead)
qtail = NULL;
} else {
ResetEvent(qwait_event);
}
apr_thread_mutex_unlock(qlock);
if (!context) {
if (num_completion_contexts >= max_num_completion_contexts) {
static int reported = 0;
if (!reported) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, APLOGNO(00326)
"Server ran out of threads to serve "
"requests. Consider raising the "
"ThreadsPerChild setting");
reported = 1;
}
rv = WaitForSingleObject(qwait_event, 1000);
if (rv == WAIT_OBJECT_0)
continue;
else {
if (rv == WAIT_TIMEOUT) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00327)
"mpm_get_completion_context: Failed to get a "
"free context within 1 second");
*timeout = 1;
} else {
ap_log_error(APLOG_MARK, APLOG_WARNING, apr_get_os_error(),
ap_server_conf, APLOGNO(00328)
"mpm_get_completion_context: "
"WaitForSingleObject failed to get free context");
}
return NULL;
}
} else {
apr_allocator_t *allocator;
apr_thread_mutex_lock(child_lock);
context = (winnt_conn_ctx_t *)apr_pcalloc(pchild,
sizeof(winnt_conn_ctx_t));
context->overlapped.hEvent = CreateEvent(NULL, TRUE,
FALSE, NULL);
if (context->overlapped.hEvent == NULL) {
ap_log_error(APLOG_MARK, APLOG_WARNING, apr_get_os_error(),
ap_server_conf, APLOGNO(00329)
"mpm_get_completion_context: "
"CreateEvent failed.");
apr_thread_mutex_unlock(child_lock);
return NULL;
}
apr_allocator_create(&allocator);
apr_allocator_max_free_set(allocator, ap_max_mem_free);
rv = apr_pool_create_ex(&context->ptrans, pchild, NULL,
allocator);
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_WARNING, rv, ap_server_conf, APLOGNO(00330)
"mpm_get_completion_context: Failed "
"to create the transaction pool.");
CloseHandle(context->overlapped.hEvent);
apr_thread_mutex_unlock(child_lock);
return NULL;
}
apr_allocator_owner_set(allocator, context->ptrans);
apr_pool_tag(context->ptrans, "transaction");
context->accept_socket = INVALID_SOCKET;
context->ba = apr_bucket_alloc_create(context->ptrans);
apr_atomic_inc32(&num_completion_contexts);
apr_thread_mutex_unlock(child_lock);
break;
}
} else {
break;
}
}
return context;
}
#define MAX_ACCEPTEX_ERR_COUNT 10
static unsigned int __stdcall winnt_accept(void *lr_) {
ap_listen_rec *lr = (ap_listen_rec *)lr_;
apr_os_sock_info_t sockinfo;
winnt_conn_ctx_t *context = NULL;
DWORD BytesRead;
SOCKET nlsd;
LPFN_ACCEPTEX lpfnAcceptEx = NULL;
LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = NULL;
GUID GuidAcceptEx = WSAID_ACCEPTEX;
GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
core_server_config *core_sconf;
const char *accf_name;
int rv;
int accf;
int err_count = 0;
HANDLE events[3];
#if APR_HAVE_IPV6
SOCKADDR_STORAGE ss_listen;
int namelen = sizeof(ss_listen);
#endif
u_long zero = 0;
core_sconf = ap_get_core_module_config(ap_server_conf->module_config);
accf_name = apr_table_get(core_sconf->accf_map, lr->protocol);
if (!accf_name) {
accf = 0;
accf_name = "none";
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, ap_server_conf,
APLOGNO(02531) "winnt_accept: Listen protocol '%s' has "
"no known accept filter. Using 'none' instead",
lr->protocol);
} else if (strcmp(accf_name, "data") == 0) {
accf = 1;
accf_name = "connect";
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ap_server_conf,
APLOGNO(03458) "winnt_accept: 'data' accept filter is no "
"longer supported. Using 'connect' instead");
} else if (strcmp(accf_name, "connect") == 0)
accf = 1;
else if (strcmp(accf_name, "none") == 0)
accf = 0;
else {
accf = 0;
accf_name = "none";
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, ap_server_conf, APLOGNO(00331)
"winnt_accept: unrecognized AcceptFilter '%s', "
"only 'data', 'connect' or 'none' are valid. "
"Using 'none' instead", accf_name);
}
apr_os_sock_get(&nlsd, lr->sd);
#if APR_HAVE_IPV6
if (getsockname(nlsd, (struct sockaddr *)&ss_listen, &namelen) == SOCKET_ERROR) {
ap_log_error(APLOG_MARK, APLOG_ERR, apr_get_netos_error(),
ap_server_conf, APLOGNO(00332)
"winnt_accept: getsockname error on listening socket, "
"is IPv6 available?");
return 1;
}
#endif
if (accf > 0) {
if (WSAIoctl(nlsd, SIO_GET_EXTENSION_FUNCTION_POINTER,
&GuidAcceptEx, sizeof GuidAcceptEx,
&lpfnAcceptEx, sizeof lpfnAcceptEx,
&BytesRead, NULL, NULL) == SOCKET_ERROR) {
ap_log_error(APLOG_MARK, APLOG_ERR, apr_get_netos_error(),
ap_server_conf, APLOGNO(02322)
"winnt_accept: failed to retrieve AcceptEx, try 'AcceptFilter none'");
return 1;
}
if (WSAIoctl(nlsd, SIO_GET_EXTENSION_FUNCTION_POINTER,
&GuidGetAcceptExSockaddrs, sizeof GuidGetAcceptExSockaddrs,
&lpfnGetAcceptExSockaddrs, sizeof lpfnGetAcceptExSockaddrs,
&BytesRead, NULL, NULL) == SOCKET_ERROR) {
ap_log_error(APLOG_MARK, APLOG_ERR, apr_get_netos_error(),
ap_server_conf, APLOGNO(02323)
"winnt_accept: failed to retrieve GetAcceptExSockaddrs, try 'AcceptFilter none'");
return 1;
}
events[1] = exit_event;
events[2] = max_requests_per_child_event;
} else {
reinit:
events[0] = exit_event;
events[1] = max_requests_per_child_event;
events[2] = CreateEvent(NULL, FALSE, FALSE, NULL);
rv = WSAEventSelect(nlsd, events[2], FD_ACCEPT);
if (rv) {
ap_log_error(APLOG_MARK, APLOG_ERR,
apr_get_netos_error(), ap_server_conf, APLOGNO(00333)
"WSAEventSelect() failed.");
CloseHandle(events[2]);
return 1;
}
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00334)
"Child: Accept thread listening on %pI using AcceptFilter %s",
lr->bind_addr, accf_name);
while (!shutdown_in_progress) {
if (!context) {
int timeout;
context = mpm_get_completion_context(&timeout);
if (!context) {
if (!timeout) {
++err_count;
if (err_count > MAX_ACCEPTEX_ERR_COUNT) {
ap_log_error(APLOG_MARK, APLOG_CRIT, 0, ap_server_conf, APLOGNO(00335)
"winnt_accept: Too many failures grabbing a "
"connection ctx. Aborting.");
break;
}
}
Sleep(100);
continue;
}
}
if (accf > 0) {
char *buf;
#if APR_HAVE_IPV6
if (context->accept_socket == INVALID_SOCKET) {
context->accept_socket = socket(ss_listen.ss_family, SOCK_STREAM,
IPPROTO_TCP);
context->socket_family = ss_listen.ss_family;
} else if (context->socket_family != ss_listen.ss_family) {
closesocket(context->accept_socket);
context->accept_socket = socket(ss_listen.ss_family, SOCK_STREAM,
IPPROTO_TCP);
context->socket_family = ss_listen.ss_family;
}
#else
if (context->accept_socket == INVALID_SOCKET)
context->accept_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
if (context->accept_socket == INVALID_SOCKET) {
ap_log_error(APLOG_MARK, APLOG_WARNING, apr_get_netos_error(),
ap_server_conf, APLOGNO(00336)
"winnt_accept: Failed to allocate an accept socket. "
"Temporary resource constraint? Try again.");
Sleep(100);
continue;
}
buf = context->buff;
if (!lpfnAcceptEx(nlsd, context->accept_socket, buf, 0,
PADDED_ADDR_SIZE, PADDED_ADDR_SIZE, &BytesRead,
&context->overlapped)) {
rv = apr_get_netos_error();
if ((rv == APR_FROM_OS_ERROR(WSAECONNRESET)) ||
(rv == APR_FROM_OS_ERROR(WSAEACCES))) {
closesocket(context->accept_socket);
context->accept_socket = INVALID_SOCKET;
continue;
} else if ((rv == APR_FROM_OS_ERROR(WSAEINVAL)) ||
(rv == APR_FROM_OS_ERROR(WSAENOTSOCK))) {
closesocket(context->accept_socket);
context->accept_socket = INVALID_SOCKET;
++err_count;
if (err_count > MAX_ACCEPTEX_ERR_COUNT) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, ap_server_conf, APLOGNO(00337)
"Child: Encountered too many AcceptEx "
"faults accepting client connections. "
"Possible causes: dynamic address renewal, "
"or incompatible VPN or firewall software. ");
ap_log_error(APLOG_MARK, APLOG_NOTICE, rv, ap_server_conf, APLOGNO(00338)
"winnt_mpm: falling back to "
"'AcceptFilter none'.");
err_count = 0;
accf = 0;
}
continue;
} else if ((rv != APR_FROM_OS_ERROR(ERROR_IO_PENDING)) &&
(rv != APR_FROM_OS_ERROR(WSA_IO_PENDING))) {
closesocket(context->accept_socket);
context->accept_socket = INVALID_SOCKET;
++err_count;
if (err_count > MAX_ACCEPTEX_ERR_COUNT) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, ap_server_conf, APLOGNO(00339)
"Child: Encountered too many AcceptEx "
"faults accepting client connections.");
ap_log_error(APLOG_MARK, APLOG_NOTICE, rv, ap_server_conf, APLOGNO(00340)
"winnt_mpm: falling back to "
"'AcceptFilter none'.");
err_count = 0;
accf = 0;
goto reinit;
}
continue;
}
err_count = 0;
events[0] = context->overlapped.hEvent;
do {
rv = WaitForMultipleObjectsEx(3, events, FALSE, INFINITE, TRUE);
} while (rv == WAIT_IO_COMPLETION);
if (rv == WAIT_OBJECT_0) {
if ((context->accept_socket != INVALID_SOCKET) &&
!GetOverlappedResult((HANDLE)context->accept_socket,
&context->overlapped,
&BytesRead, FALSE)) {
ap_log_error(APLOG_MARK, APLOG_WARNING,
apr_get_os_error(), ap_server_conf, APLOGNO(00341)
"winnt_accept: Asynchronous AcceptEx failed.");
closesocket(context->accept_socket);
context->accept_socket = INVALID_SOCKET;
}
} else {
closesocket(context->accept_socket);
context->accept_socket = INVALID_SOCKET;
break;
}
if (context->accept_socket == INVALID_SOCKET) {
continue;
}
}
err_count = 0;
if (setsockopt(context->accept_socket, SOL_SOCKET,
SO_UPDATE_ACCEPT_CONTEXT, (char *)&nlsd,
sizeof(nlsd))) {
ap_log_error(APLOG_MARK, APLOG_WARNING, apr_get_netos_error(),
ap_server_conf, APLOGNO(00342)
"setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed.");
}
lpfnGetAcceptExSockaddrs(buf, 0, PADDED_ADDR_SIZE, PADDED_ADDR_SIZE,
&context->sa_server, &context->sa_server_len,
&context->sa_client, &context->sa_client_len);
} else {
if (context->accept_socket != INVALID_SOCKET)
closesocket(context->accept_socket);
do {
rv = WaitForMultipleObjectsEx(3, events, FALSE, INFINITE, TRUE);
} while (rv == WAIT_IO_COMPLETION);
if (rv != WAIT_OBJECT_0 + 2) {
break;
}
context->sa_server = (void *) context->buff;
context->sa_server_len = sizeof(context->buff) / 2;
context->sa_client_len = context->sa_server_len;
context->sa_client = (void *) (context->buff
+ context->sa_server_len);
context->accept_socket = accept(nlsd, context->sa_server,
&context->sa_server_len);
if (context->accept_socket == INVALID_SOCKET) {
rv = apr_get_netos_error();
if ( rv == APR_FROM_OS_ERROR(WSAECONNRESET)
|| rv == APR_FROM_OS_ERROR(WSAEINPROGRESS)
|| rv == APR_FROM_OS_ERROR(WSAEWOULDBLOCK) ) {
ap_log_error(APLOG_MARK, APLOG_DEBUG,
rv, ap_server_conf, APLOGNO(00343)
"accept() failed, retrying.");
continue;
}
ap_log_error(APLOG_MARK, APLOG_WARNING,
rv, ap_server_conf, APLOGNO(00344)
"accept() failed.");
if ( rv == APR_FROM_OS_ERROR(WSAEMFILE)
|| rv == APR_FROM_OS_ERROR(WSAENOBUFS) ) {
Sleep(100);
++err_count;
if (err_count > MAX_ACCEPTEX_ERR_COUNT) {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, ap_server_conf, APLOGNO(00345)
"Child: Encountered too many accept() "
"resource faults, aborting.");
break;
}
continue;
}
break;
}
WSAEventSelect(context->accept_socket, 0, 0);
err_count = 0;
context->sa_server_len = sizeof(context->buff) / 2;
if (getsockname(context->accept_socket, context->sa_server,
&context->sa_server_len) == SOCKET_ERROR) {
ap_log_error(APLOG_MARK, APLOG_WARNING, apr_get_netos_error(), ap_server_conf, APLOGNO(00346)
"getsockname failed");
continue;
}
if ((getpeername(context->accept_socket, context->sa_client,
&context->sa_client_len)) == SOCKET_ERROR) {
ap_log_error(APLOG_MARK, APLOG_WARNING, apr_get_netos_error(), ap_server_conf, APLOGNO(00347)
"getpeername failed");
memset(&context->sa_client, '\0', sizeof(context->sa_client));
}
}
sockinfo.os_sock = &context->accept_socket;
sockinfo.local = context->sa_server;
sockinfo.remote = context->sa_client;
sockinfo.family = context->sa_server->sa_family;
sockinfo.type = SOCK_STREAM;
sockinfo.protocol = IPPROTO_TCP;
ioctlsocket(context->accept_socket, FIONBIO, &zero);
setsockopt(context->accept_socket, SOL_SOCKET, SO_RCVTIMEO,
(char *) &zero, sizeof(zero));
setsockopt(context->accept_socket, SOL_SOCKET, SO_SNDTIMEO,
(char *) &zero, sizeof(zero));
apr_os_sock_make(&context->sock, &sockinfo, context->ptrans);
PostQueuedCompletionStatus(ThreadDispatchIOCP, BytesRead,
IOCP_CONNECTION_ACCEPTED,
&context->overlapped);
context = NULL;
}
if (!accf)
CloseHandle(events[2]);
if (!shutdown_in_progress) {
SetEvent(exit_event);
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, ap_server_conf, APLOGNO(00348)
"Child: Accept thread exiting.");
return 0;
}
static winnt_conn_ctx_t *winnt_get_connection(winnt_conn_ctx_t *context) {
int rc;
DWORD BytesRead;
LPOVERLAPPED pol;
#if defined(_WIN64)
ULONG_PTR CompKey;
#else
DWORD CompKey;
#endif
mpm_recycle_completion_context(context);
apr_atomic_inc32(&g_blocked_threads);
while (1) {
if (workers_may_exit) {
apr_atomic_dec32(&g_blocked_threads);
return NULL;
}
rc = GetQueuedCompletionStatus(ThreadDispatchIOCP, &BytesRead,
&CompKey, &pol, INFINITE);
if (!rc) {
rc = apr_get_os_error();
ap_log_error(APLOG_MARK, APLOG_DEBUG, rc, ap_server_conf, APLOGNO(00349)
"Child: GetQueuedComplationStatus returned %d",
rc);
continue;
}
switch (CompKey) {
case IOCP_CONNECTION_ACCEPTED:
context = CONTAINING_RECORD(pol, winnt_conn_ctx_t, overlapped);
break;
case IOCP_SHUTDOWN:
apr_atomic_dec32(&g_blocked_threads);
return NULL;
default:
apr_atomic_dec32(&g_blocked_threads);
return NULL;
}
break;
}
apr_atomic_dec32(&g_blocked_threads);
return context;
}
static DWORD __stdcall worker_main(void *thread_num_val) {
apr_thread_t *thd;
apr_os_thread_t osthd;
static int requests_this_child = 0;
winnt_conn_ctx_t *context = NULL;
int thread_num = (int)thread_num_val;
ap_sb_handle_t *sbh;
conn_rec *c;
apr_int32_t disconnected;
osthd = apr_os_thread_current();
while (1) {
ap_update_child_status_from_indexes(0, thread_num, SERVER_READY, NULL);
context = winnt_get_connection(context);
if (!context) {
break;
}
if (ap_max_requests_per_child) {
requests_this_child++;
if (requests_this_child > ap_max_requests_per_child) {
SetEvent(max_requests_per_child_event);
}
}
ap_create_sb_handle(&sbh, context->ptrans, 0, thread_num);
c = ap_run_create_connection(context->ptrans, ap_server_conf,
context->sock, thread_num, sbh,
context->ba);
if (!c) {
context->accept_socket = INVALID_SOCKET;
continue;
}
thd = NULL;
apr_os_thread_put(&thd, &osthd, context->ptrans);
c->current_thread = thd;
ap_process_connection(c, context->sock);
ap_lingering_close(c);
apr_socket_opt_get(context->sock, APR_SO_DISCONNECTED, &disconnected);
if (!disconnected) {
context->accept_socket = INVALID_SOCKET;
}
}
ap_update_child_status_from_indexes(0, thread_num, SERVER_DEAD, NULL);
return 0;
}
static void cleanup_thread(HANDLE *handles, int *thread_cnt,
int thread_to_clean) {
int i;
CloseHandle(handles[thread_to_clean]);
for (i = thread_to_clean; i < ((*thread_cnt) - 1); i++)
handles[i] = handles[i + 1];
(*thread_cnt)--;
}
static void create_listener_thread(void) {
unsigned tid;
int num_listeners = 0;
ap_listen_rec *lr;
for (lr = ap_listeners; lr ; lr = lr->next) {
num_listeners++;
}
max_num_completion_contexts = ap_threads_per_child + num_listeners;
for (lr = ap_listeners; lr; lr = lr->next) {
if (lr->sd != NULL) {
_beginthreadex(NULL, 65536, winnt_accept,
(void *) lr, stack_res_flag, &tid);
}
}
}
void child_main(apr_pool_t *pconf, DWORD parent_pid) {
apr_status_t status;
apr_hash_t *ht;
ap_listen_rec *lr;
HANDLE child_events[3];
HANDLE *child_handles;
int listener_started = 0;
int threads_created = 0;
int watch_thread;
int time_remains;
int cld;
DWORD tid;
int rv;
int i;
int num_events;
apr_pool_create(&pchild, pconf);
apr_pool_tag(pchild, "pchild");
ap_run_child_init(pchild, ap_server_conf);
ht = apr_hash_make(pchild);
max_requests_per_child_event = CreateEvent(NULL, TRUE, FALSE, NULL);
if (!max_requests_per_child_event) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(), ap_server_conf, APLOGNO(00350)
"Child: Failed to create a max_requests event.");
exit(APEXIT_CHILDINIT);
}
child_events[0] = exit_event;
child_events[1] = max_requests_per_child_event;
if (parent_pid != my_pid) {
child_events[2] = OpenProcess(SYNCHRONIZE, FALSE, parent_pid);
if (child_events[2] == NULL) {
num_events = 2;
ap_log_error(APLOG_MARK, APLOG_ERR, apr_get_os_error(), ap_server_conf, APLOGNO(02643)
"Child: Failed to open handle to parent process %ld; "
"will not react to abrupt parent termination", parent_pid);
} else {
num_events = 3;
}
} else {
child_events[2] = NULL;
num_events = 2;
}
status = apr_proc_mutex_lock(start_mutex);
if (status != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, status, ap_server_conf, APLOGNO(00351)
"Child: Failed to acquire the start_mutex. "
"Process will exit.");
exit(APEXIT_CHILDINIT);
}
ap_log_error(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, ap_server_conf, APLOGNO(00352)
"Child: Acquired the start mutex.");
ThreadDispatchIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
NULL, 0, 0);
apr_thread_mutex_create(&qlock, APR_THREAD_MUTEX_DEFAULT, pchild);
qwait_event = CreateEvent(NULL, TRUE, FALSE, NULL);
if (!qwait_event) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(),
ap_server_conf, APLOGNO(00353)
"Child: Failed to create a qwait event.");
exit(APEXIT_CHILDINIT);
}
ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, ap_server_conf, APLOGNO(00354)
"Child: Starting %d worker threads.", ap_threads_per_child);
child_handles = (HANDLE) apr_pcalloc(pchild, ap_threads_per_child
* sizeof(HANDLE));
apr_thread_mutex_create(&child_lock, APR_THREAD_MUTEX_DEFAULT, pchild);
while (1) {
for (i = 0; i < ap_threads_per_child; i++) {
int *score_idx;
int status = ap_scoreboard_image->servers[0][i].status;
if (status != SERVER_GRACEFUL && status != SERVER_DEAD) {
continue;
}
ap_update_child_status_from_indexes(0, i, SERVER_STARTING, NULL);
child_handles[i] = CreateThread(NULL, ap_thread_stacksize,
worker_main, (void *) i,
stack_res_flag, &tid);
if (child_handles[i] == 0) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(),
ap_server_conf, APLOGNO(00355)
"Child: CreateThread failed. Unable to "
"create all worker threads. Created %d of the %d "
"threads requested with the ThreadsPerChild "
"configuration directive.",
threads_created, ap_threads_per_child);
ap_signal_parent(SIGNAL_PARENT_SHUTDOWN);
goto shutdown;
}
threads_created++;
apr_thread_mutex_lock(child_lock);
score_idx = apr_pcalloc(pchild, sizeof(int));
*score_idx = i;
apr_hash_set(ht, &child_handles[i], sizeof(HANDLE), score_idx);
apr_thread_mutex_unlock(child_lock);
}
if (!listener_started && threads_created) {
create_listener_thread();
listener_started = 1;
winnt_mpm_state = AP_MPMQ_RUNNING;
}
if (threads_created == ap_threads_per_child) {
break;
}
if (WaitForSingleObject(exit_event, 0) != WAIT_TIMEOUT) {
break;
}
apr_sleep(1 * APR_USEC_PER_SEC);
}
while (1) {
#if !APR_HAS_OTHER_CHILD
rv = WaitForMultipleObjects(num_events, (HANDLE *)child_events, FALSE, INFINITE);
cld = rv - WAIT_OBJECT_0;
#else
rv = WaitForMultipleObjects(num_events, (HANDLE *)child_events, FALSE, 1000);
cld = rv - WAIT_OBJECT_0;
if (rv == WAIT_TIMEOUT) {
apr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
} else
#endif
if (rv == WAIT_FAILED) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(),
ap_server_conf, APLOGNO(00356)
"Child: WAIT_FAILED -- shutting down server");
for (i = 0; i < num_events; i++) {
DWORD out_flags;
if (0 == GetHandleInformation(child_events[i], &out_flags)) {
ap_log_error(APLOG_MARK, APLOG_CRIT, apr_get_os_error(),
ap_server_conf, APLOGNO(02644)
"Child: Event handle #%d (%pp) is invalid",
i, child_events[i]);
}
}
break;
} else if (cld == 0) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, ap_server_conf, APLOGNO(00357)
"Child: Exit event signaled. Child process is "
"ending.");
break;
} else if (cld == 2) {
ap_log_error(APLOG_MARK, APLOG_CRIT, 0, ap_server_conf, APLOGNO(02538)
"Child: Parent process exited abruptly. Child process "
"is ending");
break;
} else {
ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, ap_server_conf, APLOGNO(00358)
"Child: Process exiting because it reached "
"MaxConnectionsPerChild. Signaling the parent to "
"restart a new child process.");
ap_signal_parent(SIGNAL_PARENT_RESTART);
break;
}
}
shutdown:
winnt_mpm_state = AP_MPMQ_STOPPING;
for (lr = ap_listeners; lr ; lr = lr->next) {
apr_socket_close(lr->sd);
}
shutdown_in_progress = 1;
Sleep(1000);
workers_may_exit = 1;
rv = apr_proc_mutex_unlock(start_mutex);
if (rv == APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, rv, ap_server_conf, APLOGNO(00359)
"Child: Released the start mutex");
} else {
ap_log_error(APLOG_MARK, APLOG_ERR, rv, ap_server_conf, APLOGNO(00360)
"Child: Failure releasing the start mutex");
}
while (g_blocked_threads > 0) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, APR_SUCCESS, ap_server_conf, APLOGNO(00361)
"Child: %d threads blocked on the completion port",
g_blocked_threads);
for (i=g_blocked_threads; i > 0; i--) {
PostQueuedCompletionStatus(ThreadDispatchIOCP, 0,
IOCP_SHUTDOWN, NULL);
}
Sleep(1000);
}
apr_thread_mutex_lock(qlock);
while (qhead) {
CloseHandle(qhead->overlapped.hEvent);
closesocket(qhead->accept_socket);
qhead = qhead->next;
}
apr_thread_mutex_unlock(qlock);
watch_thread = 0;
time_remains = (int)(ap_server_conf->timeout / APR_TIME_C(1000));
while (threads_created) {
int nFailsafe = MAXIMUM_WAIT_OBJECTS;
DWORD dwRet;
if (watch_thread >= threads_created) {
if ((time_remains -= 100) < 0)
break;
if ((time_remains % 30000) == 0) {
ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS,
ap_server_conf, APLOGNO(00362)
"Child: Waiting %d more seconds "
"for %d worker threads to finish.",
time_remains / 1000, threads_created);
}
Sleep(100);
watch_thread = 0;
}
dwRet = WaitForMultipleObjects(min(threads_created - watch_thread,
MAXIMUM_WAIT_OBJECTS),
child_handles + watch_thread, 0, 0);
if (dwRet == WAIT_FAILED) {
break;
}
if (dwRet == WAIT_TIMEOUT) {
watch_thread += MAXIMUM_WAIT_OBJECTS;
continue;
} else if (dwRet >= WAIT_ABANDONED_0) {
if ((nFailsafe--) < 1)
break;
} else {
watch_thread += (dwRet - WAIT_OBJECT_0);
if (watch_thread >= threads_created)
break;
cleanup_thread(child_handles, &threads_created, watch_thread);
}
}
if (threads_created) {
ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, ap_server_conf, APLOGNO(00363)
"Child: Terminating %d threads that failed to exit.",
threads_created);
}
for (i = 0; i < threads_created; i++) {
int *idx;
TerminateThread(child_handles[i], 1);
CloseHandle(child_handles[i]);
idx = apr_hash_get(ht, &child_handles[i], sizeof(HANDLE));
if (idx) {
ap_update_child_status_from_indexes(0, *idx, SERVER_DEAD, NULL);
}
}
ap_log_error(APLOG_MARK, APLOG_NOTICE, APR_SUCCESS, ap_server_conf, APLOGNO(00364)
"Child: All worker threads have exited.");
apr_thread_mutex_destroy(child_lock);
apr_thread_mutex_destroy(qlock);
CloseHandle(qwait_event);
apr_pool_destroy(pchild);
CloseHandle(exit_event);
if (child_events[2] != NULL) {
CloseHandle(child_events[2]);
}
}
#endif
