#include "apr.h"
#include "ap_config.h"
#include "suexec.h"
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#if APR_HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if defined(HAVE_PWD_H)
#include <pwd.h>
#endif
#if defined(HAVE_GRP_H)
#include <grp.h>
#endif
#if defined(PATH_MAX)
#define AP_MAXPATH PATH_MAX
#elif defined(MAXPATHLEN)
#define AP_MAXPATH MAXPATHLEN
#else
#define AP_MAXPATH 8192
#endif
#define AP_ENVBUF 256
extern char **environ;
static FILE *log = NULL;
static const char *const safe_env_lst[] = {
"HTTP_",
"SSL_",
"AUTH_TYPE=",
"CONTENT_LENGTH=",
"CONTENT_TYPE=",
"CONTEXT_DOCUMENT_ROOT=",
"CONTEXT_PREFIX=",
"DATE_GMT=",
"DATE_LOCAL=",
"DOCUMENT_ARGS=",
"DOCUMENT_NAME=",
"DOCUMENT_PATH_INFO=",
"DOCUMENT_ROOT=",
"DOCUMENT_URI=",
"GATEWAY_INTERFACE=",
"HTTPS=",
"LAST_MODIFIED=",
"PATH_INFO=",
"PATH_TRANSLATED=",
"QUERY_STRING=",
"QUERY_STRING_UNESCAPED=",
"REMOTE_ADDR=",
"REMOTE_HOST=",
"REMOTE_IDENT=",
"REMOTE_PORT=",
"REMOTE_USER=",
"REDIRECT_ERROR_NOTES=",
"REDIRECT_HANDLER=",
"REDIRECT_QUERY_STRING=",
"REDIRECT_REMOTE_USER=",
"REDIRECT_SCRIPT_FILENAME=",
"REDIRECT_STATUS=",
"REDIRECT_URL=",
"REQUEST_METHOD=",
"REQUEST_URI=",
"REQUEST_SCHEME=",
"SCRIPT_FILENAME=",
"SCRIPT_NAME=",
"SCRIPT_URI=",
"SCRIPT_URL=",
"SERVER_ADMIN=",
"SERVER_NAME=",
"SERVER_ADDR=",
"SERVER_PORT=",
"SERVER_PROTOCOL=",
"SERVER_SIGNATURE=",
"SERVER_SOFTWARE=",
"UNIQUE_ID=",
"USER_NAME=",
"TZ=",
NULL
};
static void log_err(const char *fmt,...)
__attribute__((format(printf,1,2)));
static void log_no_err(const char *fmt,...)
__attribute__((format(printf,1,2)));
static void err_output(int is_error, const char *fmt, va_list ap)
__attribute__((format(printf,2,0)));
static void err_output(int is_error, const char *fmt, va_list ap) {
#if defined(AP_LOG_EXEC)
time_t timevar;
struct tm *lt;
if (!log) {
#if defined(_LARGEFILE64_SOURCE) && HAVE_FOPEN64
if ((log = fopen64(AP_LOG_EXEC, "a")) == NULL) {
#else
if ((log = fopen(AP_LOG_EXEC, "a")) == NULL) {
#endif
fprintf(stderr, "suexec failure: could not open log file\n");
perror("fopen");
exit(1);
}
}
if (is_error) {
fprintf(stderr, "suexec policy violation: see suexec log for more "
"details\n");
}
time(&timevar);
lt = localtime(&timevar);
fprintf(log, "[%d-%.2d-%.2d %.2d:%.2d:%.2d]: ",
lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
lt->tm_hour, lt->tm_min, lt->tm_sec);
vfprintf(log, fmt, ap);
fflush(log);
#endif
return;
}
static void log_err(const char *fmt,...) {
#if defined(AP_LOG_EXEC)
va_list ap;
va_start(ap, fmt);
err_output(1, fmt, ap);
va_end(ap);
#endif
return;
}
static void log_no_err(const char *fmt,...) {
#if defined(AP_LOG_EXEC)
va_list ap;
va_start(ap, fmt);
err_output(0, fmt, ap);
va_end(ap);
#endif
return;
}
static void clean_env(void) {
char pathbuf[512];
char **cleanenv;
char **ep;
int cidx = 0;
int idx;
char **envp = environ;
char *empty_ptr = NULL;
environ = &empty_ptr;
if ((cleanenv = (char **) calloc(AP_ENVBUF, sizeof(char *))) == NULL) {
log_err("failed to malloc memory for environment\n");
exit(123);
}
sprintf(pathbuf, "PATH=%s", AP_SAFE_PATH);
cleanenv[cidx] = strdup(pathbuf);
if (cleanenv[cidx] == NULL) {
log_err("failed to malloc memory for environment\n");
exit(124);
}
cidx++;
for (ep = envp; *ep && cidx < AP_ENVBUF-1; ep++) {
for (idx = 0; safe_env_lst[idx]; idx++) {
if (!strncmp(*ep, safe_env_lst[idx],
strlen(safe_env_lst[idx]))) {
cleanenv[cidx] = *ep;
cidx++;
break;
}
}
}
cleanenv[cidx] = NULL;
environ = cleanenv;
}
int main(int argc, char *argv[]) {
int userdir = 0;
uid_t uid;
gid_t gid;
char *target_uname;
char *target_gname;
char *target_homedir;
char *actual_uname;
char *actual_gname;
char *cmd;
char cwd[AP_MAXPATH];
char dwd[AP_MAXPATH];
struct passwd *pw;
struct group *gr;
struct stat dir_info;
struct stat prg_info;
clean_env();
uid = getuid();
if ((pw = getpwuid(uid)) == NULL) {
log_err("crit: invalid uid: (%lu)\n", (unsigned long)uid);
exit(102);
}
if ((argc > 1)
&& (! strcmp(argv[1], "-V"))
&& ((uid == 0)
#if defined(_OSD_POSIX)
|| (! strcasecmp(AP_HTTPD_USER, pw->pw_name)))
#else
|| (! strcmp(AP_HTTPD_USER, pw->pw_name)))
#endif
) {
#if defined(AP_DOC_ROOT)
fprintf(stderr, " -D AP_DOC_ROOT=\"%s\"\n", AP_DOC_ROOT);
#endif
#if defined(AP_GID_MIN)
fprintf(stderr, " -D AP_GID_MIN=%d\n", AP_GID_MIN);
#endif
#if defined(AP_HTTPD_USER)
fprintf(stderr, " -D AP_HTTPD_USER=\"%s\"\n", AP_HTTPD_USER);
#endif
#if defined(AP_LOG_EXEC)
fprintf(stderr, " -D AP_LOG_EXEC=\"%s\"\n", AP_LOG_EXEC);
#endif
#if defined(AP_SAFE_PATH)
fprintf(stderr, " -D AP_SAFE_PATH=\"%s\"\n", AP_SAFE_PATH);
#endif
#if defined(AP_SUEXEC_UMASK)
fprintf(stderr, " -D AP_SUEXEC_UMASK=%03o\n", AP_SUEXEC_UMASK);
#endif
#if defined(AP_UID_MIN)
fprintf(stderr, " -D AP_UID_MIN=%d\n", AP_UID_MIN);
#endif
#if defined(AP_USERDIR_SUFFIX)
fprintf(stderr, " -D AP_USERDIR_SUFFIX=\"%s\"\n", AP_USERDIR_SUFFIX);
#endif
exit(0);
}
if (argc < 4) {
log_err("too few arguments\n");
exit(101);
}
target_uname = argv[1];
target_gname = argv[2];
cmd = argv[3];
#if defined(_OSD_POSIX)
if (strcasecmp(AP_HTTPD_USER, pw->pw_name)) {
log_err("user mismatch (%s instead of %s)\n", pw->pw_name, AP_HTTPD_USER);
exit(103);
}
#else
if (strcmp(AP_HTTPD_USER, pw->pw_name)) {
log_err("user mismatch (%s instead of %s)\n", pw->pw_name, AP_HTTPD_USER);
exit(103);
}
#endif
if ((cmd[0] == '/') || (!strncmp(cmd, "../", 3))
|| (strstr(cmd, "/../") != NULL)) {
log_err("invalid command (%s)\n", cmd);
exit(104);
}
if (!strncmp("~", target_uname, 1)) {
target_uname++;
userdir = 1;
}
if (strspn(target_uname, "1234567890") != strlen(target_uname)) {
if ((pw = getpwnam(target_uname)) == NULL) {
log_err("invalid target user name: (%s)\n", target_uname);
exit(105);
}
} else {
if ((pw = getpwuid(atoi(target_uname))) == NULL) {
log_err("invalid target user id: (%s)\n", target_uname);
exit(121);
}
}
if (strspn(target_gname, "1234567890") != strlen(target_gname)) {
if ((gr = getgrnam(target_gname)) == NULL) {
log_err("invalid target group name: (%s)\n", target_gname);
exit(106);
}
} else {
if ((gr = getgrgid(atoi(target_gname))) == NULL) {
log_err("invalid target group id: (%s)\n", target_gname);
exit(106);
}
}
gid = gr->gr_gid;
if ((actual_gname = strdup(gr->gr_name)) == NULL) {
log_err("failed to alloc memory\n");
exit(125);
}
#if defined(_OSD_POSIX)
{
pid_t pid;
int status;
switch (pid = ufork(target_uname)) {
case -1:
log_err("failed to setup bs2000 environment for user %s: %s\n",
target_uname, strerror(errno));
exit(150);
case 0:
break;
default:
while (pid != waitpid(pid, &status, 0))
;
if (WIFSIGNALED(status)) {
kill (getpid(), WTERMSIG(status));
}
exit(WEXITSTATUS(status));
}
}
#endif
uid = pw->pw_uid;
actual_uname = strdup(pw->pw_name);
target_homedir = strdup(pw->pw_dir);
if (actual_uname == NULL || target_homedir == NULL) {
log_err("failed to alloc memory\n");
exit(126);
}
log_no_err("uid: (%s/%s) gid: (%s/%s) cmd: %s\n",
target_uname, actual_uname,
target_gname, actual_gname,
cmd);
if ((uid == 0) || (uid < AP_UID_MIN)) {
log_err("cannot run as forbidden uid (%lu/%s)\n", (unsigned long)uid, cmd);
exit(107);
}
if ((gid == 0) || (gid < AP_GID_MIN)) {
log_err("cannot run as forbidden gid (%lu/%s)\n", (unsigned long)gid, cmd);
exit(108);
}
if (((setgid(gid)) != 0) || (initgroups(actual_uname, gid) != 0)) {
log_err("failed to setgid (%lu: %s)\n", (unsigned long)gid, cmd);
exit(109);
}
if ((setuid(uid)) != 0) {
log_err("failed to setuid (%lu: %s)\n", (unsigned long)uid, cmd);
exit(110);
}
if (getcwd(cwd, AP_MAXPATH) == NULL) {
log_err("cannot get current working directory\n");
exit(111);
}
if (userdir) {
if (((chdir(target_homedir)) != 0) ||
((chdir(AP_USERDIR_SUFFIX)) != 0) ||
((getcwd(dwd, AP_MAXPATH)) == NULL) ||
((chdir(cwd)) != 0)) {
log_err("cannot get docroot information (%s)\n", target_homedir);
exit(112);
}
} else {
if (((chdir(AP_DOC_ROOT)) != 0) ||
((getcwd(dwd, AP_MAXPATH)) == NULL) ||
((chdir(cwd)) != 0)) {
log_err("cannot get docroot information (%s)\n", AP_DOC_ROOT);
exit(113);
}
}
if ((strncmp(cwd, dwd, strlen(dwd))) != 0) {
log_err("command not in docroot (%s/%s)\n", cwd, cmd);
exit(114);
}
if (((lstat(cwd, &dir_info)) != 0) || !(S_ISDIR(dir_info.st_mode))) {
log_err("cannot stat directory: (%s)\n", cwd);
exit(115);
}
if ((dir_info.st_mode & S_IWOTH) || (dir_info.st_mode & S_IWGRP)) {
log_err("directory is writable by others: (%s)\n", cwd);
exit(116);
}
if (((lstat(cmd, &prg_info)) != 0) || (S_ISLNK(prg_info.st_mode))) {
log_err("cannot stat program: (%s)\n", cmd);
exit(117);
}
if ((prg_info.st_mode & S_IWOTH) || (prg_info.st_mode & S_IWGRP)) {
log_err("file is writable by others: (%s/%s)\n", cwd, cmd);
exit(118);
}
if ((prg_info.st_mode & S_ISUID) || (prg_info.st_mode & S_ISGID)) {
log_err("file is either setuid or setgid: (%s/%s)\n", cwd, cmd);
exit(119);
}
if ((uid != dir_info.st_uid) ||
(gid != dir_info.st_gid) ||
(uid != prg_info.st_uid) ||
(gid != prg_info.st_gid)) {
log_err("target uid/gid (%lu/%lu) mismatch "
"with directory (%lu/%lu) or program (%lu/%lu)\n",
(unsigned long)uid, (unsigned long)gid,
(unsigned long)dir_info.st_uid, (unsigned long)dir_info.st_gid,
(unsigned long)prg_info.st_uid, (unsigned long)prg_info.st_gid);
exit(120);
}
if (!(prg_info.st_mode & S_IXUSR)) {
log_err("file has no execute permission: (%s/%s)\n", cwd, cmd);
exit(121);
}
#if defined(AP_SUEXEC_UMASK)
if ((~AP_SUEXEC_UMASK) & 0022) {
log_err("notice: AP_SUEXEC_UMASK of %03o allows "
"write permission to group and/or other\n", AP_SUEXEC_UMASK);
}
umask(AP_SUEXEC_UMASK);
#endif
if (log != NULL) {
#if APR_HAVE_FCNTL_H
fflush(log);
setbuf(log, NULL);
if ((fcntl(fileno(log), F_SETFD, FD_CLOEXEC) == -1)) {
log_err("error: can't set close-on-exec flag");
exit(122);
}
#else
fclose(log);
log = NULL;
#endif
}
#if defined(NEED_HASHBANG_EMUL)
{
extern char **environ;
ap_execve(cmd, &argv[3], environ);
}
#else
execv(cmd, &argv[3]);
#endif
log_err("(%d)%s: exec failed (%s)\n", errno, strerror(errno), cmd);
exit(255);
}