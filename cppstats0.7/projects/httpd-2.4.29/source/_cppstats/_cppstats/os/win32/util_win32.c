#include "apr_strings.h"
#include "arch/win32/apr_arch_file_io.h"
#include "arch/win32/apr_arch_misc.h"
#include "httpd.h"
#include "http_log.h"
#include "ap_mpm.h"
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
AP_DECLARE(apr_status_t) ap_os_proc_filepath(char **binpath, apr_pool_t *p) {
apr_wchar_t wbinpath[APR_PATH_MAX];
#if APR_HAS_UNICODE_FS
IF_WIN_OS_IS_UNICODE {
apr_size_t binlen;
apr_size_t wbinlen;
apr_status_t rv;
if (!GetModuleFileNameW(NULL, wbinpath, sizeof(wbinpath)
/ sizeof(apr_wchar_t))) {
return apr_get_os_error();
}
wbinlen = wcslen(wbinpath) + 1;
binlen = (wbinlen - 1) * 3 + 1;
*binpath = apr_palloc(p, binlen);
rv = apr_conv_ucs2_to_utf8(wbinpath, &wbinlen, *binpath, &binlen);
if (rv != APR_SUCCESS)
return rv;
else if (wbinlen)
return APR_ENAMETOOLONG;
}
#endif
#if APR_HAS_ANSI_FS
ELSE_WIN_OS_IS_ANSI {
char *pathbuf = (char*) wbinpath;
if (!GetModuleFileName(NULL, pathbuf, sizeof(wbinpath))) {
return apr_get_os_error();
}
*binpath = apr_pstrdup(p, pathbuf);
}
#endif
return APR_SUCCESS;
}
AP_DECLARE(apr_status_t) ap_os_create_privileged_process(
const request_rec *r,
apr_proc_t *newproc, const char *progname,
const char * const *args,
const char * const *env,
apr_procattr_t *attr, apr_pool_t *p) {
return apr_proc_create(newproc, progname, args, env, attr, p);
}
static const char* const lateDllName[DLL_defined] = {
"kernel32", "advapi32", "mswsock", "ws2_32"
};
static HMODULE lateDllHandle[DLL_defined] = {
NULL, NULL, NULL, NULL
};
FARPROC ap_load_dll_func(ap_dlltoken_e fnLib, char* fnName, int ordinal) {
if (!lateDllHandle[fnLib]) {
lateDllHandle[fnLib] = LoadLibrary(lateDllName[fnLib]);
if (!lateDllHandle[fnLib])
return NULL;
}
if (ordinal)
return GetProcAddress(lateDllHandle[fnLib], (char *) ordinal);
else
return GetProcAddress(lateDllHandle[fnLib], fnName);
}
PSECURITY_ATTRIBUTES GetNullACL(void) {
PSECURITY_DESCRIPTOR pSD;
PSECURITY_ATTRIBUTES sa;
sa = (PSECURITY_ATTRIBUTES) LocalAlloc(LPTR, sizeof(SECURITY_ATTRIBUTES));
sa->nLength = sizeof(SECURITY_ATTRIBUTES);
pSD = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
sa->lpSecurityDescriptor = pSD;
if (pSD == NULL || sa == NULL) {
return NULL;
}
apr_set_os_error(0);
if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION)
|| apr_get_os_error()) {
LocalFree( pSD );
LocalFree( sa );
return NULL;
}
if (!SetSecurityDescriptorDacl(pSD, TRUE, (PACL) NULL, FALSE)
|| apr_get_os_error()) {
LocalFree( pSD );
LocalFree( sa );
return NULL;
}
sa->bInheritHandle = FALSE;
return sa;
}
void CleanNullACL(void *sa) {
if (sa) {
LocalFree(((PSECURITY_ATTRIBUTES)sa)->lpSecurityDescriptor);
LocalFree(sa);
}
}
