#if !defined(WINSERVICE_H)
#define WINSERVICE_H
#if defined(__cplusplus)
extern "C" {
#endif
#if defined(WIN32)
svn_error_t *winservice_start(void);
void winservice_running(void);
void winservice_notify_stop(void);
svn_boolean_t winservice_is_stopping(void);
#endif
#if defined(__cplusplus)
}
#endif
#endif
