#if !defined(TIMEFUNCS_H)
#define TIMEFUNCS_H
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_FUNC(time_t) _PyTime_DoubleToTimet(double x);
#if defined(__cplusplus)
}
#endif
#endif