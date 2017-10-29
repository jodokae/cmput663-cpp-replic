#if !defined(Py_INTRCHECK_H)
#define Py_INTRCHECK_H
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_FUNC(int) PyOS_InterruptOccurred(void);
PyAPI_FUNC(void) PyOS_InitInterrupts(void);
PyAPI_FUNC(void) PyOS_AfterFork(void);
#if defined(__cplusplus)
}
#endif
#endif