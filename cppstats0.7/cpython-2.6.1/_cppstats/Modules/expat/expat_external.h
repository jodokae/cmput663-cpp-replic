#if !defined(Expat_External_INCLUDED)
#define Expat_External_INCLUDED 1
#include "pyexpatns.h"
#if defined(_MSC_EXTENSIONS) && !defined(__BEOS__) && !defined(__CYGWIN__)
#define XML_USE_MSC_EXTENSIONS 1
#endif
#if !defined(XMLCALL)
#if defined(XML_USE_MSC_EXTENSIONS)
#define XMLCALL __cdecl
#elif defined(__GNUC__) && defined(__i386)
#define XMLCALL __attribute__((cdecl))
#else
#define XMLCALL
#endif
#endif
#if !defined(XML_STATIC) && !defined(XMLIMPORT)
#if !defined(XML_BUILDING_EXPAT)
#if defined(XML_USE_MSC_EXTENSIONS)
#define XMLIMPORT __declspec(dllimport)
#endif
#endif
#endif
#if !defined(XMLIMPORT)
#define XMLIMPORT
#endif
#define XMLPARSEAPI(type) XMLIMPORT type XMLCALL
#if defined(__cplusplus)
extern "C" {
#endif
#if defined(XML_UNICODE_WCHAR_T)
#define XML_UNICODE
#endif
#if defined(XML_UNICODE)
#if defined(XML_UNICODE_WCHAR_T)
typedef wchar_t XML_Char;
typedef wchar_t XML_LChar;
#else
typedef unsigned short XML_Char;
typedef char XML_LChar;
#endif
#else
typedef char XML_Char;
typedef char XML_LChar;
#endif
#if defined(XML_LARGE_SIZE)
#if defined(XML_USE_MSC_EXTENSIONS) && _MSC_VER < 1400
typedef __int64 XML_Index;
typedef unsigned __int64 XML_Size;
#else
typedef long long XML_Index;
typedef unsigned long long XML_Size;
#endif
#else
typedef long XML_Index;
typedef unsigned long XML_Size;
#endif
#if defined(__cplusplus)
}
#endif
#endif
