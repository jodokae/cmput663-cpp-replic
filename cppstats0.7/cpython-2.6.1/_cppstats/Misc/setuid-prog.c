#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#if !defined(FULL_PATH)
#error "You must define FULL_PATH somewhere"
#endif
#if !defined(UMASK)
#define UMASK 077
#endif
#if defined(__STDC__) && defined(__sgi)
#define environ _environ
#endif
char def_IFS[] = "IFS= \t\n";
#if defined(__sgi)
char def_PATH[] = "PATH=/usr/bsd:/usr/bin:/bin:/usr/local/bin:/usr/sbin";
#else
char def_PATH[] = "PATH=/usr/ucb:/usr/bin:/bin:/usr/local/bin";
#endif
char def_CDPATH[] = "CDPATH=.";
char def_ENV[] = "ENV=:";
void
clean_environ(void) {
char **p;
extern char **environ;
for (p = environ; *p; p++) {
if (strncmp(*p, "LD_", 3) == 0)
**p = 'X';
else if (strncmp(*p, "_RLD", 4) == 0)
**p = 'X';
else if (strncmp(*p, "PYTHON", 6) == 0)
**p = 'X';
else if (strncmp(*p, "IFS=", 4) == 0)
*p = def_IFS;
else if (strncmp(*p, "CDPATH=", 7) == 0)
*p = def_CDPATH;
else if (strncmp(*p, "ENV=", 4) == 0)
*p = def_ENV;
}
putenv(def_PATH);
}
int
main(int argc, char **argv) {
struct stat statb;
gid_t egid = getegid();
uid_t euid = geteuid();
if (FULL_PATH[0] != '/') {
fprintf(stderr, "%s: %s is not a full path name\n", argv[0],
FULL_PATH);
fprintf(stderr, "You can only use this wrapper if you\n");
fprintf(stderr, "compile it with an absolute path.\n");
exit(1);
}
if (stat(FULL_PATH, &statb) < 0) {
perror("stat");
exit(1);
}
if (statb.st_uid != 0 && statb.st_uid != euid) {
fprintf(stderr, "%s: %s has the wrong owner\n", argv[0],
FULL_PATH);
fprintf(stderr, "The script should be owned by root,\n");
fprintf(stderr, "and shouldn't be writeable by anyone.\n");
exit(1);
}
if (setregid(egid, egid) < 0)
perror("setregid");
if (setreuid(euid, euid) < 0)
perror("setreuid");
clean_environ();
umask(UMASK);
while (**argv == '-')
(*argv)++;
execv(FULL_PATH, argv);
fprintf(stderr, "%s: could not execute the script\n", argv[0]);
exit(1);
}
