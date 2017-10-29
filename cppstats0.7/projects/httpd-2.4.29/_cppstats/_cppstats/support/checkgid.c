#include "ap_config.h"
#if APR_HAVE_STDIO_H
#include <stdio.h>
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_GRP_H
#include <grp.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
int main(int argc, char *argv[]) {
int i;
int result;
gid_t gid;
struct group *grent;
struct group fake_grent;
result = 0;
for (i = 1; i < argc; ++i) {
char *arg;
arg = argv[i];
if (*arg == '#') {
gid = atoi(&arg[1]);
fake_grent.gr_gid = gid;
grent = &fake_grent;
} else {
grent = getgrnam(arg);
}
if (grent == NULL) {
fprintf(stderr, "%s: group '%s' not found\n", argv[0], arg);
result = -1;
} else {
int check;
gid = grent->gr_gid;
check = setgid(gid);
if (check != 0) {
fprintf(stderr, "%s: invalid group '%s'\n", argv[0], arg);
perror(argv[0]);
result = -1;
}
}
}
return result;
}