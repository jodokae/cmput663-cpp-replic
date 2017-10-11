/*****************************************************************
This file should be kept compatible with Python 2.3, see PEP 291.
*****************************************************************/
#if !defined(_CTYPES_DLFCN_H_)
#define _CTYPES_DLFCN_H_
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */
#if !defined(MS_WIN32)
#include <dlfcn.h>
#if !defined(CTYPES_DARWIN_DLFCN)
#define ctypes_dlsym dlsym
#define ctypes_dlerror dlerror
#define ctypes_dlopen dlopen
#define ctypes_dlclose dlclose
#define ctypes_dladdr dladdr
#endif /* !CTYPES_DARWIN_DLFCN */
#endif /* !MS_WIN32 */
#if defined(__cplusplus)
}
#endif /* __cplusplus */
#endif /* _CTYPES_DLFCN_H_ */
