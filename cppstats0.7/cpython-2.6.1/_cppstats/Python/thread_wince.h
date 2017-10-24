#include <windows.h>
#include <limits.h>
#include <pydebug.h>
long PyThread_get_thread_ident(void);
static void PyThread__init_thread(void) {
}
long PyThread_start_new_thread(void (*func)(void *), void *arg) {
long rv;
int success = -1;
dprintf(("%ld: PyThread_start_new_thread called\n", PyThread_get_thread_ident()));
if (!initialized)
PyThread_init_thread();
rv = _beginthread(func, 0, arg);
if (rv != -1) {
success = 0;
dprintf(("%ld: PyThread_start_new_thread succeeded:\n", PyThread_get_thread_ident()));
}
return success;
}
long PyThread_get_thread_ident(void) {
if (!initialized)
PyThread_init_thread();
return GetCurrentThreadId();
}
static void do_PyThread_exit_thread(int no_cleanup) {
dprintf(("%ld: do_PyThread_exit_thread called\n", PyThread_get_thread_ident()));
if (!initialized)
if (no_cleanup)
exit(0);
else
exit(0);
_endthread();
}
void PyThread_exit_thread(void) {
do_PyThread_exit_thread(0);
}
void PyThread__exit_thread(void) {
do_PyThread_exit_thread(1);
}
#if !defined(NO_EXIT_PROG)
static void do_PyThread_exit_prog(int status, int no_cleanup) {
dprintf(("PyThread_exit_prog(%d) called\n", status));
if (!initialized)
if (no_cleanup)
_exit(status);
else
exit(status);
}
void PyThread_exit_prog(int status) {
do_PyThread_exit_prog(status, 0);
}
void PyThread__exit_prog(int status) {
do_PyThread_exit_prog(status, 1);
}
#endif
PyThread_type_lock PyThread_allocate_lock(void) {
HANDLE aLock;
dprintf(("PyThread_allocate_lock called\n"));
if (!initialized)
PyThread_init_thread();
aLock = CreateEvent(NULL,
0,
1,
NULL);
dprintf(("%ld: PyThread_allocate_lock() -> %p\n", PyThread_get_thread_ident(), aLock));
return (PyThread_type_lock) aLock;
}
void PyThread_free_lock(PyThread_type_lock aLock) {
dprintf(("%ld: PyThread_free_lock(%p) called\n", PyThread_get_thread_ident(),aLock));
CloseHandle(aLock);
}
int PyThread_acquire_lock(PyThread_type_lock aLock, int waitflag) {
int success = 1;
DWORD waitResult;
dprintf(("%ld: PyThread_acquire_lock(%p, %d) called\n", PyThread_get_thread_ident(),aLock, waitflag));
#if !defined(DEBUG)
waitResult = WaitForSingleObject(aLock, (waitflag ? INFINITE : 0));
#else
while (TRUE) {
waitResult = WaitForSingleObject(aLock, waitflag ? 3000 : 0);
if (waitflag==0 || (waitflag && waitResult == WAIT_OBJECT_0))
break;
}
#endif
if (waitResult != WAIT_OBJECT_0) {
success = 0;
}
dprintf(("%ld: PyThread_acquire_lock(%p, %d) -> %d\n", PyThread_get_thread_ident(),aLock, waitflag, success));
return success;
}
void PyThread_release_lock(PyThread_type_lock aLock) {
dprintf(("%ld: PyThread_release_lock(%p) called\n", PyThread_get_thread_ident(),aLock));
if (!SetEvent(aLock))
dprintf(("%ld: Could not PyThread_release_lock(%p) error: %l\n", PyThread_get_thread_ident(), aLock, GetLastError()));
}
