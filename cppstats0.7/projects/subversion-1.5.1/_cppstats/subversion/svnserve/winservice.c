#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_errno.h>
#include "svn_error.h"
#include "svn_private_config.h"
#include "winservice.h"
#if defined(WIN32)
#include <assert.h>
#include <winsvc.h>
#define WINSERVICE_SERVICE_NAME "svnserve"
static HANDLE winservice_dispatcher_thread = NULL;
static HANDLE winservice_start_event = NULL;
static SERVICE_STATUS_HANDLE winservice_status_handle = NULL;
static SERVICE_STATUS winservice_status;
#if defined(SVN_DEBUG)
static void dbg_print(const char* text) {
OutputDebugStringA(text);
}
#else
#define dbg_print(text)
#endif
static void winservice_atexit(void);
static void
winservice_update_state(void) {
if (winservice_status_handle != NULL) {
if (!SetServiceStatus(winservice_status_handle, &winservice_status)) {
dbg_print("SetServiceStatus - FAILED\r\n");
}
}
}
static void
winservice_cleanup(void) {
if (winservice_start_event != NULL) {
CloseHandle(winservice_start_event);
winservice_start_event = NULL;
}
if (winservice_dispatcher_thread != NULL) {
dbg_print("winservice_cleanup:"
" waiting for dispatcher thread to exit\r\n");
WaitForSingleObject(winservice_dispatcher_thread, INFINITE);
CloseHandle(winservice_dispatcher_thread);
winservice_dispatcher_thread = NULL;
}
}
static void WINAPI
winservice_handler(DWORD control) {
switch (control) {
case SERVICE_CONTROL_INTERROGATE:
dbg_print("SERVICE_CONTROL_INTERROGATE\r\n");
winservice_update_state();
break;
case SERVICE_CONTROL_STOP:
dbg_print("SERVICE_CONTROL_STOP\r\n");
winservice_status.dwCurrentState = SERVICE_STOP_PENDING;
winservice_update_state();
winservice_notify_stop();
break;
}
}
static void WINAPI
winservice_service_main(DWORD argc, LPTSTR *argv) {
DWORD error;
assert(winservice_start_event != NULL);
winservice_status_handle =
RegisterServiceCtrlHandler(WINSERVICE_SERVICE_NAME, winservice_handler);
if (winservice_status_handle == NULL) {
error = GetLastError();
dbg_print("RegisterServiceCtrlHandler FAILED\r\n");
winservice_status.dwWin32ExitCode = error;
SetEvent(winservice_start_event);
return;
}
winservice_status.dwCurrentState = SERVICE_START_PENDING;
winservice_status.dwWin32ExitCode = ERROR_SUCCESS;
winservice_update_state();
dbg_print("winservice_service_main: service is starting\r\n");
SetEvent(winservice_start_event);
}
static const SERVICE_TABLE_ENTRY winservice_service_table[] = {
{ WINSERVICE_SERVICE_NAME, winservice_service_main },
{ NULL, NULL }
};
static DWORD WINAPI
winservice_dispatcher_thread_routine(PVOID arg) {
dbg_print("winservice_dispatcher_thread_routine: starting\r\n");
if (!StartServiceCtrlDispatcher(winservice_service_table)) {
DWORD error = GetLastError();
dbg_print("dispatcher: FAILED to connect to SCM\r\n");
return error;
}
dbg_print("dispatcher: SCM is done using this process -- exiting\r\n");
return ERROR_SUCCESS;
}
svn_error_t *
winservice_start(void) {
HANDLE handles[2];
DWORD thread_id;
DWORD error_code;
apr_status_t apr_status;
DWORD wait_status;
dbg_print("winservice_start: starting svnserve as a service...\r\n");
ZeroMemory(&winservice_status, sizeof(winservice_status));
winservice_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
winservice_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
winservice_status.dwCurrentState = SERVICE_STOPPED;
winservice_start_event = CreateEvent(NULL, FALSE, FALSE, NULL);
if (winservice_start_event == NULL) {
apr_status = apr_get_os_error();
return svn_error_wrap_apr(apr_status,
_("Failed to create winservice_start_event"));
}
winservice_dispatcher_thread =
(HANDLE)_beginthreadex(NULL, 0, winservice_dispatcher_thread_routine,
NULL, 0, &thread_id);
if (winservice_dispatcher_thread == NULL) {
apr_status = apr_get_os_error();
winservice_cleanup();
return svn_error_wrap_apr(apr_status,
_("The service failed to start"));
}
handles[0] = winservice_start_event;
handles[1] = winservice_dispatcher_thread;
wait_status = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
switch (wait_status) {
case WAIT_OBJECT_0:
dbg_print("winservice_start: service is now starting\r\n");
CloseHandle(winservice_start_event);
winservice_start_event = NULL;
atexit(winservice_atexit);
return SVN_NO_ERROR;
case WAIT_OBJECT_0+1:
dbg_print("winservice_start: dispatcher thread has failed\r\n");
if (GetExitCodeThread(winservice_dispatcher_thread, &error_code)) {
dbg_print("winservice_start: dispatcher thread failed\r\n");
if (error_code == ERROR_SUCCESS)
error_code = ERROR_INTERNAL_ERROR;
} else {
error_code = ERROR_INTERNAL_ERROR;
}
CloseHandle(winservice_dispatcher_thread);
winservice_dispatcher_thread = NULL;
winservice_cleanup();
return svn_error_wrap_apr
(APR_FROM_OS_ERROR(error_code),
_("Failed to connect to Service Control Manager"));
default:
apr_status = apr_get_os_error();
dbg_print("winservice_start: WaitForMultipleObjects failed!\r\n");
winservice_cleanup();
return svn_error_wrap_apr
(apr_status, _("The service failed to start; an internal error"
" occurred while starting the service"));
}
}
void
winservice_running(void) {
winservice_status.dwCurrentState = SERVICE_RUNNING;
winservice_update_state();
dbg_print("winservice_notify_running: service is now running\r\n");
}
static void
winservice_stop(DWORD exit_code) {
dbg_print("winservice_stop - notifying SCM that service has stopped\r\n");
winservice_status.dwCurrentState = SERVICE_STOPPED;
winservice_status.dwWin32ExitCode = exit_code;
winservice_update_state();
if (winservice_dispatcher_thread != NULL) {
dbg_print("waiting for dispatcher thread to exit...\r\n");
WaitForSingleObject(winservice_dispatcher_thread, INFINITE);
dbg_print("dispatcher thread has exited.\r\n");
CloseHandle(winservice_dispatcher_thread);
winservice_dispatcher_thread = NULL;
} else {
exit_code = winservice_status.dwWin32ExitCode;
dbg_print("dispatcher thread was not running\r\n");
}
if (winservice_start_event != NULL) {
CloseHandle(winservice_start_event);
winservice_start_event = NULL;
}
dbg_print("winservice_stop - service has stopped\r\n");
}
static void
winservice_atexit(void) {
dbg_print("winservice_atexit - stopping\r\n");
winservice_stop(ERROR_SUCCESS);
}
svn_boolean_t
winservice_is_stopping(void) {
return (winservice_status.dwCurrentState == SERVICE_STOP_PENDING);
}
#endif