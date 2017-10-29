#include <windows.h>
#include <limits.h>
#if defined(HAVE_PROCESS_H)
#include <process.h>
#endif
typedef struct NRMUTEX {
LONG owned ;
DWORD thread_id ;
HANDLE hevent ;
} NRMUTEX, *PNRMUTEX ;
BOOL
InitializeNonRecursiveMutex(PNRMUTEX mutex) {
mutex->owned = -1 ;
mutex->thread_id = 0 ;
mutex->hevent = CreateEvent(NULL, FALSE, FALSE, NULL) ;
return mutex->hevent != NULL ;
}
VOID
DeleteNonRecursiveMutex(PNRMUTEX mutex) {
CloseHandle(mutex->hevent) ;
mutex->hevent = NULL ;
}
DWORD
EnterNonRecursiveMutex(PNRMUTEX mutex, BOOL wait) {
DWORD ret ;
if (!wait) {
if (InterlockedCompareExchange(&mutex->owned, 0, -1) != -1)
return WAIT_TIMEOUT ;
ret = WAIT_OBJECT_0 ;
} else
ret = InterlockedIncrement(&mutex->owned) ?
WaitForSingleObject(mutex->hevent, INFINITE) : WAIT_OBJECT_0 ;
mutex->thread_id = GetCurrentThreadId() ;
return ret ;
}
BOOL
LeaveNonRecursiveMutex(PNRMUTEX mutex) {
mutex->thread_id = 0 ;
return
InterlockedDecrement(&mutex->owned) < 0 ||
SetEvent(mutex->hevent) ;
}
PNRMUTEX
AllocNonRecursiveMutex(void) {
PNRMUTEX mutex = (PNRMUTEX)malloc(sizeof(NRMUTEX)) ;
if (mutex && !InitializeNonRecursiveMutex(mutex)) {
free(mutex) ;
mutex = NULL ;
}
return mutex ;
}
void
FreeNonRecursiveMutex(PNRMUTEX mutex) {
if (mutex) {
DeleteNonRecursiveMutex(mutex) ;
free(mutex) ;
}
}
long PyThread_get_thread_ident(void);
static void
PyThread__init_thread(void) {
}
typedef struct {
void (*func)(void*);
void *arg;
long id;
HANDLE done;
} callobj;
static int
bootstrap(void *call) {
callobj *obj = (callobj*)call;
void (*func)(void*) = obj->func;
void *arg = obj->arg;
obj->id = PyThread_get_thread_ident();
ReleaseSemaphore(obj->done, 1, NULL);
func(arg);
return 0;
}
long
PyThread_start_new_thread(void (*func)(void *), void *arg) {
Py_uintptr_t rv;
callobj obj;
dprintf(("%ld: PyThread_start_new_thread called\n",
PyThread_get_thread_ident()));
if (!initialized)
PyThread_init_thread();
obj.id = -1;
obj.func = func;
obj.arg = arg;
obj.done = CreateSemaphore(NULL, 0, 1, NULL);
if (obj.done == NULL)
return -1;
rv = _beginthread(bootstrap,
Py_SAFE_DOWNCAST(_pythread_stacksize,
Py_ssize_t, int),
&obj);
if (rv == (Py_uintptr_t)-1) {
dprintf(("%ld: PyThread_start_new_thread failed: %p errno %d\n",
PyThread_get_thread_ident(), (void*)rv, errno));
obj.id = -1;
} else {
dprintf(("%ld: PyThread_start_new_thread succeeded: %p\n",
PyThread_get_thread_ident(), (void*)rv));
WaitForSingleObject(obj.done, INFINITE);
assert(obj.id != -1);
}
CloseHandle((HANDLE)obj.done);
return obj.id;
}
long
PyThread_get_thread_ident(void) {
if (!initialized)
PyThread_init_thread();
return GetCurrentThreadId();
}
static void
do_PyThread_exit_thread(int no_cleanup) {
dprintf(("%ld: PyThread_exit_thread called\n", PyThread_get_thread_ident()));
if (!initialized)
if (no_cleanup)
_exit(0);
else
exit(0);
_endthread();
}
void
PyThread_exit_thread(void) {
do_PyThread_exit_thread(0);
}
void
PyThread__exit_thread(void) {
do_PyThread_exit_thread(1);
}
#if !defined(NO_EXIT_PROG)
static void
do_PyThread_exit_prog(int status, int no_cleanup) {
dprintf(("PyThread_exit_prog(%d) called\n", status));
if (!initialized)
if (no_cleanup)
_exit(status);
else
exit(status);
}
void
PyThread_exit_prog(int status) {
do_PyThread_exit_prog(status, 0);
}
void
PyThread__exit_prog(int status) {
do_PyThread_exit_prog(status, 1);
}
#endif
PyThread_type_lock
PyThread_allocate_lock(void) {
PNRMUTEX aLock;
dprintf(("PyThread_allocate_lock called\n"));
if (!initialized)
PyThread_init_thread();
aLock = AllocNonRecursiveMutex() ;
dprintf(("%ld: PyThread_allocate_lock() -> %p\n", PyThread_get_thread_ident(), aLock));
return (PyThread_type_lock) aLock;
}
void
PyThread_free_lock(PyThread_type_lock aLock) {
dprintf(("%ld: PyThread_free_lock(%p) called\n", PyThread_get_thread_ident(),aLock));
FreeNonRecursiveMutex(aLock) ;
}
int
PyThread_acquire_lock(PyThread_type_lock aLock, int waitflag) {
int success ;
dprintf(("%ld: PyThread_acquire_lock(%p, %d) called\n", PyThread_get_thread_ident(),aLock, waitflag));
success = aLock && EnterNonRecursiveMutex((PNRMUTEX) aLock, (waitflag ? INFINITE : 0)) == WAIT_OBJECT_0 ;
dprintf(("%ld: PyThread_acquire_lock(%p, %d) -> %d\n", PyThread_get_thread_ident(),aLock, waitflag, success));
return success;
}
void
PyThread_release_lock(PyThread_type_lock aLock) {
dprintf(("%ld: PyThread_release_lock(%p) called\n", PyThread_get_thread_ident(),aLock));
if (!(aLock && LeaveNonRecursiveMutex((PNRMUTEX) aLock)))
dprintf(("%ld: Could not PyThread_release_lock(%p) error: %ld\n", PyThread_get_thread_ident(), aLock, GetLastError()));
}
#define THREAD_MIN_STACKSIZE 0x8000
#define THREAD_MAX_STACKSIZE 0x10000000
static int
_pythread_nt_set_stacksize(size_t size) {
if (size == 0) {
_pythread_stacksize = 0;
return 0;
}
if (size >= THREAD_MIN_STACKSIZE && size < THREAD_MAX_STACKSIZE) {
_pythread_stacksize = size;
return 0;
}
return -1;
}
#define THREAD_SET_STACKSIZE(x) _pythread_nt_set_stacksize(x)