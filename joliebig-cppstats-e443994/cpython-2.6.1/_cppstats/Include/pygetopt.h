#if !defined(Py_PYGETOPT_H)
#define Py_PYGETOPT_H
#if defined(__cplusplus)
extern "C" {
#endif
PyAPI_DATA(int) _PyOS_opterr;
PyAPI_DATA(int) _PyOS_optind;
PyAPI_DATA(char *) _PyOS_optarg;
PyAPI_FUNC(int) _PyOS_GetOpt(int argc, char **argv, char *optstring);
#if defined(__cplusplus)
}
#endif
#endif /* !Py_PYGETOPT_H */
