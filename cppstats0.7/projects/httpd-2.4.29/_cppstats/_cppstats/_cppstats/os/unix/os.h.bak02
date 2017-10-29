#if !defined(APACHE_OS_H)
#define APACHE_OS_H
#include "apr.h"
#include "ap_config.h"
#if !defined(PLATFORM)
#define PLATFORM "Unix"
#endif
#define AP_NEED_SET_MUTEX_PERMS 1
#define AP_PLATFORM_REWRITE_ARGS_HOOK ap_mpm_rewrite_args
#if defined(_OSD_POSIX)
pid_t os_fork(const char *user);
#endif
#endif
