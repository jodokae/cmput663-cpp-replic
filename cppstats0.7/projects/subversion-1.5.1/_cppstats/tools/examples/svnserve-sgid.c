#include <stdio.h>
#include <unistd.h>
#define REAL_PATH "/usr/bin/svnserve.real"
char *newenv[] = { "PATH=/bin:/usr/bin", "SHELL=/bin/sh", NULL };
int
main(int argc, char **argv) {
if (setgid(getegid()) == -1) {
perror("setgid(getegid())");
return 1;
}
if (seteuid(getuid()) == -1) {
perror("seteuid(getuid())");
return 1;
}
execve(REAL_PATH, argv, newenv);
perror("attempting to exec " REAL_PATH " failed");
return 1;
}
