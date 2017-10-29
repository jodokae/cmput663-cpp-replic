#if !defined(MOD_AUTHZ_DBD_H)
#define MOD_AUTHZ_DBD_H
#include "httpd.h"
#if !defined(WIN32)
#define AUTHZ_DBD_DECLARE(type) type
#define AUTHZ_DBD_DECLARE_NONSTD(type) type
#define AUTHZ_DBD_DECLARE_DATA
#elif defined(AUTHZ_DBD_DECLARE_STATIC)
#define AUTHZ_DBD_DECLARE(type) type __stdcall
#define AUTHZ_DBD_DECLARE_NONSTD(type) type
#define AUTHZ_DBD_DECLARE_DATA
#elif defined(AUTHZ_DBD_DECLARE_EXPORT)
#define AUTHZ_DBD_DECLARE(type) __declspec(dllexport) type __stdcall
#define AUTHZ_DBD_DECLARE_NONSTD(type) __declspec(dllexport) type
#define AUTHZ_DBD_DECLARE_DATA __declspec(dllexport)
#else
#define AUTHZ_DBD_DECLARE(type) __declspec(dllimport) type __stdcall
#define AUTHZ_DBD_DECLARE_NONSTD(type) __declspec(dllimport) type
#define AUTHZ_DBD_DECLARE_DATA __declspec(dllimport)
#endif
APR_DECLARE_EXTERNAL_HOOK(authz_dbd, AUTHZ_DBD, int, client_login,
(request_rec *r, int code, const char *action))
#endif