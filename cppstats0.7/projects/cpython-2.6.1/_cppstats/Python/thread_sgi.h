#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <ulocks.h>
#include <errno.h>
#define HDR_SIZE 2680
#define MAXPROC 100
static usptr_t *shared_arena;
static ulock_t count_lock;
static ulock_t wait_lock;
static int waiting_for_threads;
static int nthreads;
static int exit_status;
#if !defined(NO_EXIT_PROG)
static int do_exit;
#endif
static int exiting;
static pid_t my_pid;
static struct pidlist {
pid_t parent;
pid_t child;
} pidlist[MAXPROC];
static int maxpidindex;
#if !defined(NO_EXIT_PROG)
static void exit_sig(void) {
d2printf(("exit_sig called\n"));
if (exiting && getpid() == my_pid) {
d2printf(("already exiting\n"));
return;
}
if (do_exit) {
d2printf(("exiting in exit_sig\n"));
#if defined(Py_DEBUG)
if ((thread_debug & 8) == 0)
thread_debug &= ~1;
#endif
PyThread_exit_thread();
}
}
static void maybe_exit(void) {
dprintf(("maybe_exit called\n"));
if (exiting) {
dprintf(("already exiting\n"));
return;
}
PyThread_exit_prog(0);
}
#endif
static void PyThread__init_thread(void) {
#if !defined(NO_EXIT_PROG)
struct sigaction s;
#endif
#if defined(USE_DL)
long addr, size;
#endif
#if defined(USE_DL)
if ((size = usconfig(CONF_INITSIZE, 64*1024)) < 0)
perror("usconfig - CONF_INITSIZE (check)");
if (usconfig(CONF_INITSIZE, size) < 0)
perror("usconfig - CONF_INITSIZE (reset)");
addr = (long) dl_getrange(size + HDR_SIZE);
dprintf(("trying to use addr %p-%p for shared arena\n", addr, addr+size));
errno = 0;
if ((addr = usconfig(CONF_ATTACHADDR, addr)) < 0 && errno != 0)
perror("usconfig - CONF_ATTACHADDR (set)");
#endif
if (usconfig(CONF_INITUSERS, 16) < 0)
perror("usconfig - CONF_INITUSERS");
my_pid = getpid();
#if !defined(NO_EXIT_PROG)
atexit(maybe_exit);
s.sa_handler = exit_sig;
sigemptyset(&s.sa_mask);
s.sa_flags = 0;
sigaction(SIGUSR1, &s, 0);
if (prctl(PR_SETEXITSIG, SIGUSR1) < 0)
perror("prctl - PR_SETEXITSIG");
#endif
if (usconfig(CONF_ARENATYPE, US_SHAREDONLY) < 0)
perror("usconfig - CONF_ARENATYPE");
usconfig(CONF_LOCKTYPE, US_DEBUG);
#if defined(Py_DEBUG)
if (thread_debug & 4)
usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);
else if (thread_debug & 2)
usconfig(CONF_LOCKTYPE, US_DEBUG);
#endif
if ((shared_arena = usinit(tmpnam(0))) == 0)
perror("usinit");
#if defined(USE_DL)
if (usconfig(CONF_ATTACHADDR, addr) < 0)
perror("usconfig - CONF_ATTACHADDR (reset)");
#endif
if ((count_lock = usnewlock(shared_arena)) == NULL)
perror("usnewlock (count_lock)");
(void) usinitlock(count_lock);
if ((wait_lock = usnewlock(shared_arena)) == NULL)
perror("usnewlock (wait_lock)");
dprintf(("arena start: %p, arena size: %ld\n", shared_arena, (long) usconfig(CONF_GETSIZE, shared_arena)));
}
static void clean_threads(void) {
int i, j;
pid_t mypid, pid;
mypid = getpid();
i = 0;
while (i < maxpidindex) {
if (pidlist[i].parent == mypid && (pid = pidlist[i].child) > 0) {
pid = waitpid(pid, 0, WNOHANG);
if (pid > 0) {
pidlist[i] = pidlist[--maxpidindex];
for (j = 0; j < maxpidindex; j++)
if (pidlist[j].parent == pid)
pidlist[j].child = -1;
continue;
}
}
i++;
}
i = 0;
while (i < maxpidindex) {
if (pidlist[i].child == -1) {
pidlist[i] = pidlist[--maxpidindex];
continue;
}
i++;
}
}
long PyThread_start_new_thread(void (*func)(void *), void *arg) {
#if defined(USE_DL)
long addr, size;
static int local_initialized = 0;
#endif
int success = 0;
dprintf(("PyThread_start_new_thread called\n"));
if (!initialized)
PyThread_init_thread();
switch (ussetlock(count_lock)) {
case 0:
return 0;
case -1:
perror("ussetlock (count_lock)");
}
if (maxpidindex >= MAXPROC)
success = -1;
else {
#if defined(USE_DL)
if (!local_initialized) {
if ((size = usconfig(CONF_INITSIZE, 64*1024)) < 0)
perror("usconfig - CONF_INITSIZE (check)");
if (usconfig(CONF_INITSIZE, size) < 0)
perror("usconfig - CONF_INITSIZE (reset)");
addr = (long) dl_getrange(size + HDR_SIZE);
dprintf(("trying to use addr %p-%p for sproc\n",
addr, addr+size));
errno = 0;
if ((addr = usconfig(CONF_ATTACHADDR, addr)) < 0 &&
errno != 0)
perror("usconfig - CONF_ATTACHADDR (set)");
}
#endif
clean_threads();
if ((success = sproc(func, PR_SALL, arg)) < 0)
perror("sproc");
#if defined(USE_DL)
if (!local_initialized) {
if (usconfig(CONF_ATTACHADDR, addr) < 0)
perror("usconfig - CONF_ATTACHADDR (reset)");
local_initialized = 1;
}
#endif
if (success >= 0) {
nthreads++;
pidlist[maxpidindex].parent = getpid();
pidlist[maxpidindex++].child = success;
dprintf(("pidlist[%d] = %d\n",
maxpidindex-1, success));
}
}
if (usunsetlock(count_lock) < 0)
perror("usunsetlock (count_lock)");
return success;
}
long PyThread_get_thread_ident(void) {
return getpid();
}
static void do_PyThread_exit_thread(int no_cleanup) {
dprintf(("PyThread_exit_thread called\n"));
if (!initialized)
if (no_cleanup)
_exit(0);
else
exit(0);
if (ussetlock(count_lock) < 0)
perror("ussetlock (count_lock)");
nthreads--;
if (getpid() == my_pid) {
exiting = 1;
#if !defined(NO_EXIT_PROG)
if (do_exit) {
int i;
clean_threads();
if (nthreads >= 0) {
dprintf(("kill other threads\n"));
for (i = 0; i < maxpidindex; i++)
if (pidlist[i].child > 0)
(void) kill(pidlist[i].child,
SIGKILL);
_exit(exit_status);
}
}
#endif
waiting_for_threads = 1;
if (ussetlock(wait_lock) < 0)
perror("ussetlock (wait_lock)");
for (;;) {
if (nthreads < 0) {
dprintf(("really exit (%d)\n", exit_status));
if (no_cleanup)
_exit(exit_status);
else
exit(exit_status);
}
if (usunsetlock(count_lock) < 0)
perror("usunsetlock (count_lock)");
dprintf(("waiting for other threads (%d)\n", nthreads));
if (ussetlock(wait_lock) < 0)
perror("ussetlock (wait_lock)");
if (ussetlock(count_lock) < 0)
perror("ussetlock (count_lock)");
}
}
if (waiting_for_threads) {
dprintf(("main thread is waiting\n"));
if (usunsetlock(wait_lock) < 0)
perror("usunsetlock (wait_lock)");
}
#if !defined(NO_EXIT_PROG)
else if (do_exit)
(void) kill(my_pid, SIGUSR1);
#endif
if (usunsetlock(count_lock) < 0)
perror("usunsetlock (count_lock)");
_exit(0);
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
do_exit = 1;
exit_status = status;
do_PyThread_exit_thread(no_cleanup);
}
void PyThread_exit_prog(int status) {
do_PyThread_exit_prog(status, 0);
}
void PyThread__exit_prog(int status) {
do_PyThread_exit_prog(status, 1);
}
#endif
PyThread_type_lock PyThread_allocate_lock(void) {
ulock_t lock;
dprintf(("PyThread_allocate_lock called\n"));
if (!initialized)
PyThread_init_thread();
if ((lock = usnewlock(shared_arena)) == NULL)
perror("usnewlock");
(void) usinitlock(lock);
dprintf(("PyThread_allocate_lock() -> %p\n", lock));
return (PyThread_type_lock) lock;
}
void PyThread_free_lock(PyThread_type_lock lock) {
dprintf(("PyThread_free_lock(%p) called\n", lock));
usfreelock((ulock_t) lock, shared_arena);
}
int PyThread_acquire_lock(PyThread_type_lock lock, int waitflag) {
int success;
dprintf(("PyThread_acquire_lock(%p, %d) called\n", lock, waitflag));
errno = 0;
if (waitflag)
success = ussetlock((ulock_t) lock);
else
success = uscsetlock((ulock_t) lock, 1);
if (success < 0)
perror(waitflag ? "ussetlock" : "uscsetlock");
dprintf(("PyThread_acquire_lock(%p, %d) -> %d\n", lock, waitflag, success));
return success;
}
void PyThread_release_lock(PyThread_type_lock lock) {
dprintf(("PyThread_release_lock(%p) called\n", lock));
if (usunsetlock((ulock_t) lock) < 0)
perror("usunsetlock");
}
