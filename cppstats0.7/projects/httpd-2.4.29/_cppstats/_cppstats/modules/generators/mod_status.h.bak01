#if !defined(MOD_STATUS_H)
#define MOD_STATUS_H
#include "ap_config.h"
#include "httpd.h"
#define AP_STATUS_SHORT (0x1)
#define AP_STATUS_NOTABLE (0x2)
#define AP_STATUS_EXTENDED (0x4)
#if !defined(WIN32)
#define STATUS_DECLARE(type) type
#define STATUS_DECLARE_NONSTD(type) type
#define STATUS_DECLARE_DATA
#elif defined(STATUS_DECLARE_STATIC)
#define STATUS_DECLARE(type) type __stdcall
#define STATUS_DECLARE_NONSTD(type) type
#define STATUS_DECLARE_DATA
#elif defined(STATUS_DECLARE_EXPORT)
#define STATUS_DECLARE(type) __declspec(dllexport) type __stdcall
#define STATUS_DECLARE_NONSTD(type) __declspec(dllexport) type
#define STATUS_DECLARE_DATA __declspec(dllexport)
#else
#define STATUS_DECLARE(type) __declspec(dllimport) type __stdcall
#define STATUS_DECLARE_NONSTD(type) __declspec(dllimport) type
#define STATUS_DECLARE_DATA __declspec(dllimport)
#endif
APR_DECLARE_EXTERNAL_HOOK(ap, STATUS, int, status_hook,
(request_rec *r, int flags))
#endif
