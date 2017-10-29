#if !defined(APACHE_HTTP_LOG_H)
#define APACHE_HTTP_LOG_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "apr_thread_proc.h"
#include "http_config.h"
#if defined(HAVE_SYSLOG)
#include <syslog.h>
#if !defined(LOG_PRIMASK)
#define LOG_PRIMASK 7
#endif
#define APLOG_EMERG LOG_EMERG
#define APLOG_ALERT LOG_ALERT
#define APLOG_CRIT LOG_CRIT
#define APLOG_ERR LOG_ERR
#define APLOG_WARNING LOG_WARNING
#define APLOG_NOTICE LOG_NOTICE
#define APLOG_INFO LOG_INFO
#define APLOG_DEBUG LOG_DEBUG
#define APLOG_TRACE1 (LOG_DEBUG + 1)
#define APLOG_TRACE2 (LOG_DEBUG + 2)
#define APLOG_TRACE3 (LOG_DEBUG + 3)
#define APLOG_TRACE4 (LOG_DEBUG + 4)
#define APLOG_TRACE5 (LOG_DEBUG + 5)
#define APLOG_TRACE6 (LOG_DEBUG + 6)
#define APLOG_TRACE7 (LOG_DEBUG + 7)
#define APLOG_TRACE8 (LOG_DEBUG + 8)
#define APLOG_LEVELMASK 15
#else
#define APLOG_EMERG 0
#define APLOG_ALERT 1
#define APLOG_CRIT 2
#define APLOG_ERR 3
#define APLOG_WARNING 4
#define APLOG_NOTICE 5
#define APLOG_INFO 6
#define APLOG_DEBUG 7
#define APLOG_TRACE1 8
#define APLOG_TRACE2 9
#define APLOG_TRACE3 10
#define APLOG_TRACE4 11
#define APLOG_TRACE5 12
#define APLOG_TRACE6 13
#define APLOG_TRACE7 14
#define APLOG_TRACE8 15
#define APLOG_LEVELMASK 15
#endif
#define APLOG_NOERRNO (APLOG_LEVELMASK + 1)
#define APLOG_TOCLIENT ((APLOG_LEVELMASK + 1) * 2)
#define APLOG_STARTUP ((APLOG_LEVELMASK + 1) * 4)
#if !defined(DEFAULT_LOGLEVEL)
#define DEFAULT_LOGLEVEL APLOG_WARNING
#endif
#define APLOGNO(n) "AH" #n ": "
#define APLOG_NO_MODULE -1
#if defined(__cplusplus)
#else
static int * const aplog_module_index;
#endif
#if defined(__cplusplus)
#define APLOG_MODULE_INDEX (*aplog_module_index)
#else
#define APLOG_MODULE_INDEX (aplog_module_index ? *aplog_module_index : APLOG_NO_MODULE)
#endif
#if defined(DOXYGEN)
#define APLOG_MAX_LOGLEVEL
#endif
#if !defined(APLOG_MAX_LOGLEVEL)
#define APLOG_MODULE_IS_LEVEL(s,module_index,level) ( (((level)&APLOG_LEVELMASK) <= APLOG_NOTICE) || (s == NULL) || (ap_get_server_module_loglevel(s, module_index) >= ((level)&APLOG_LEVELMASK) ) )
#define APLOG_C_MODULE_IS_LEVEL(c,module_index,level) ( (((level)&APLOG_LEVELMASK) <= APLOG_NOTICE) || (ap_get_conn_module_loglevel(c, module_index) >= ((level)&APLOG_LEVELMASK) ) )
#define APLOG_CS_MODULE_IS_LEVEL(c,s,module_index,level) ( (((level)&APLOG_LEVELMASK) <= APLOG_NOTICE) || (ap_get_conn_server_module_loglevel(c, s, module_index) >= ((level)&APLOG_LEVELMASK) ) )
#define APLOG_R_MODULE_IS_LEVEL(r,module_index,level) ( (((level)&APLOG_LEVELMASK) <= APLOG_NOTICE) || (ap_get_request_module_loglevel(r, module_index) >= ((level)&APLOG_LEVELMASK) ) )
#else
#define APLOG_MODULE_IS_LEVEL(s,module_index,level) ( (((level)&APLOG_LEVELMASK) <= APLOG_MAX_LOGLEVEL) && ( (((level)&APLOG_LEVELMASK) <= APLOG_NOTICE) || (s == NULL) || (ap_get_server_module_loglevel(s, module_index) >= ((level)&APLOG_LEVELMASK) ) ) )
#define APLOG_CS_MODULE_IS_LEVEL(c,s,module_index,level) ( (((level)&APLOG_LEVELMASK) <= APLOG_MAX_LOGLEVEL) && ( (((level)&APLOG_LEVELMASK) <= APLOG_NOTICE) || (ap_get_conn_server_module_loglevel(c, s, module_index) >= ((level)&APLOG_LEVELMASK) ) ) )
#define APLOG_C_MODULE_IS_LEVEL(c,module_index,level) ( (((level)&APLOG_LEVELMASK) <= APLOG_MAX_LOGLEVEL) && ( (((level)&APLOG_LEVELMASK) <= APLOG_NOTICE) || (ap_get_conn_module_loglevel(c, module_index) >= ((level)&APLOG_LEVELMASK) ) ) )
#define APLOG_R_MODULE_IS_LEVEL(r,module_index,level) ( (((level)&APLOG_LEVELMASK) <= APLOG_MAX_LOGLEVEL) && ( (((level)&APLOG_LEVELMASK) <= APLOG_NOTICE) || (ap_get_request_module_loglevel(r, module_index) >= ((level)&APLOG_LEVELMASK) ) ) )
#endif
#define APLOG_IS_LEVEL(s,level) APLOG_MODULE_IS_LEVEL(s,APLOG_MODULE_INDEX,level)
#define APLOG_C_IS_LEVEL(c,level) APLOG_C_MODULE_IS_LEVEL(c,APLOG_MODULE_INDEX,level)
#define APLOG_CS_IS_LEVEL(c,s,level) APLOG_CS_MODULE_IS_LEVEL(c,s,APLOG_MODULE_INDEX,level)
#define APLOG_R_IS_LEVEL(r,level) APLOG_R_MODULE_IS_LEVEL(r,APLOG_MODULE_INDEX,level)
#define APLOGinfo(s) APLOG_IS_LEVEL(s,APLOG_INFO)
#define APLOGdebug(s) APLOG_IS_LEVEL(s,APLOG_DEBUG)
#define APLOGtrace1(s) APLOG_IS_LEVEL(s,APLOG_TRACE1)
#define APLOGtrace2(s) APLOG_IS_LEVEL(s,APLOG_TRACE2)
#define APLOGtrace3(s) APLOG_IS_LEVEL(s,APLOG_TRACE3)
#define APLOGtrace4(s) APLOG_IS_LEVEL(s,APLOG_TRACE4)
#define APLOGtrace5(s) APLOG_IS_LEVEL(s,APLOG_TRACE5)
#define APLOGtrace6(s) APLOG_IS_LEVEL(s,APLOG_TRACE6)
#define APLOGtrace7(s) APLOG_IS_LEVEL(s,APLOG_TRACE7)
#define APLOGtrace8(s) APLOG_IS_LEVEL(s,APLOG_TRACE8)
#define APLOGrinfo(r) APLOG_R_IS_LEVEL(r,APLOG_INFO)
#define APLOGrdebug(r) APLOG_R_IS_LEVEL(r,APLOG_DEBUG)
#define APLOGrtrace1(r) APLOG_R_IS_LEVEL(r,APLOG_TRACE1)
#define APLOGrtrace2(r) APLOG_R_IS_LEVEL(r,APLOG_TRACE2)
#define APLOGrtrace3(r) APLOG_R_IS_LEVEL(r,APLOG_TRACE3)
#define APLOGrtrace4(r) APLOG_R_IS_LEVEL(r,APLOG_TRACE4)
#define APLOGrtrace5(r) APLOG_R_IS_LEVEL(r,APLOG_TRACE5)
#define APLOGrtrace6(r) APLOG_R_IS_LEVEL(r,APLOG_TRACE6)
#define APLOGrtrace7(r) APLOG_R_IS_LEVEL(r,APLOG_TRACE7)
#define APLOGrtrace8(r) APLOG_R_IS_LEVEL(r,APLOG_TRACE8)
#define APLOGcinfo(c) APLOG_C_IS_LEVEL(c,APLOG_INFO)
#define APLOGcdebug(c) APLOG_C_IS_LEVEL(c,APLOG_DEBUG)
#define APLOGctrace1(c) APLOG_C_IS_LEVEL(c,APLOG_TRACE1)
#define APLOGctrace2(c) APLOG_C_IS_LEVEL(c,APLOG_TRACE2)
#define APLOGctrace3(c) APLOG_C_IS_LEVEL(c,APLOG_TRACE3)
#define APLOGctrace4(c) APLOG_C_IS_LEVEL(c,APLOG_TRACE4)
#define APLOGctrace5(c) APLOG_C_IS_LEVEL(c,APLOG_TRACE5)
#define APLOGctrace6(c) APLOG_C_IS_LEVEL(c,APLOG_TRACE6)
#define APLOGctrace7(c) APLOG_C_IS_LEVEL(c,APLOG_TRACE7)
#define APLOGctrace8(c) APLOG_C_IS_LEVEL(c,APLOG_TRACE8)
AP_DECLARE_DATA extern int ap_default_loglevel;
#define APLOG_MARK __FILE__,__LINE__,APLOG_MODULE_INDEX
AP_DECLARE(void) ap_open_stderr_log(apr_pool_t *p);
AP_DECLARE(apr_status_t) ap_replace_stderr_log(apr_pool_t *p,
const char *file);
int ap_open_logs(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s_main);
void ap_logs_child_init(apr_pool_t *p, server_rec *s);
#if defined(DOXYGEN)
AP_DECLARE(void) ap_log_error(const char *file, int line, int module_index,
int level, apr_status_t status,
const server_rec *s, const char *fmt, ...);
#else
#if defined(AP_HAVE_C99)
#define ap_log_error(...) ap_log_error__(__VA_ARGS__)
#define ap_log_error__(file, line, mi, level, status, s, ...) do { const server_rec *sr__ = s; if (APLOG_MODULE_IS_LEVEL(sr__, mi, level)) ap_log_error_(file, line, mi, level, status, sr__, __VA_ARGS__); } while(0)
#else
#define ap_log_error ap_log_error_
#endif
AP_DECLARE(void) ap_log_error_(const char *file, int line, int module_index,
int level, apr_status_t status,
const server_rec *s, const char *fmt, ...)
__attribute__((format(printf,7,8)));
#endif
#if defined(DOXYGEN)
AP_DECLARE(void) ap_log_perror(const char *file, int line, int module_index,
int level, apr_status_t status, apr_pool_t *p,
const char *fmt, ...);
#else
#if defined(AP_HAVE_C99) && defined(APLOG_MAX_LOGLEVEL)
#define ap_log_perror(...) ap_log_perror__(__VA_ARGS__)
#define ap_log_perror__(file, line, mi, level, status, p, ...) do { if ((level) <= APLOG_MAX_LOGLEVEL ) ap_log_perror_(file, line, mi, level, status, p, __VA_ARGS__); } while(0)
#else
#define ap_log_perror ap_log_perror_
#endif
AP_DECLARE(void) ap_log_perror_(const char *file, int line, int module_index,
int level, apr_status_t status, apr_pool_t *p,
const char *fmt, ...)
__attribute__((format(printf,7,8)));
#endif
#if defined(DOXYGEN)
AP_DECLARE(void) ap_log_rerror(const char *file, int line, int module_index,
int level, apr_status_t status,
const request_rec *r, const char *fmt, ...);
#else
#if defined(AP_HAVE_C99)
#define ap_log_rerror(...) ap_log_rerror__(__VA_ARGS__)
#define ap_log_rerror__(file, line, mi, level, status, r, ...) do { if (APLOG_R_MODULE_IS_LEVEL(r, mi, level)) ap_log_rerror_(file, line, mi, level, status, r, __VA_ARGS__); } while(0)
#else
#define ap_log_rerror ap_log_rerror_
#endif
AP_DECLARE(void) ap_log_rerror_(const char *file, int line, int module_index,
int level, apr_status_t status,
const request_rec *r, const char *fmt, ...)
__attribute__((format(printf,7,8)));
#endif
#if defined(DOXYGEN)
AP_DECLARE(void) ap_log_cerror(const char *file, int line, int module_index,
int level, apr_status_t status,
const conn_rec *c, const char *fmt, ...);
#else
#if defined(AP_HAVE_C99)
#define ap_log_cerror(...) ap_log_cerror__(__VA_ARGS__)
#define ap_log_cerror__(file, line, mi, level, status, c, ...) do { if (APLOG_C_MODULE_IS_LEVEL(c, mi, level)) ap_log_cerror_(file, line, mi, level, status, c, __VA_ARGS__); } while(0)
#else
#define ap_log_cerror ap_log_cerror_
#endif
AP_DECLARE(void) ap_log_cerror_(const char *file, int line, int module_index,
int level, apr_status_t status,
const conn_rec *c, const char *fmt, ...)
__attribute__((format(printf,7,8)));
#endif
#if defined(DOXYGEN)
AP_DECLARE(void) ap_log_cserror(const char *file, int line, int module_index,
int level, apr_status_t status,
const conn_rec *c, const server_rec *s,
const char *fmt, ...);
#else
#if defined(AP_HAVE_C99)
#define ap_log_cserror(...) ap_log_cserror__(__VA_ARGS__)
#define ap_log_cserror__(file, line, mi, level, status, c, s, ...) do { if (APLOG_CS_MODULE_IS_LEVEL(c, s, mi, level)) ap_log_cserror_(file, line, mi, level, status, c, s, __VA_ARGS__); } while(0)
#else
#define ap_log_cserror ap_log_cserror_
#endif
AP_DECLARE(void) ap_log_cserror_(const char *file, int line, int module_index,
int level, apr_status_t status,
const conn_rec *c, const server_rec *s,
const char *fmt, ...)
__attribute__((format(printf,8,9)));
#endif
#define AP_LOG_DATA_DEFAULT 0
#define AP_LOG_DATA_SHOW_OFFSET 1
#if defined(DOXYGEN)
AP_DECLARE(void) ap_log_data(const char *file, int line, int module_index,
int level, const server_rec *s, const char *label,
const void *data, apr_size_t len, unsigned int flags);
#else
#if defined(AP_HAVE_C99)
#define ap_log_data(...) ap_log_data__(__VA_ARGS__)
#define ap_log_data__(file, line, mi, level, s, ...) do { const server_rec *sr__ = s; if (APLOG_MODULE_IS_LEVEL(sr__, mi, level)) ap_log_data_(file, line, mi, level, sr__, __VA_ARGS__); } while(0)
#else
#define ap_log_data ap_log_data_
#endif
AP_DECLARE(void) ap_log_data_(const char *file, int line, int module_index,
int level, const server_rec *s, const char *label,
const void *data, apr_size_t len, unsigned int flags);
#endif
#if defined(DOXYGEN)
AP_DECLARE(void) ap_log_rdata(const char *file, int line, int module_index,
int level, const request_rec *r, const char *label,
const void *data, apr_size_t len, unsigned int flags);
#else
#if defined(AP_HAVE_C99)
#define ap_log_rdata(...) ap_log_rdata__(__VA_ARGS__)
#define ap_log_rdata__(file, line, mi, level, r, ...) do { if (APLOG_R_MODULE_IS_LEVEL(r, mi, level)) ap_log_rdata_(file, line, mi, level, r, __VA_ARGS__); } while(0)
#else
#define ap_log_rdata ap_log_rdata_
#endif
AP_DECLARE(void) ap_log_rdata_(const char *file, int line, int module_index,
int level, const request_rec *r, const char *label,
const void *data, apr_size_t len, unsigned int flags);
#endif
#if defined(DOXYGEN)
AP_DECLARE(void) ap_log_cdata(const char *file, int line, int module_index,
int level, const conn_rec *c, const char *label,
const void *data, apr_size_t len, unsigned int flags);
#else
#if defined(AP_HAVE_C99)
#define ap_log_cdata(...) ap_log_cdata__(__VA_ARGS__)
#define ap_log_cdata__(file, line, mi, level, c, ...) do { if (APLOG_C_MODULE_IS_LEVEL(c, mi, level)) ap_log_cdata_(file, line, mi, level, c, __VA_ARGS__); } while(0)
#else
#define ap_log_cdata ap_log_cdata_
#endif
AP_DECLARE(void) ap_log_cdata_(const char *file, int line, int module_index,
int level, const conn_rec *c, const char *label,
const void *data, apr_size_t len, unsigned int flags);
#endif
#if defined(DOXYGEN)
AP_DECLARE(void) ap_log_csdata(const char *file, int line, int module_index,
int level, const conn_rec *c, const server_rec *s,
const char *label, const void *data,
apr_size_t len, unsigned int flags);
#else
#if defined(AP_HAVE_C99)
#define ap_log_csdata(...) ap_log_csdata__(__VA_ARGS__)
#define ap_log_csdata__(file, line, mi, level, c, s, ...) do { if (APLOG_CS_MODULE_IS_LEVEL(c, s, mi, level)) ap_log_csdata_(file, line, mi, level, c, s, __VA_ARGS__); } while(0)
#else
#define ap_log_cdata ap_log_cdata_
#endif
AP_DECLARE(void) ap_log_csdata_(const char *file, int line, int module_index,
int level, const conn_rec *c, const server_rec *s,
const char *label, const void *data,
apr_size_t len, unsigned int flags);
#endif
AP_DECLARE(void) ap_error_log2stderr(server_rec *s);
AP_DECLARE(void) ap_log_command_line(apr_pool_t *p, server_rec *s);
AP_DECLARE(void) ap_log_mpm_common(server_rec *s);
AP_DECLARE(void) ap_log_pid(apr_pool_t *p, const char *fname);
AP_DECLARE(void) ap_remove_pid(apr_pool_t *p, const char *fname);
AP_DECLARE(apr_status_t) ap_read_pid(apr_pool_t *p, const char *filename, pid_t *mypid);
typedef struct piped_log piped_log;
AP_DECLARE(piped_log *) ap_open_piped_log(apr_pool_t *p, const char *program);
AP_DECLARE(piped_log *) ap_open_piped_log_ex(apr_pool_t *p,
const char *program,
apr_cmdtype_e cmdtype);
AP_DECLARE(void) ap_close_piped_log(piped_log *pl);
AP_DECLARE(apr_file_t *) ap_piped_log_read_fd(piped_log *pl);
AP_DECLARE(apr_file_t *) ap_piped_log_write_fd(piped_log *pl);
AP_DECLARE_HOOK(int, generate_log_id,
(const conn_rec *c, const request_rec *r, const char **id))
#if defined(__cplusplus)
}
#endif
#endif
