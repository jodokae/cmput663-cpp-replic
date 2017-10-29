#if !defined(APACHE_MPM_DEFAULT_H)
#define APACHE_MPM_DEFAULT_H
#if !defined(HARD_THREAD_LIMIT)
#define HARD_THREAD_LIMIT 2048
#endif
#if !defined(DEFAULT_THREADS_PER_CHILD)
#define DEFAULT_THREADS_PER_CHILD 50
#endif
#if !defined(DEFAULT_START_THREADS)
#define DEFAULT_START_THREADS DEFAULT_THREADS_PER_CHILD
#endif
#if !defined(DEFAULT_MAX_FREE_THREADS)
#define DEFAULT_MAX_FREE_THREADS 100
#endif
#if !defined(DEFAULT_MIN_FREE_THREADS)
#define DEFAULT_MIN_FREE_THREADS 10
#endif
#if !defined(SCOREBOARD_MAINTENANCE_INTERVAL)
#define SCOREBOARD_MAINTENANCE_INTERVAL 1000000
#endif
#if !defined(DEFAULT_THREAD_STACKSIZE)
#define DEFAULT_THREAD_STACKSIZE 65536
#endif
#endif
