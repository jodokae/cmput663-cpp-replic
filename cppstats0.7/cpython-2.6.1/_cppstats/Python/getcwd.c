#include <stdio.h>
#include <errno.h>
#if defined(HAVE_GETWD)
#if defined(HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif
#if !defined(MAXPATHLEN)
#if defined(PATH_MAX) && PATH_MAX > 1024
#define MAXPATHLEN PATH_MAX
#else
#define MAXPATHLEN 1024
#endif
#endif
extern char *getwd(char *);
char *
getcwd(char *buf, int size) {
char localbuf[MAXPATHLEN+1];
char *ret;
if (size <= 0) {
errno = EINVAL;
return NULL;
}
ret = getwd(localbuf);
if (ret != NULL && strlen(localbuf) >= (size_t)size) {
errno = ERANGE;
return NULL;
}
if (ret == NULL) {
errno = EACCES;
return NULL;
}
strncpy(buf, localbuf, size);
return buf;
}
#else
#if !defined(PWD_CMD)
#define PWD_CMD "/bin/pwd"
#endif
char *
getcwd(char *buf, int size) {
FILE *fp;
char *p;
int sts;
if (size <= 0) {
errno = EINVAL;
return NULL;
}
if ((fp = popen(PWD_CMD, "r")) == NULL)
return NULL;
if (fgets(buf, size, fp) == NULL || (sts = pclose(fp)) != 0) {
errno = EACCES;
return NULL;
}
for (p = buf; *p != '\n'; p++) {
if (*p == '\0') {
errno = ERANGE;
return NULL;
}
}
*p = '\0';
return buf;
}
#endif
