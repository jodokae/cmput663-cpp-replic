#if !defined(AP_CONFIG_H)
#define AP_CONFIG_H
#include "ap_hooks.h"
#if defined(DOXYGEN)
#define AP_DECLARE_STATIC
#define AP_DECLARE_EXPORT
#endif
#if !defined(WIN32)
#define AP_DECLARE(type) type
#define AP_DECLARE_NONSTD(type) type
#define AP_DECLARE_DATA
#elif defined(AP_DECLARE_STATIC)
#define AP_DECLARE(type) type __stdcall
#define AP_DECLARE_NONSTD(type) type
#define AP_DECLARE_DATA
#elif defined(AP_DECLARE_EXPORT)
#define AP_DECLARE(type) __declspec(dllexport) type __stdcall
#define AP_DECLARE_NONSTD(type) __declspec(dllexport) type
#define AP_DECLARE_DATA __declspec(dllexport)
#else
#define AP_DECLARE(type) __declspec(dllimport) type __stdcall
#define AP_DECLARE_NONSTD(type) __declspec(dllimport) type
#define AP_DECLARE_DATA __declspec(dllimport)
#endif
#if !defined(WIN32) || defined(AP_MODULE_DECLARE_STATIC)
#if defined(WIN32)
#define AP_MODULE_DECLARE(type) type __stdcall
#else
#define AP_MODULE_DECLARE(type) type
#endif
#define AP_MODULE_DECLARE_NONSTD(type) type
#define AP_MODULE_DECLARE_DATA
#else
#define AP_MODULE_DECLARE_EXPORT
#define AP_MODULE_DECLARE(type) __declspec(dllexport) type __stdcall
#define AP_MODULE_DECLARE_NONSTD(type) __declspec(dllexport) type
#define AP_MODULE_DECLARE_DATA __declspec(dllexport)
#endif
#include "os.h"
#if (!defined(WIN32) && !defined(NETWARE)) || defined(__MINGW32__)
#include "ap_config_auto.h"
#endif
#include "ap_config_layout.h"
#if !defined(DEFAULT_PIDLOG)
#define DEFAULT_PIDLOG DEFAULT_REL_RUNTIMEDIR "/httpd.pid"
#endif
#if defined(NETWARE)
#define AP_NONBLOCK_WHEN_MULTI_LISTEN 1
#endif
#if defined(AP_ENABLE_DTRACE) && HAVE_SYS_SDT_H
#include <sys/sdt.h>
#else
#undef _DTRACE_VERSION
#endif
#if defined(_DTRACE_VERSION)
#include "apache_probes.h"
#else
#include "apache_noprobes.h"
#endif
#if APR_HAS_OTHER_CHILD
#define AP_HAVE_RELIABLE_PIPED_LOGS TRUE
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define AP_HAVE_C99
#endif
#if (defined(__GNUC__) && !defined(__cplusplus)) || defined(AP_HAVE_C99)
#define AP_HAVE_DESIGNATED_INITIALIZER
#endif
#if !defined(__has_attribute)
#define __has_attribute(x) 0
#endif
#if (defined(__GNUC__) && __GNUC__ >= 4) || __has_attribute(sentinel)
#define AP_FN_ATTR_SENTINEL __attribute__((sentinel))
#else
#define AP_FN_ATTR_SENTINEL
#endif
#if ( defined(__GNUC__) && (__GNUC__ >= 4 || ( __GNUC__ == 3 && __GNUC_MINOR__ >= 4))) || __has_attribute(warn_unused_result)
#define AP_FN_ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define AP_FN_ATTR_WARN_UNUSED_RESULT
#endif
#if ( defined(__GNUC__) && (__GNUC__ >= 4 && __GNUC_MINOR__ >= 3)) || __has_attribute(alloc_size)
#define AP_FN_ATTR_ALLOC_SIZE(x) __attribute__((alloc_size(x)))
#define AP_FN_ATTR_ALLOC_SIZE2(x,y) __attribute__((alloc_size(x,y)))
#else
#define AP_FN_ATTR_ALLOC_SIZE(x)
#define AP_FN_ATTR_ALLOC_SIZE2(x,y)
#endif
#endif