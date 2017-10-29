#if defined(WIN32)
#if !defined(AP_OS_H)
#define AP_OS_H
#include "apr_pools.h"
#include <io.h>
#include <fcntl.h>
#if defined(_WIN64)
#define PLATFORM "Win64"
#else
#define PLATFORM "Win32"
#endif
#define AP_PLATFORM_REWRITE_ARGS_HOOK NULL
#define HAVE_DRIVE_LETTERS
#define HAVE_UNC_PATHS
#define CASE_BLIND_FILESYSTEM
#include <stddef.h>
#include <stdlib.h>
#if defined(__cplusplus)
extern "C" {
#endif
AP_DECLARE_DATA extern int ap_real_exit_code;
#define exit(status) ((exit)((ap_real_exit_code==2) ? (ap_real_exit_code = (status)) : ((ap_real_exit_code = 0), (status))))
#if defined(AP_DECLARE_EXPORT)
AP_DECLARE(apr_status_t) ap_os_proc_filepath(char **binpath, apr_pool_t *p);
typedef enum {
AP_DLL_WINBASEAPI = 0,
AP_DLL_WINADVAPI = 1,
AP_DLL_WINSOCKAPI = 2,
AP_DLL_WINSOCK2API = 3,
AP_DLL_defined = 4
} ap_dlltoken_e;
FARPROC ap_load_dll_func(ap_dlltoken_e fnLib, char* fnName, int ordinal);
PSECURITY_ATTRIBUTES GetNullACL(void);
void CleanNullACL(void *sa);
#define AP_DECLARE_LATE_DLL_FUNC(lib, rettype, calltype, fn, ord, args, names) typedef rettype (calltype *ap_winapi_fpt_##fn) args; static ap_winapi_fpt_##fn ap_winapi_pfn_##fn = NULL; static APR_INLINE rettype ap_winapi_##fn args { if (!ap_winapi_pfn_##fn) ap_winapi_pfn_##fn = (ap_winapi_fpt_##fn) ap_load_dll_func(lib, #fn, ord); return (*(ap_winapi_pfn_##fn)) names; };
AP_DECLARE_LATE_DLL_FUNC(AP_DLL_WINADVAPI, BOOL, WINAPI, ChangeServiceConfig2A, 0, (
SC_HANDLE hService,
DWORD dwInfoLevel,
LPVOID lpInfo),
(hService, dwInfoLevel, lpInfo));
#undef ChangeServiceConfig2
#define ChangeServiceConfig2 ap_winapi_ChangeServiceConfig2A
AP_DECLARE_LATE_DLL_FUNC(AP_DLL_WINBASEAPI, BOOL, WINAPI, CancelIo, 0, (
IN HANDLE hFile),
(hFile));
#undef CancelIo
#define CancelIo ap_winapi_CancelIo
AP_DECLARE_LATE_DLL_FUNC(AP_DLL_WINBASEAPI, DWORD, WINAPI, RegisterServiceProcess, 0, (
DWORD dwProcessId,
DWORD dwType),
(dwProcessId, dwType));
#define RegisterServiceProcess ap_winapi_RegisterServiceProcess
#endif
#if defined(__cplusplus)
}
#endif
#endif
#endif