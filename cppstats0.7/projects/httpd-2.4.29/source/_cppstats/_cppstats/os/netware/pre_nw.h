#if !defined(__pre_nw__)
#define __pre_nw__
#include <stdint.h>
#if !defined(__GNUC__)
#pragma precompile_target "precomp.mch"
#endif
#define NETWARE
#define N_PLAT_NLM
#define __NETWARE__
#define FAR
#define far
#define cdecl
#if !defined(__GNUC__)
#if (__option(cplusplus) && __option(wchar_type))
#define _WCHAR_T
#endif
#endif
#define DECIMAL_DIG 17
#if !defined(__int64)
#define __int64 long long
#endif
#define AP_MAX_INCLUDE_DEPTH 48
#endif
