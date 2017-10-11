/* Subroutine to get the last modification time of a file */
/* (A separate file because this may be OS dependent) */
#include "Python.h"
#include "pyconfig.h"
#if defined(__cplusplus)
extern "C" {
#endif
time_t
PyOS_GetLastModificationTime(char *path, FILE *fp) {
struct stat st;
if (fstat(fileno(fp), &st) != 0)
return -1;
else
return st.st_mtime;
}
#if defined(__cplusplus)
}
#endif
