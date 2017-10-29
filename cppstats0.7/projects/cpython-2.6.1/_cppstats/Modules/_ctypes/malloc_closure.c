#include <Python.h>
#include <ffi.h>
#if defined(MS_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif
#include "ctypes.h"
#define BLOCKSIZE _pagesize
typedef union _tagITEM {
ffi_closure closure;
union _tagITEM *next;
} ITEM;
static ITEM *free_list;
int _pagesize;
static void more_core(void) {
ITEM *item;
int count, i;
#if defined(MS_WIN32)
if (!_pagesize) {
SYSTEM_INFO systeminfo;
GetSystemInfo(&systeminfo);
_pagesize = systeminfo.dwPageSize;
}
#else
if (!_pagesize) {
#if defined(_SC_PAGESIZE)
_pagesize = sysconf(_SC_PAGESIZE);
#else
_pagesize = getpagesize();
#endif
}
#endif
count = BLOCKSIZE / sizeof(ITEM);
#if defined(MS_WIN32)
item = (ITEM *)VirtualAlloc(NULL,
count * sizeof(ITEM),
MEM_COMMIT,
PAGE_EXECUTE_READWRITE);
if (item == NULL)
return;
#else
item = (ITEM *)mmap(NULL,
count * sizeof(ITEM),
PROT_READ | PROT_WRITE | PROT_EXEC,
MAP_PRIVATE | MAP_ANONYMOUS,
-1,
0);
if (item == (void *)MAP_FAILED)
return;
#endif
#if defined(MALLOC_CLOSURE_DEBUG)
printf("block at %p allocated (%d bytes), %d ITEMs\n",
item, count * sizeof(ITEM), count);
#endif
for (i = 0; i < count; ++i) {
item->next = free_list;
free_list = item;
++item;
}
}
void FreeClosure(void *p) {
ITEM *item = (ITEM *)p;
item->next = free_list;
free_list = item;
}
void *MallocClosure(void) {
ITEM *item;
if (!free_list)
more_core();
if (!free_list)
return NULL;
item = free_list;
free_list = item->next;
return item;
}