#if defined(MACH_C_THREADS)
#include <mach/cthreads.h>
#endif
#if defined(HURD_C_THREADS)
#include <cthreads.h>
#endif
static void
PyThread__init_thread(void) {
#if !defined(HURD_C_THREADS)
cthread_init();
#else
;
#endif
}
long
PyThread_start_new_thread(void (*func)(void *), void *arg) {
int success = 0;
dprintf(("PyThread_start_new_thread called\n"));
if (!initialized)
PyThread_init_thread();
cthread_detach(cthread_fork((cthread_fn_t) func, arg));
return success < 0 ? -1 : 0;
}
long
PyThread_get_thread_ident(void) {
if (!initialized)
PyThread_init_thread();
return (long) cthread_self();
}
static void
do_PyThread_exit_thread(int no_cleanup) {
dprintf(("PyThread_exit_thread called\n"));
if (!initialized)
if (no_cleanup)
_exit(0);
else
exit(0);
cthread_exit(0);
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
static
void do_PyThread_exit_prog(int status, int no_cleanup) {
dprintf(("PyThread_exit_prog(%d) called\n", status));
if (!initialized)
if (no_cleanup)
_exit(status);
else
exit(status);
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
mutex_t lock;
dprintf(("PyThread_allocate_lock called\n"));
if (!initialized)
PyThread_init_thread();
lock = mutex_alloc();
if (mutex_init(lock)) {
perror("mutex_init");
free((void *) lock);
lock = 0;
}
dprintf(("PyThread_allocate_lock() -> %p\n", lock));
return (PyThread_type_lock) lock;
}
void
PyThread_free_lock(PyThread_type_lock lock) {
dprintf(("PyThread_free_lock(%p) called\n", lock));
mutex_free(lock);
}
int
PyThread_acquire_lock(PyThread_type_lock lock, int waitflag) {
int success = FALSE;
dprintf(("PyThread_acquire_lock(%p, %d) called\n", lock, waitflag));
if (waitflag) {
mutex_lock((mutex_t)lock);
success = TRUE;
} else {
success = mutex_try_lock((mutex_t)lock);
}
dprintf(("PyThread_acquire_lock(%p, %d) -> %d\n", lock, waitflag, success));
return success;
}
void
PyThread_release_lock(PyThread_type_lock lock) {
dprintf(("PyThread_release_lock(%p) called\n", lock));
mutex_unlock((mutex_t )lock);
}