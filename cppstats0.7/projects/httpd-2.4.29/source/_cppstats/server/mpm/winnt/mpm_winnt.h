#if !defined(APACHE_MPM_WINNT_H)
#define APACHE_MPM_WINNT_H
#include "apr_proc_mutex.h"
#include "ap_listen.h"
#define SERVICE_APACHE_RESTART 128
#if !defined(AP_DEFAULT_SERVICE_NAME)
#define AP_DEFAULT_SERVICE_NAME "Apache2.4"
#endif
#define SERVICECONFIG "System\\CurrentControlSet\\Services\\%s"
#define SERVICEPARAMS "System\\CurrentControlSet\\Services\\%s\\Parameters"
apr_status_t mpm_service_set_name(apr_pool_t *p, const char **display_name,
const char *set_name);
apr_status_t mpm_merge_service_args(apr_pool_t *p, apr_array_header_t *args,
int fixed_args);
apr_status_t mpm_service_to_start(const char **display_name, apr_pool_t *p);
apr_status_t mpm_service_started(void);
apr_status_t mpm_service_install(apr_pool_t *ptemp, int argc,
char const* const* argv, int reconfig);
apr_status_t mpm_service_uninstall(void);
apr_status_t mpm_service_start(apr_pool_t *ptemp, int argc,
char const* const* argv);
void mpm_signal_service(apr_pool_t *ptemp, int signal);
void mpm_service_stopping(void);
void mpm_start_console_handler(void);
void mpm_start_child_console_handler(void);
void mpm_nt_eventlog_stderr_open(const char *display_name, apr_pool_t *p);
void mpm_nt_eventlog_stderr_flush(void);
extern module AP_MODULE_DECLARE_DATA mpm_winnt_module;
extern int ap_threads_per_child;
extern DWORD my_pid;
extern apr_proc_mutex_t *start_mutex;
extern HANDLE exit_event;
extern int winnt_mpm_state;
extern OSVERSIONINFO osver;
extern DWORD stack_res_flag;
extern void clean_child_exit(int);
typedef enum {
SIGNAL_PARENT_SHUTDOWN,
SIGNAL_PARENT_RESTART,
SIGNAL_PARENT_RESTART_GRACEFUL
} ap_signal_parent_e;
AP_DECLARE(void) ap_signal_parent(ap_signal_parent_e type);
void hold_console_open_on_error(void);
void child_main(apr_pool_t *pconf, DWORD parent_pid);
#endif
