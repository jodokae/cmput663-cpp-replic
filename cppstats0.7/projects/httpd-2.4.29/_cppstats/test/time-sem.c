#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <signal.h>
#if defined(USE_FCNTL_SERIALIZED_ACCEPT)
static struct flock lock_it;
static struct flock unlock_it;
static int fcntl_fd=-1;
#define accept_mutex_child_init()
#define accept_mutex_cleanup()
void
accept_mutex_init(void) {
lock_it.l_whence = SEEK_SET;
lock_it.l_start = 0;
lock_it.l_len = 0;
lock_it.l_type = F_WRLCK;
lock_it.l_pid = 0;
unlock_it.l_whence = SEEK_SET;
unlock_it.l_start = 0;
unlock_it.l_len = 0;
unlock_it.l_type = F_UNLCK;
unlock_it.l_pid = 0;
printf("opening test-lock-thing in current directory\n");
fcntl_fd = open("test-lock-thing", O_CREAT | O_WRONLY | O_EXCL, 0644);
if (fcntl_fd == -1) {
perror ("open");
fprintf (stderr, "Cannot open lock file: %s\n", "test-lock-thing");
exit (1);
}
unlink("test-lock-thing");
}
void accept_mutex_on(void) {
int ret;
while ((ret = fcntl(fcntl_fd, F_SETLKW, &lock_it)) < 0 && errno == EINTR)
continue;
if (ret < 0) {
perror ("fcntl lock_it");
exit(1);
}
}
void accept_mutex_off(void) {
if (fcntl (fcntl_fd, F_SETLKW, &unlock_it) < 0) {
perror ("fcntl unlock_it");
exit(1);
}
}
#elif defined(USE_FLOCK_SERIALIZED_ACCEPT)
#include <sys/file.h>
static int flock_fd=-1;
#define FNAME "test-lock-thing"
void accept_mutex_init(void) {
printf("opening " FNAME " in current directory\n");
flock_fd = open(FNAME, O_CREAT | O_WRONLY | O_EXCL, 0644);
if (flock_fd == -1) {
perror ("open");
fprintf (stderr, "Cannot open lock file: %s\n", "test-lock-thing");
exit (1);
}
}
void accept_mutex_child_init(void) {
flock_fd = open(FNAME, O_WRONLY, 0600);
if (flock_fd == -1) {
perror("open");
exit(1);
}
}
void accept_mutex_cleanup(void) {
unlink(FNAME);
}
void accept_mutex_on(void) {
int ret;
while ((ret = flock(flock_fd, LOCK_EX)) < 0 && errno == EINTR)
continue;
if (ret < 0) {
perror ("flock(LOCK_EX)");
exit(1);
}
}
void accept_mutex_off(void) {
if (flock (flock_fd, LOCK_UN) < 0) {
perror ("flock(LOCK_UN)");
exit(1);
}
}
#elif defined (USE_SYSVSEM_SERIALIZED_ACCEPT)
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
static int sem_id = -1;
#if defined(NO_SEM_UNDO)
static sigset_t accept_block_mask;
static sigset_t accept_previous_mask;
#endif
#define accept_mutex_child_init()
#define accept_mutex_cleanup()
void accept_mutex_init(void) {
#if defined(NEED_UNION_SEMUN)
union semun {
int val;
struct semid_ds *buf;
ushort *array;
};
#endif
union semun ick;
sem_id = semget(999, 1, IPC_CREAT | 0666);
if (sem_id < 0) {
perror ("semget");
exit (1);
}
ick.val = 1;
if (semctl(sem_id, 0, SETVAL, ick) < 0) {
perror ("semctl");
exit(1);
}
#if defined(NO_SEM_UNDO)
sigfillset(&accept_block_mask);
sigdelset(&accept_block_mask, SIGHUP);
sigdelset(&accept_block_mask, SIGTERM);
sigdelset(&accept_block_mask, SIGUSR1);
#endif
}
void accept_mutex_on() {
struct sembuf op;
#if defined(NO_SEM_UNDO)
if (sigprocmask(SIG_BLOCK, &accept_block_mask, &accept_previous_mask)) {
perror("sigprocmask(SIG_BLOCK)");
exit (1);
}
op.sem_flg = 0;
#else
op.sem_flg = SEM_UNDO;
#endif
op.sem_num = 0;
op.sem_op = -1;
if (semop(sem_id, &op, 1) < 0) {
perror ("accept_mutex_on");
exit (1);
}
}
void accept_mutex_off() {
struct sembuf op;
op.sem_num = 0;
op.sem_op = 1;
#if defined(NO_SEM_UNDO)
op.sem_flg = 0;
#else
op.sem_flg = SEM_UNDO;
#endif
if (semop(sem_id, &op, 1) < 0) {
perror ("accept_mutex_off");
exit (1);
}
#if defined(NO_SEM_UNDO)
if (sigprocmask(SIG_SETMASK, &accept_previous_mask, NULL)) {
perror("sigprocmask(SIG_SETMASK)");
exit (1);
}
#endif
}
#elif defined (USE_PTHREAD_SERIALIZED_ACCEPT)
#include <pthread.h>
static pthread_mutex_t *mutex;
static sigset_t accept_block_mask;
static sigset_t accept_previous_mask;
#define accept_mutex_child_init()
#define accept_mutex_cleanup()
void accept_mutex_init(void) {
pthread_mutexattr_t mattr;
int fd;
fd = open ("/dev/zero", O_RDWR);
if (fd == -1) {
perror ("open(/dev/zero)");
exit (1);
}
mutex = (pthread_mutex_t *)mmap ((caddr_t)0, sizeof (*mutex),
PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
if (mutex == (void *)(caddr_t)-1) {
perror ("mmap");
exit (1);
}
close (fd);
if (pthread_mutexattr_init(&mattr)) {
perror ("pthread_mutexattr_init");
exit (1);
}
if (pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED)) {
perror ("pthread_mutexattr_setpshared");
exit (1);
}
if (pthread_mutex_init(mutex, &mattr)) {
perror ("pthread_mutex_init");
exit (1);
}
sigfillset(&accept_block_mask);
sigdelset(&accept_block_mask, SIGHUP);
sigdelset(&accept_block_mask, SIGTERM);
sigdelset(&accept_block_mask, SIGUSR1);
}
void accept_mutex_on() {
if (sigprocmask(SIG_BLOCK, &accept_block_mask, &accept_previous_mask)) {
perror("sigprocmask(SIG_BLOCK)");
exit (1);
}
if (pthread_mutex_lock (mutex)) {
perror ("pthread_mutex_lock");
exit (1);
}
}
void accept_mutex_off() {
if (pthread_mutex_unlock (mutex)) {
perror ("pthread_mutex_unlock");
exit (1);
}
if (sigprocmask(SIG_SETMASK, &accept_previous_mask, NULL)) {
perror("sigprocmask(SIG_SETMASK)");
exit (1);
}
}
#elif defined (USE_USLOCK_SERIALIZED_ACCEPT)
#include <ulocks.h>
static usptr_t *us = NULL;
static ulock_t uslock = NULL;
#define accept_mutex_child_init()
#define accept_mutex_cleanup()
void accept_mutex_init(void) {
ptrdiff_t old;
#define CONF_INITUSERS_MAX 15
if ((old = usconfig(CONF_INITUSERS, CONF_INITUSERS_MAX)) == -1) {
perror("usconfig");
exit(-1);
}
if ((old = usconfig(CONF_LOCKTYPE, US_NODEBUG)) == -1) {
perror("usconfig");
exit(-1);
}
if ((old = usconfig(CONF_ARENATYPE, US_SHAREDONLY)) == -1) {
perror("usconfig");
exit(-1);
}
if ((us = usinit("/dev/zero")) == NULL) {
perror("usinit");
exit(-1);
}
if ((uslock = usnewlock(us)) == NULL) {
perror("usnewlock");
exit(-1);
}
}
void accept_mutex_on() {
switch(ussetlock(uslock)) {
case 1:
break;
case 0:
fprintf(stderr, "didn't get lock\n");
exit(-1);
case -1:
perror("ussetlock");
exit(-1);
}
}
void accept_mutex_off() {
if (usunsetlock(uslock) == -1) {
perror("usunsetlock");
exit(-1);
}
}
#endif
#if !defined(USE_SHMGET_SCOREBOARD)
static void *get_shared_mem(apr_size_t size) {
void *result;
result = (unsigned long *)mmap ((caddr_t)0, size,
PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
if (result == (void *)(caddr_t)-1) {
perror ("mmap");
exit (1);
}
return result;
}
#else
#include <sys/types.h>
#include <sys/ipc.h>
#if defined(HAVE_SYS_MUTEX_H)
#include <sys/mutex.h>
#endif
#include <sys/shm.h>
static void *get_shared_mem(apr_size_t size) {
key_t shmkey = IPC_PRIVATE;
int shmid = -1;
void *result;
#if defined(MOVEBREAK)
char *obrk;
#endif
if ((shmid = shmget(shmkey, size, IPC_CREAT | SHM_R | SHM_W)) == -1) {
perror("shmget");
exit(1);
}
#if defined(MOVEBREAK)
if ((obrk = sbrk(MOVEBREAK)) == (char *) -1) {
perror("sbrk");
}
#endif
#define BADSHMAT ((void *)(-1))
if ((result = shmat(shmid, 0, 0)) == BADSHMAT) {
perror("shmat");
}
if (shmctl(shmid, IPC_RMID, NULL) != 0) {
perror("shmctl(IPC_RMID)");
}
if (result == BADSHMAT)
exit(1);
#if defined(MOVEBREAK)
if (obrk == (char *) -1)
return;
if (sbrk(-(MOVEBREAK)) == (char *) -1) {
perror("sbrk 2");
}
#endif
return result;
}
#endif
#if defined(_POSIX_PRIORITY_SCHEDULING)
#define _P __P
#include <sched.h>
#define YIELD sched_yield()
#else
#define YIELD do { struct timeval zero; zero.tv_sec = zero.tv_usec = 0; select(0,0,0,0,&zero); } while(0)
#endif
void main (int argc, char **argv) {
int num_iter;
int num_child;
int i;
struct timeval first;
struct timeval last;
long ms;
int pid;
unsigned long *shared_counter;
if (argc != 3) {
fprintf (stderr, "Usage: time-sem num-child num iter\n");
exit (1);
}
num_child = atoi (argv[1]);
num_iter = atoi (argv[2]);
shared_counter = get_shared_mem(sizeof(*shared_counter));
*shared_counter = 0;
accept_mutex_init ();
accept_mutex_on ();
for (i = 0; i < num_child; ++i) {
pid = fork();
if (pid == 0) {
accept_mutex_child_init();
for (i = 0; i < num_iter; ++i) {
unsigned long tmp;
accept_mutex_on ();
tmp = *shared_counter;
YIELD;
*shared_counter = tmp + 1;
accept_mutex_off ();
}
exit (0);
} else if (pid == -1) {
perror ("fork");
exit (1);
}
}
if (*shared_counter != 0) {
puts ("WTF! shared_counter != 0 before the children have been started!");
exit (1);
}
gettimeofday (&first, NULL);
accept_mutex_off ();
for (i = 0; i < num_child; ++i) {
if (wait(NULL) == -1) {
perror ("wait");
}
}
gettimeofday (&last, NULL);
if (*shared_counter != num_child * num_iter) {
printf ("WTF! shared_counter != num_child * num_iter!\n"
"shared_counter = %lu\nnum_child = %d\nnum_iter=%d\n",
*shared_counter,
num_child, num_iter);
}
last.tv_sec -= first.tv_sec;
ms = last.tv_usec - first.tv_usec;
if (ms < 0) {
--last.tv_sec;
ms += 1000000;
}
last.tv_usec = ms;
printf ("%8lu.%06lu\n", last.tv_sec, last.tv_usec);
accept_mutex_cleanup();
exit(0);
}
