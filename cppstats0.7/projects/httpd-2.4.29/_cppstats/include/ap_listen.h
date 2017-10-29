#if !defined(AP_LISTEN_H)
#define AP_LISTEN_H
#include "apr_network_io.h"
#include "httpd.h"
#include "http_config.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct ap_slave_t ap_slave_t;
typedef struct ap_listen_rec ap_listen_rec;
typedef apr_status_t (*accept_function)(void **csd, ap_listen_rec *lr, apr_pool_t *ptrans);
struct ap_listen_rec {
ap_listen_rec *next;
apr_socket_t *sd;
apr_sockaddr_t *bind_addr;
accept_function accept_func;
int active;
const char* protocol;
ap_slave_t *slave;
};
AP_DECLARE_DATA extern ap_listen_rec *ap_listeners;
AP_DECLARE_DATA extern int ap_num_listen_buckets;
AP_DECLARE_DATA extern int ap_have_so_reuseport;
AP_DECLARE(void) ap_listen_pre_config(void);
AP_DECLARE(int) ap_setup_listeners(server_rec *s);
AP_DECLARE(apr_status_t) ap_duplicate_listeners(apr_pool_t *p, server_rec *s,
ap_listen_rec ***buckets,
int *num_buckets);
AP_DECLARE_NONSTD(void) ap_close_listeners(void);
AP_DECLARE_NONSTD(void) ap_close_listeners_ex(ap_listen_rec *listeners);
AP_DECLARE_NONSTD(int) ap_close_selected_listeners(ap_slave_t *);
AP_DECLARE_NONSTD(const char *) ap_set_listenbacklog(cmd_parms *cmd, void *dummy, const char *arg);
AP_DECLARE_NONSTD(const char *) ap_set_listencbratio(cmd_parms *cmd, void *dummy, const char *arg);
AP_DECLARE_NONSTD(const char *) ap_set_listener(cmd_parms *cmd, void *dummy,
int argc, char *const argv[]);
AP_DECLARE_NONSTD(const char *) ap_set_send_buffer_size(cmd_parms *cmd, void *dummy,
const char *arg);
AP_DECLARE_NONSTD(const char *) ap_set_receive_buffer_size(cmd_parms *cmd,
void *dummy,
const char *arg);
#define LISTEN_COMMANDS AP_INIT_TAKE1("ListenBacklog", ap_set_listenbacklog, NULL, RSRC_CONF, "Maximum length of the queue of pending connections, as used by listen(2)"), AP_INIT_TAKE1("ListenCoresBucketsRatio", ap_set_listencbratio, NULL, RSRC_CONF, "Ratio between the number of CPU cores (online) and the number of listeners buckets"), AP_INIT_TAKE_ARGV("Listen", ap_set_listener, NULL, RSRC_CONF, "A port number or a numeric IP address and a port number, and an optional protocol"), AP_INIT_TAKE1("SendBufferSize", ap_set_send_buffer_size, NULL, RSRC_CONF, "Send buffer size in bytes"), AP_INIT_TAKE1("ReceiveBufferSize", ap_set_receive_buffer_size, NULL, RSRC_CONF, "Receive buffer size in bytes")
#if defined(__cplusplus)
}
#endif
#endif
