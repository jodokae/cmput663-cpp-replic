#if !defined(WINCONFIG_H)
#define WINCONFIG_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <memory.h>
#include <string.h>
#define XML_NS 1
#define XML_DTD 1
#define XML_CONTEXT_BYTES 1024
#define BYTEORDER 1234
#define HAVE_MEMMOVE
#endif