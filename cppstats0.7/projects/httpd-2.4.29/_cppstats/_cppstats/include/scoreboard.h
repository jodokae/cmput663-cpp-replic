#if !defined(APACHE_SCOREBOARD_H)
#define APACHE_SCOREBOARD_H
#if defined(__cplusplus)
extern "C" {
#endif
#if APR_HAVE_SYS_TIME_H
#include <sys/time.h>
#include <sys/times.h>
#endif
#include "ap_config.h"
#include "http_config.h"
#include "apr_thread_proc.h"
#include "apr_portable.h"
#include "apr_shm.h"
#include "apr_optional.h"
#if !defined(DEFAULT_SCOREBOARD)
#define DEFAULT_SCOREBOARD "logs/apache_runtime_status"
#endif
#define SERVER_DEAD 0
#define SERVER_STARTING 1
#define SERVER_READY 2
#define SERVER_BUSY_READ 3
#define SERVER_BUSY_WRITE 4
#define SERVER_BUSY_KEEPALIVE 5
#define SERVER_BUSY_LOG 6
#define SERVER_BUSY_DNS 7
#define SERVER_CLOSING 8
#define SERVER_GRACEFUL 9
#define SERVER_IDLE_KILL 10
#define SERVER_NUM_STATUS 11
typedef int ap_generation_t;
typedef enum {
SB_NOT_SHARED = 1,
SB_SHARED = 2
} ap_scoreboard_e;
typedef struct worker_score worker_score;
struct worker_score {
#if APR_HAS_THREADS
apr_os_thread_t tid;
#endif
int thread_num;
pid_t pid;
ap_generation_t generation;
unsigned char status;
unsigned short conn_count;
apr_off_t conn_bytes;
unsigned long access_count;
apr_off_t bytes_served;
unsigned long my_access_count;
apr_off_t my_bytes_served;
apr_time_t start_time;
apr_time_t stop_time;
apr_time_t last_used;
#if defined(HAVE_TIMES)
struct tms times;
#endif
char client[32];
char request[64];
char vhost[32];
char protocol[16];
};
typedef struct {
int server_limit;
int thread_limit;
ap_generation_t running_generation;
apr_time_t restart_time;
} global_score;
typedef struct process_score process_score;
struct process_score {
pid_t pid;
ap_generation_t generation;
char quiescing;
char not_accepting;
apr_uint32_t connections;
apr_uint32_t write_completion;
apr_uint32_t lingering_close;
apr_uint32_t keep_alive;
apr_uint32_t suspended;
int bucket;
};
typedef struct {
global_score *global;
process_score *parent;
worker_score **servers;
} scoreboard;
typedef struct ap_sb_handle_t ap_sb_handle_t;
int ap_create_scoreboard(apr_pool_t *p, ap_scoreboard_e t);
apr_status_t ap_cleanup_scoreboard(void *d);
AP_DECLARE(int) ap_exists_scoreboard_image(void);
AP_DECLARE(void) ap_increment_counts(ap_sb_handle_t *sbh, request_rec *r);
AP_DECLARE(apr_status_t) ap_reopen_scoreboard(apr_pool_t *p, apr_shm_t **shm, int detached);
AP_DECLARE(void) ap_init_scoreboard(void *shared_score);
AP_DECLARE(int) ap_calc_scoreboard_size(void);
AP_DECLARE(void) ap_create_sb_handle(ap_sb_handle_t **new_sbh, apr_pool_t *p,
int child_num, int thread_num);
AP_DECLARE(int) ap_find_child_by_pid(apr_proc_t *pid);
AP_DECLARE(int) ap_update_child_status(ap_sb_handle_t *sbh, int status, request_rec *r);
AP_DECLARE(int) ap_update_child_status_from_indexes(int child_num, int thread_num,
int status, request_rec *r);
AP_DECLARE(int) ap_update_child_status_from_conn(ap_sb_handle_t *sbh, int status, conn_rec *c);
AP_DECLARE(int) ap_update_child_status_from_server(ap_sb_handle_t *sbh, int status,
conn_rec *c, server_rec *s);
AP_DECLARE(int) ap_update_child_status_descr(ap_sb_handle_t *sbh, int status, const char *descr);
AP_DECLARE(void) ap_time_process_request(ap_sb_handle_t *sbh, int status);
AP_DECLARE(worker_score *) ap_get_scoreboard_worker(ap_sb_handle_t *sbh);
AP_DECLARE(worker_score *) ap_get_scoreboard_worker_from_indexes(int child_num,
int thread_num);
AP_DECLARE(void) ap_copy_scoreboard_worker(worker_score *dest,
int child_num, int thread_num);
AP_DECLARE(process_score *) ap_get_scoreboard_process(int x);
AP_DECLARE(global_score *) ap_get_scoreboard_global(void);
AP_DECLARE_DATA extern scoreboard *ap_scoreboard_image;
AP_DECLARE_DATA extern const char *ap_scoreboard_fname;
AP_DECLARE_DATA extern int ap_extended_status;
AP_DECLARE_DATA extern int ap_mod_status_reqtail;
const char *ap_set_scoreboard(cmd_parms *cmd, void *dummy, const char *arg);
const char *ap_set_extended_status(cmd_parms *cmd, void *dummy, int arg);
const char *ap_set_reqtail(cmd_parms *cmd, void *dummy, int arg);
AP_DECLARE_HOOK(int, pre_mpm, (apr_pool_t *p, ap_scoreboard_e sb_type))
#define START_PREQUEST 1
#define STOP_PREQUEST 2
#if defined(__cplusplus)
}
#endif
#endif