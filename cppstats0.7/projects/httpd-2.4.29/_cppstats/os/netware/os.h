#if !defined(APACHE_OS_H)
#define APACHE_OS_H
#if !defined(PLATFORM)
#define PLATFORM "NETWARE"
#endif
#define AP_PLATFORM_REWRITE_ARGS_HOOK NULL
#include <screen.h>
AP_DECLARE_DATA extern int hold_screen_on_exit;
#define CASE_BLIND_FILESYSTEM
#define NO_WRITEV
#define getpid NXThreadGetId
#define exit(s) {if((s||hold_screen_on_exit)&&(hold_screen_on_exit>=0)){pressanykey();}apr_terminate();exit(s);}
#endif