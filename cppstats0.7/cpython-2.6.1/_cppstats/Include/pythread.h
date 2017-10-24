#if !defined(Py_PYTHREAD_H)
#define Py_PYTHREAD_H
#define NO_EXIT_PROG
typedef void *PyThread_type_lock;
typedef void *PyThread_type_sema;
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_FUNC(void) PyThread_init_thread(void);
PyAPI_FUNC(long) PyThread_start_new_thread(void (*)(void *), void *);
PyAPI_FUNC(void) PyThread_exit_thread(void);
PyAPI_FUNC(void) PyThread__PyThread_exit_thread(void);
PyAPI_FUNC(long) PyThread_get_thread_ident(void);
PyAPI_FUNC(PyThread_type_lock) PyThread_allocate_lock(void);
PyAPI_FUNC(void) PyThread_free_lock(PyThread_type_lock);
PyAPI_FUNC(int) PyThread_acquire_lock(PyThread_type_lock, int);
#define WAIT_LOCK 1
#define NOWAIT_LOCK 0
PyAPI_FUNC(void) PyThread_release_lock(PyThread_type_lock);
PyAPI_FUNC(size_t) PyThread_get_stacksize(void);
PyAPI_FUNC(int) PyThread_set_stacksize(size_t);
#if !defined(NO_EXIT_PROG)
PyAPI_FUNC(void) PyThread_exit_prog(int);
PyAPI_FUNC(void) PyThread__PyThread_exit_prog(int);
#endif
PyAPI_FUNC(int) PyThread_create_key(void);
PyAPI_FUNC(void) PyThread_delete_key(int);
PyAPI_FUNC(int) PyThread_set_key_value(int, void *);
PyAPI_FUNC(void *) PyThread_get_key_value(int);
PyAPI_FUNC(void) PyThread_delete_key_value(int key);
PyAPI_FUNC(void) PyThread_ReInitTLS(void);
#if defined(__cplusplus)
}
#endif
#endif