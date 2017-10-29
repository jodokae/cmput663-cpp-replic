#define _WINUSER_
#include "apr.h"
#include "apr_strings.h"
#include "apr_lib.h"
#if APR_HAS_UNICODE_FS
#include "arch/win32/apr_arch_utf8.h"
#include "arch/win32/apr_arch_misc.h"
#include <wchar.h>
#endif
#include "httpd.h"
#include "http_log.h"
#include "mpm_winnt.h"
#include "ap_regkey.h"
#if defined(NOUSER)
#undef NOUSER
#endif
#undef _WINUSER_
#include <winuser.h>
#include <time.h>
APLOG_USE_MODULE(mpm_winnt);
static char *mpm_service_name = NULL;
static char *mpm_display_name = NULL;
#if APR_HAS_UNICODE_FS
static apr_wchar_t *mpm_service_name_w;
#endif
typedef struct nt_service_ctx_t {
HANDLE mpm_thread;
HANDLE service_thread;
DWORD service_thread_id;
HANDLE service_init;
HANDLE service_term;
SERVICE_STATUS ssStatus;
SERVICE_STATUS_HANDLE hServiceStatus;
} nt_service_ctx_t;
static nt_service_ctx_t globdat;
static int ReportStatusToSCMgr(int currentState, int waitHint,
nt_service_ctx_t *ctx);
#undef OpenSCManager
typedef SC_HANDLE (WINAPI *fpt_OpenSCManager)(const void *lpMachine,
const void *lpDatabase,
DWORD dwAccess);
static fpt_OpenSCManager pfn_OpenSCManager = NULL;
static APR_INLINE SC_HANDLE OpenSCManager(const void *lpMachine,
const void *lpDatabase,
DWORD dwAccess) {
if (!pfn_OpenSCManager) {
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE
pfn_OpenSCManager = (fpt_OpenSCManager)OpenSCManagerW;
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI
pfn_OpenSCManager = (fpt_OpenSCManager)OpenSCManagerA;
#endif
}
return (*(pfn_OpenSCManager))(lpMachine, lpDatabase, dwAccess);
}
AP_DECLARE_DATA int ap_real_exit_code = 1;
void hold_console_open_on_error(void) {
HANDLE hConIn;
HANDLE hConErr;
DWORD result;
time_t start;
time_t remains;
char *msg = "Note the errors or messages above, "
"and press the <ESC> key to exit. ";
CONSOLE_SCREEN_BUFFER_INFO coninfo;
INPUT_RECORD in;
char count[16];
if (!ap_real_exit_code)
return;
hConIn = GetStdHandle(STD_INPUT_HANDLE);
hConErr = GetStdHandle(STD_ERROR_HANDLE);
if ((hConIn == INVALID_HANDLE_VALUE) || (hConErr == INVALID_HANDLE_VALUE))
return;
if (!WriteConsole(hConErr, msg, (DWORD)strlen(msg), &result, NULL)
|| !result)
return;
if (!GetConsoleScreenBufferInfo(hConErr, &coninfo))
return;
if (!SetConsoleMode(hConIn, ENABLE_MOUSE_INPUT | 0x80))
return;
start = time(NULL);
do {
while (PeekConsoleInput(hConIn, &in, 1, &result) && result) {
if (!ReadConsoleInput(hConIn, &in, 1, &result) || !result)
return;
if ((in.EventType == KEY_EVENT) && in.Event.KeyEvent.bKeyDown
&& (in.Event.KeyEvent.uChar.AsciiChar == 27))
return;
if (in.EventType == MOUSE_EVENT
&& (in.Event.MouseEvent.dwEventFlags == DOUBLE_CLICK))
return;
}
remains = ((start + 30) - time(NULL));
sprintf(count, "%d...",
(int)remains);
if (!SetConsoleCursorPosition(hConErr, coninfo.dwCursorPosition))
return;
if (!WriteConsole(hConErr, count, (DWORD)strlen(count), &result, NULL)
|| !result)
return;
} while ((remains > 0) && WaitForSingleObject(hConIn, 1000) != WAIT_FAILED);
}
static BOOL CALLBACK console_control_handler(DWORD ctrl_type) {
switch (ctrl_type) {
case CTRL_BREAK_EVENT:
fprintf(stderr, "Apache server restarting...\n");
ap_signal_parent(SIGNAL_PARENT_RESTART);
return TRUE;
case CTRL_C_EVENT:
fprintf(stderr, "Apache server interrupted...\n");
ap_signal_parent(SIGNAL_PARENT_SHUTDOWN);
return TRUE;
case CTRL_CLOSE_EVENT:
case CTRL_LOGOFF_EVENT:
case CTRL_SHUTDOWN_EVENT:
fprintf(stderr, "Apache server shutdown initiated...\n");
ap_signal_parent(SIGNAL_PARENT_SHUTDOWN);
Sleep(30000);
return TRUE;
}
return FALSE;
}
static void stop_console_handler(void) {
SetConsoleCtrlHandler(console_control_handler, FALSE);
}
void mpm_start_console_handler(void) {
SetConsoleCtrlHandler(console_control_handler, TRUE);
atexit(stop_console_handler);
}
void mpm_start_child_console_handler(void) {
FreeConsole();
}
static int ReportStatusToSCMgr(int currentState, int waitHint,
nt_service_ctx_t *ctx) {
int rv = APR_SUCCESS;
if (ctx->hServiceStatus) {
if (currentState == SERVICE_RUNNING) {
ctx->ssStatus.dwWaitHint = 0;
ctx->ssStatus.dwCheckPoint = 0;
ctx->ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP
| SERVICE_ACCEPT_SHUTDOWN;
} else if (currentState == SERVICE_STOPPED) {
ctx->ssStatus.dwWaitHint = 0;
ctx->ssStatus.dwCheckPoint = 0;
if (ctx->ssStatus.dwCurrentState != SERVICE_STOP_PENDING
&& !ctx->ssStatus.dwServiceSpecificExitCode)
ctx->ssStatus.dwServiceSpecificExitCode = 1;
if (ctx->ssStatus.dwServiceSpecificExitCode)
ctx->ssStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
} else {
++ctx->ssStatus.dwCheckPoint;
ctx->ssStatus.dwControlsAccepted = 0;
if(waitHint)
ctx->ssStatus.dwWaitHint = waitHint;
}
ctx->ssStatus.dwCurrentState = currentState;
rv = SetServiceStatus(ctx->hServiceStatus, &ctx->ssStatus);
}
return(rv);
}
extern apr_pool_t *pconf;
static void set_service_description(void) {
const char *full_description;
SC_HANDLE schSCManager;
if (!mpm_service_name)
return;
full_description = ap_get_server_description();
if ((ChangeServiceConfig2) &&
(schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT))) {
SC_HANDLE schService;
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
schService = OpenServiceW(schSCManager,
(LPCWSTR)mpm_service_name_w,
SERVICE_CHANGE_CONFIG);
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
schService = OpenService(schSCManager, mpm_service_name,
SERVICE_CHANGE_CONFIG);
}
#endif
if (schService) {
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
apr_size_t slen = strlen(full_description) + 1;
apr_size_t wslen = slen;
apr_wchar_t *full_description_w =
(apr_wchar_t*)apr_palloc(pconf,
wslen * sizeof(apr_wchar_t));
apr_status_t rv = apr_conv_utf8_to_ucs2(full_description, &slen,
full_description_w,
&wslen);
if ((rv != APR_SUCCESS) || slen
|| ChangeServiceConfig2W(schService, 1
,
(LPVOID) &full_description_w))
full_description = NULL;
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
if (ChangeServiceConfig2(schService,
1 ,
(LPVOID) &full_description))
full_description = NULL;
}
#endif
CloseServiceHandle(schService);
}
CloseServiceHandle(schSCManager);
}
}
static DWORD WINAPI service_nt_ctrl(DWORD dwCtrlCode, DWORD dwEventType,
LPVOID lpEventData, LPVOID lpContext) {
nt_service_ctx_t *ctx = lpContext;
if ((dwCtrlCode == SERVICE_CONTROL_STOP)
|| (dwCtrlCode == SERVICE_CONTROL_SHUTDOWN)) {
ap_signal_parent(SIGNAL_PARENT_SHUTDOWN);
ReportStatusToSCMgr(SERVICE_STOP_PENDING, 30000, ctx);
return (NO_ERROR);
}
if (dwCtrlCode == SERVICE_APACHE_RESTART) {
ap_signal_parent(SIGNAL_PARENT_RESTART);
ReportStatusToSCMgr(SERVICE_START_PENDING, 30000, ctx);
return (NO_ERROR);
}
if (dwCtrlCode == SERVICE_CONTROL_INTERROGATE) {
ReportStatusToSCMgr(globdat.ssStatus.dwCurrentState, 0, ctx);
return (NO_ERROR);
}
return (ERROR_CALL_NOT_IMPLEMENTED);
}
extern apr_array_header_t *mpm_new_argv;
#if APR_HAS_UNICODE_FS
static void __stdcall service_nt_main_fn_w(DWORD argc, LPWSTR *argv) {
const char *ignored;
nt_service_ctx_t *ctx = &globdat;
char *service_name;
apr_size_t wslen = wcslen(argv[0]) + 1;
apr_size_t slen = wslen * 3 - 2;
service_name = malloc(slen);
(void)apr_conv_ucs2_to_utf8(argv[0], &wslen, service_name, &slen);
mpm_service_set_name(mpm_new_argv->pool, &ignored, service_name);
memset(&ctx->ssStatus, 0, sizeof(ctx->ssStatus));
ctx->ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
ctx->ssStatus.dwCurrentState = SERVICE_START_PENDING;
ctx->ssStatus.dwCheckPoint = 1;
if (!(ctx->hServiceStatus =
RegisterServiceCtrlHandlerExW(argv[0], service_nt_ctrl, ctx))) {
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP,
apr_get_os_error(), NULL,
APLOGNO(00365) "Failure registering service handler");
return;
}
ReportStatusToSCMgr(SERVICE_START_PENDING, 30000, ctx);
if (argc > 1) {
char **cmb_data, **cmb;
DWORD i;
mpm_new_argv->nalloc = mpm_new_argv->nelts + argc - 1;
cmb_data = malloc(mpm_new_argv->nalloc * sizeof(const char *));
memcpy (cmb_data, mpm_new_argv->elts,
mpm_new_argv->elt_size * mpm_new_argv->nelts);
memcpy (cmb_data + mpm_new_argv->nelts, argv + 1,
mpm_new_argv->elt_size * (argc - 1));
cmb = cmb_data + mpm_new_argv->nelts;
for (i = 1; i < argc; ++i) {
wslen = wcslen(argv[i]) + 1;
slen = wslen * 3 - 2;
service_name = malloc(slen);
(void)apr_conv_ucs2_to_utf8(argv[i], &wslen, *(cmb++), &slen);
}
mpm_new_argv->elts = (char *)cmb_data;
mpm_new_argv->nelts = mpm_new_argv->nalloc;
}
SetEvent(ctx->service_init);
WaitForSingleObject(ctx->service_term, INFINITE);
}
#endif
#if APR_HAS_ANSI_FS
static void __stdcall service_nt_main_fn(DWORD argc, LPSTR *argv) {
const char *ignored;
nt_service_ctx_t *ctx = &globdat;
mpm_service_set_name(mpm_new_argv->pool, &ignored, argv[0]);
memset(&ctx->ssStatus, 0, sizeof(ctx->ssStatus));
ctx->ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
ctx->ssStatus.dwCurrentState = SERVICE_START_PENDING;
ctx->ssStatus.dwCheckPoint = 1;
if (!(ctx->hServiceStatus =
RegisterServiceCtrlHandlerExA(argv[0], service_nt_ctrl, ctx))) {
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP,
apr_get_os_error(), NULL,
APLOGNO(10008) "Failure registering service handler");
return;
}
ReportStatusToSCMgr(SERVICE_START_PENDING, 30000, ctx);
if (argc > 1) {
char **cmb_data;
mpm_new_argv->nalloc = mpm_new_argv->nelts + argc - 1;
cmb_data = malloc(mpm_new_argv->nalloc * sizeof(const char *));
memcpy (cmb_data, mpm_new_argv->elts,
mpm_new_argv->elt_size * mpm_new_argv->nelts);
memcpy (cmb_data + mpm_new_argv->nelts, argv + 1,
mpm_new_argv->elt_size * (argc - 1));
mpm_new_argv->elts = (char *)cmb_data;
mpm_new_argv->nelts = mpm_new_argv->nalloc;
}
SetEvent(ctx->service_init);
WaitForSingleObject(ctx->service_term, INFINITE);
}
#endif
static DWORD WINAPI service_nt_dispatch_thread(LPVOID nada) {
#if APR_HAS_UNICODE_FS
SERVICE_TABLE_ENTRYW dispatchTable_w[] = {
{ L"", service_nt_main_fn_w },
{ NULL, NULL }
};
#endif
#if APR_HAS_ANSI_FS
SERVICE_TABLE_ENTRYA dispatchTable[] = {
{ "", service_nt_main_fn },
{ NULL, NULL }
};
#endif
apr_status_t rv;
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE
rv = StartServiceCtrlDispatcherW(dispatchTable_w);
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI
rv = StartServiceCtrlDispatcherA(dispatchTable);
#endif
if (rv) {
rv = APR_SUCCESS;
} else {
rv = apr_get_os_error();
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(00366) "Error starting Windows service control "
"dispatcher");
}
return (rv);
}
apr_status_t mpm_service_set_name(apr_pool_t *p, const char **display_name,
const char *set_name) {
char key_name[MAX_PATH];
ap_regkey_t *key;
apr_status_t rv;
mpm_service_name = apr_palloc(p, strlen(set_name) + 1);
apr_collapse_spaces((char*) mpm_service_name, set_name);
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
apr_size_t slen = strlen(mpm_service_name) + 1;
apr_size_t wslen = slen;
mpm_service_name_w = apr_palloc(p, wslen * sizeof(apr_wchar_t));
rv = apr_conv_utf8_to_ucs2(mpm_service_name, &slen,
mpm_service_name_w, &wslen);
if (rv != APR_SUCCESS)
return rv;
else if (slen)
return APR_ENAMETOOLONG;
}
#endif
apr_snprintf(key_name, sizeof(key_name), SERVICECONFIG, mpm_service_name);
rv = ap_regkey_open(&key, AP_REGKEY_LOCAL_MACHINE, key_name,
APR_READ, pconf);
if (rv == APR_SUCCESS) {
rv = ap_regkey_value_get(&mpm_display_name, key, "DisplayName", pconf);
ap_regkey_close(key);
}
if (rv != APR_SUCCESS) {
mpm_display_name = apr_pstrdup(p, set_name);
}
*display_name = mpm_display_name;
return rv;
}
apr_status_t mpm_merge_service_args(apr_pool_t *p,
apr_array_header_t *args,
int fixed_args) {
apr_array_header_t *svc_args = NULL;
char conf_key[MAX_PATH];
char **cmb_data;
apr_status_t rv;
ap_regkey_t *key;
apr_snprintf(conf_key, sizeof(conf_key), SERVICEPARAMS, mpm_service_name);
rv = ap_regkey_open(&key, AP_REGKEY_LOCAL_MACHINE, conf_key, APR_READ, p);
if (rv == APR_SUCCESS) {
rv = ap_regkey_value_array_get(&svc_args, key, "ConfigArgs", p);
ap_regkey_close(key);
}
if (rv != APR_SUCCESS) {
if (rv == ERROR_FILE_NOT_FOUND) {
ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, APLOGNO(00367)
"No ConfigArgs registered for the '%s' service, "
"perhaps this service is not installed?",
mpm_service_name);
return APR_SUCCESS;
} else
return (rv);
}
if (!svc_args || svc_args->nelts == 0) {
return (APR_SUCCESS);
}
args->nalloc = args->nelts + svc_args->nelts;
cmb_data = malloc(args->nalloc * sizeof(const char *));
memcpy(cmb_data, args->elts, args->elt_size * fixed_args);
memcpy(cmb_data + fixed_args, svc_args->elts,
svc_args->elt_size * svc_args->nelts);
memcpy(cmb_data + fixed_args + svc_args->nelts,
(const char **)args->elts + fixed_args,
args->elt_size * (args->nelts - fixed_args));
args->elts = (char *)cmb_data;
args->nelts = args->nalloc;
return APR_SUCCESS;
}
static void service_stopped(void) {
if (globdat.service_thread) {
mpm_nt_eventlog_stderr_flush();
ReleaseMutex(globdat.service_term);
ReportStatusToSCMgr(SERVICE_STOPPED, 0, &globdat);
WaitForSingleObject(globdat.service_thread, 5000);
CloseHandle(globdat.service_thread);
}
}
apr_status_t mpm_service_to_start(const char **display_name, apr_pool_t *p) {
HANDLE hProc = GetCurrentProcess();
HANDLE hThread = GetCurrentThread();
HANDLE waitfor[2];
ap_real_exit_code = 0;
if (!DuplicateHandle(hProc, hThread, hProc, &(globdat.mpm_thread),
0, FALSE, DUPLICATE_SAME_ACCESS)) {
return APR_ENOTHREAD;
}
globdat.service_init = CreateEvent(NULL, FALSE, FALSE, NULL);
globdat.service_term = CreateMutex(NULL, TRUE, NULL);
if (!globdat.service_init || !globdat.service_term) {
return APR_EGENERAL;
}
globdat.service_thread = CreateThread(NULL, 65536,
service_nt_dispatch_thread,
NULL, stack_res_flag,
&globdat.service_thread_id);
if (!globdat.service_thread) {
return APR_ENOTHREAD;
}
waitfor[0] = globdat.service_init;
waitfor[1] = globdat.service_thread;
if (WaitForMultipleObjects(2, waitfor, FALSE, 10000) != WAIT_OBJECT_0) {
return APR_ENOTHREAD;
}
atexit(service_stopped);
*display_name = mpm_display_name;
return APR_SUCCESS;
}
apr_status_t mpm_service_started(void) {
set_service_description();
ReportStatusToSCMgr(SERVICE_RUNNING, 0, &globdat);
return APR_SUCCESS;
}
void mpm_service_stopping(void) {
ReportStatusToSCMgr(SERVICE_STOP_PENDING, 30000, &globdat);
}
apr_status_t mpm_service_install(apr_pool_t *ptemp, int argc,
const char * const * argv, int reconfig) {
char key_name[MAX_PATH];
char *launch_cmd;
ap_regkey_t *key;
apr_status_t rv;
SC_HANDLE schService;
SC_HANDLE schSCManager;
DWORD rc;
#if APR_HAS_UNICODE_FS
apr_wchar_t *display_name_w;
apr_wchar_t *launch_cmd_w;
#endif
fprintf(stderr, reconfig ? "Reconfiguring the '%s' service\n"
: "Installing the '%s' service\n",
mpm_display_name);
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
apr_size_t slen = strlen(mpm_display_name) + 1;
apr_size_t wslen = slen;
display_name_w = apr_palloc(ptemp, wslen * sizeof(apr_wchar_t));
rv = apr_conv_utf8_to_ucs2(mpm_display_name, &slen,
display_name_w, &wslen);
if (rv != APR_SUCCESS)
return rv;
else if (slen)
return APR_ENAMETOOLONG;
launch_cmd_w = apr_palloc(ptemp, (MAX_PATH + 17) * sizeof(apr_wchar_t));
launch_cmd_w[0] = L'"';
rc = GetModuleFileNameW(NULL, launch_cmd_w + 1, MAX_PATH);
wcscpy(launch_cmd_w + rc + 1, L"\" -k runservice");
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
launch_cmd = apr_palloc(ptemp, MAX_PATH + 17);
launch_cmd[0] = '"';
rc = GetModuleFileName(NULL, launch_cmd + 1, MAX_PATH);
strcpy(launch_cmd + rc + 1, "\" -k runservice");
}
#endif
if (rc == 0) {
rv = apr_get_os_error();
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(00368) "GetModuleFileName failed");
return rv;
}
schSCManager = OpenSCManager(NULL, NULL,
SC_MANAGER_CREATE_SERVICE);
if (!schSCManager) {
rv = apr_get_os_error();
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(00369) "Failed to open the Windows service "
"manager, perhaps you forgot to log in as Adminstrator?");
return (rv);
}
if (reconfig) {
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
schService = OpenServiceW(schSCManager, mpm_service_name_w,
SERVICE_CHANGE_CONFIG);
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
schService = OpenService(schSCManager, mpm_service_name,
SERVICE_CHANGE_CONFIG);
}
#endif
if (!schService) {
rv = apr_get_os_error();
CloseServiceHandle(schSCManager);
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(00373) "Failed to open the '%s' service",
mpm_display_name);
return (rv);
}
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
rc = ChangeServiceConfigW(schService,
SERVICE_WIN32_OWN_PROCESS,
SERVICE_AUTO_START,
SERVICE_ERROR_NORMAL,
launch_cmd_w, NULL, NULL,
L"Tcpip\0Afd\0", NULL, NULL,
display_name_w);
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
rc = ChangeServiceConfig(schService,
SERVICE_WIN32_OWN_PROCESS,
SERVICE_AUTO_START,
SERVICE_ERROR_NORMAL,
launch_cmd, NULL, NULL,
"Tcpip\0Afd\0", NULL, NULL,
mpm_display_name);
}
#endif
if (!rc) {
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP,
apr_get_os_error(), NULL,
APLOGNO(02652) "ChangeServiceConfig failed");
CloseServiceHandle(schService);
schService = NULL;
}
} else {
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
schService = CreateServiceW(schSCManager,
mpm_service_name_w,
display_name_w,
SERVICE_ALL_ACCESS,
SERVICE_WIN32_OWN_PROCESS,
SERVICE_AUTO_START,
SERVICE_ERROR_NORMAL,
launch_cmd_w,
NULL,
NULL,
L"Tcpip\0Afd\0",
NULL,
NULL);
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
schService = CreateService(schSCManager,
mpm_service_name,
mpm_display_name,
SERVICE_ALL_ACCESS,
SERVICE_WIN32_OWN_PROCESS,
SERVICE_AUTO_START,
SERVICE_ERROR_NORMAL,
launch_cmd,
NULL,
NULL,
"Tcpip\0Afd\0",
NULL,
NULL);
}
#endif
if (!schService) {
rv = apr_get_os_error();
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(00370) "Failed to create the '%s' service",
mpm_display_name);
CloseServiceHandle(schSCManager);
return (rv);
}
}
CloseServiceHandle(schService);
CloseServiceHandle(schSCManager);
set_service_description();
apr_snprintf(key_name, sizeof(key_name), SERVICEPARAMS, mpm_service_name);
rv = ap_regkey_open(&key, AP_REGKEY_LOCAL_MACHINE, key_name,
APR_READ | APR_WRITE | APR_CREATE, pconf);
if (rv == APR_SUCCESS) {
rv = ap_regkey_value_array_set(key, "ConfigArgs", argc, argv, pconf);
ap_regkey_close(key);
}
if (rv != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(00371) "Failed to store ConfigArgs for the "
"'%s' service in the registry.", mpm_display_name);
return (rv);
}
fprintf(stderr, "The '%s' service is successfully installed.\n",
mpm_display_name);
return APR_SUCCESS;
}
apr_status_t mpm_service_uninstall(void) {
apr_status_t rv;
SC_HANDLE schService;
SC_HANDLE schSCManager;
fprintf(stderr, "Removing the '%s' service\n", mpm_display_name);
schSCManager = OpenSCManager(NULL, NULL,
SC_MANAGER_CONNECT);
if (!schSCManager) {
rv = apr_get_os_error();
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(10009) "Failed to open the Windows service "
"manager, perhaps you forgot to log in as Adminstrator?");
return (rv);
}
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
schService = OpenServiceW(schSCManager, mpm_service_name_w, DELETE);
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
schService = OpenService(schSCManager, mpm_service_name, DELETE);
}
#endif
if (!schService) {
rv = apr_get_os_error();
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(10010) "Failed to open the '%s' service",
mpm_display_name);
return (rv);
}
if (DeleteService(schService) == 0) {
rv = apr_get_os_error();
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(00374) "Failed to delete the '%s' service",
mpm_display_name);
return (rv);
}
CloseServiceHandle(schService);
CloseServiceHandle(schSCManager);
fprintf(stderr, "The '%s' service has been removed successfully.\n",
mpm_display_name);
return APR_SUCCESS;
}
static int signal_service_transition(SC_HANDLE schService, DWORD signal,
DWORD pending, DWORD complete) {
if (signal && !ControlService(schService, signal, &globdat.ssStatus))
return FALSE;
do {
Sleep(1000);
if (!QueryServiceStatus(schService, &globdat.ssStatus))
return FALSE;
} while (globdat.ssStatus.dwCurrentState == pending);
return (globdat.ssStatus.dwCurrentState == complete);
}
apr_status_t mpm_service_start(apr_pool_t *ptemp, int argc,
const char * const * argv) {
apr_status_t rv;
SC_HANDLE schService;
SC_HANDLE schSCManager;
fprintf(stderr, "Starting the '%s' service\n", mpm_display_name);
schSCManager = OpenSCManager(NULL, NULL,
SC_MANAGER_CONNECT);
if (!schSCManager) {
rv = apr_get_os_error();
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(10011) "Failed to open the Windows service "
"manager, perhaps you forgot to log in as Adminstrator?");
return (rv);
}
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
schService = OpenServiceW(schSCManager, mpm_service_name_w,
SERVICE_START | SERVICE_QUERY_STATUS);
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
schService = OpenService(schSCManager, mpm_service_name,
SERVICE_START | SERVICE_QUERY_STATUS);
}
#endif
if (!schService) {
rv = apr_get_os_error();
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, rv, NULL,
APLOGNO(10012) "Failed to open the '%s' service",
mpm_display_name);
CloseServiceHandle(schSCManager);
return (rv);
}
if (QueryServiceStatus(schService, &globdat.ssStatus)
&& (globdat.ssStatus.dwCurrentState == SERVICE_RUNNING)) {
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP, 0, NULL,
APLOGNO(00377) "The '%s' service is already started!",
mpm_display_name);
CloseServiceHandle(schService);
CloseServiceHandle(schSCManager);
return 0;
}
rv = APR_EINIT;
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
LPWSTR *start_argv_w = malloc((argc + 1) * sizeof(LPCWSTR));
int i;
for (i = 0; i < argc; ++i) {
apr_size_t slen = strlen(argv[i]) + 1;
apr_size_t wslen = slen;
start_argv_w[i] = malloc(wslen * sizeof(WCHAR));
rv = apr_conv_utf8_to_ucs2(argv[i], &slen, start_argv_w[i], &wslen);
if (rv != APR_SUCCESS)
return rv;
else if (slen)
return APR_ENAMETOOLONG;
}
start_argv_w[argc] = NULL;
if (StartServiceW(schService, argc, start_argv_w)
&& signal_service_transition(schService, 0,
SERVICE_START_PENDING,
SERVICE_RUNNING))
rv = APR_SUCCESS;
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
char **start_argv = malloc((argc + 1) * sizeof(const char *));
memcpy(start_argv, argv, argc * sizeof(const char *));
start_argv[argc] = NULL;
if (StartService(schService, argc, start_argv)
&& signal_service_transition(schService, 0,
SERVICE_START_PENDING,
SERVICE_RUNNING))
rv = APR_SUCCESS;
}
#endif
if (rv != APR_SUCCESS)
rv = apr_get_os_error();
CloseServiceHandle(schService);
CloseServiceHandle(schSCManager);
if (rv == APR_SUCCESS)
fprintf(stderr, "The '%s' service is running.\n", mpm_display_name);
else
ap_log_error(APLOG_MARK, APLOG_CRIT, rv, NULL, APLOGNO(00378)
"Failed to start the '%s' service",
mpm_display_name);
return rv;
}
void mpm_signal_service(apr_pool_t *ptemp, int signal) {
int success = FALSE;
SC_HANDLE schService;
SC_HANDLE schSCManager;
schSCManager = OpenSCManager(NULL, NULL,
SC_MANAGER_CONNECT);
if (!schSCManager) {
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP,
apr_get_os_error(), NULL,
APLOGNO(10013) "Failed to open the Windows service "
"manager, perhaps you forgot to log in as Adminstrator?");
return;
}
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
schService = OpenServiceW(schSCManager, mpm_service_name_w,
SERVICE_INTERROGATE | SERVICE_QUERY_STATUS |
SERVICE_USER_DEFINED_CONTROL |
SERVICE_START | SERVICE_STOP);
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
schService = OpenService(schSCManager, mpm_service_name,
SERVICE_INTERROGATE | SERVICE_QUERY_STATUS |
SERVICE_USER_DEFINED_CONTROL |
SERVICE_START | SERVICE_STOP);
}
#endif
if (schService == NULL) {
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP,
apr_get_os_error(), NULL,
APLOGNO(10014) "Failed to open the '%s' service",
mpm_display_name);
CloseServiceHandle(schSCManager);
return;
}
if (!QueryServiceStatus(schService, &globdat.ssStatus)) {
ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_STARTUP,
apr_get_os_error(), NULL,
APLOGNO(00381) "Query of the '%s' service failed",
mpm_display_name);
CloseServiceHandle(schService);
CloseServiceHandle(schSCManager);
return;
}
if (!signal && (globdat.ssStatus.dwCurrentState == SERVICE_STOPPED)) {
fprintf(stderr, "The '%s' service is not started.\n", mpm_display_name);
CloseServiceHandle(schService);
CloseServiceHandle(schSCManager);
return;
}
fprintf(stderr, signal ? "The '%s' service is restarting.\n"
: "The '%s' service is stopping.\n",
mpm_display_name);
if (!signal)
success = signal_service_transition(schService,
SERVICE_CONTROL_STOP,
SERVICE_STOP_PENDING,
SERVICE_STOPPED);
else if (globdat.ssStatus.dwCurrentState == SERVICE_STOPPED) {
mpm_service_start(ptemp, 0, NULL);
CloseServiceHandle(schService);
CloseServiceHandle(schSCManager);
return;
} else
success = signal_service_transition(schService,
SERVICE_APACHE_RESTART,
SERVICE_START_PENDING,
SERVICE_RUNNING);
CloseServiceHandle(schService);
CloseServiceHandle(schSCManager);
if (success)
fprintf(stderr, signal ? "The '%s' service has restarted.\n"
: "The '%s' service has stopped.\n",
mpm_display_name);
else
fprintf(stderr, signal ? "Failed to restart the '%s' service.\n"
: "Failed to stop the '%s' service.\n",
mpm_display_name);
}
