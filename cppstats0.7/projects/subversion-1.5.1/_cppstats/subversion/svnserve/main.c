#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_general.h>
#include <apr_getopt.h>
#include <apr_network_io.h>
#include <apr_signal.h>
#include <apr_thread_proc.h>
#include <apr_portable.h>
#include <locale.h>
#include "svn_cmdline.h"
#include "svn_types.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_ra_svn.h"
#include "svn_utf.h"
#include "svn_path.h"
#include "svn_opt.h"
#include "svn_repos.h"
#include "svn_fs.h"
#include "svn_version.h"
#include "svn_io.h"
#include "svn_private_config.h"
#include "winservice.h"
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include "server.h"
enum connection_handling_mode {
connection_mode_fork,
connection_mode_thread,
connection_mode_single
};
enum run_mode {
run_mode_unspecified,
run_mode_inetd,
run_mode_daemon,
run_mode_tunnel,
run_mode_listen_once,
run_mode_service
};
#if APR_HAS_FORK
#if APR_HAS_THREADS
#define CONNECTION_DEFAULT connection_mode_fork
#define CONNECTION_HAVE_THREAD_OPTION
#else
#define CONNECTION_DEFAULT connection_mode_fork
#endif
#elif APR_HAS_THREADS
#define CONNECTION_DEFAULT connection_mode_thread
#else
#define CONNECTION_DEFAULT connection_mode_single
#endif
#if defined(WIN32)
static apr_os_sock_t winservice_svnserve_accept_socket = INVALID_SOCKET;
void winservice_notify_stop(void) {
if (winservice_svnserve_accept_socket != INVALID_SOCKET)
closesocket(winservice_svnserve_accept_socket);
}
#endif
#define SVNSERVE_OPT_LISTEN_PORT 256
#define SVNSERVE_OPT_LISTEN_HOST 257
#define SVNSERVE_OPT_FOREGROUND 258
#define SVNSERVE_OPT_TUNNEL_USER 259
#define SVNSERVE_OPT_VERSION 260
#define SVNSERVE_OPT_PID_FILE 261
#define SVNSERVE_OPT_SERVICE 262
#define SVNSERVE_OPT_CONFIG_FILE 263
static const apr_getopt_option_t svnserve__options[] = {
{"daemon", 'd', 0, N_("daemon mode")},
{
"listen-port", SVNSERVE_OPT_LISTEN_PORT, 1,
N_("listen port (for daemon mode)")
},
{
"listen-host", SVNSERVE_OPT_LISTEN_HOST, 1,
N_("listen hostname or IP address (for daemon mode)")
},
{
"foreground", SVNSERVE_OPT_FOREGROUND, 0,
N_("run in foreground (useful for debugging)")
},
{"help", 'h', 0, N_("display this help")},
{
"version", SVNSERVE_OPT_VERSION, 0,
N_("show program version information")
},
{"inetd", 'i', 0, N_("inetd mode")},
{"root", 'r', 1, N_("root of directory to serve")},
{
"read-only", 'R', 0,
N_("force read only, overriding repository config file")
},
{"tunnel", 't', 0, N_("tunnel mode")},
{
"tunnel-user", SVNSERVE_OPT_TUNNEL_USER, 1,
N_("tunnel username (default is current uid's name)")
},
#if defined(CONNECTION_HAVE_THREAD_OPTION)
{"threads", 'T', 0, N_("use threads instead of fork")},
#endif
{"listen-once", 'X', 0, N_("listen once (useful for debugging)")},
{
"config-file", SVNSERVE_OPT_CONFIG_FILE, 1,
N_("read configuration from file ARG")
},
{
"pid-file", SVNSERVE_OPT_PID_FILE, 1,
N_("write server process ID to file ARG")
},
#if defined(WIN32)
{
"service", SVNSERVE_OPT_SERVICE, 0,
N_("run as a windows service (SCM only)")
},
#endif
{0, 0, 0, 0}
};
static void usage(const char *progname, apr_pool_t *pool) {
if (!progname)
progname = "svnserve";
svn_error_clear(svn_cmdline_fprintf(stderr, pool,
_("Type '%s --help' for usage.\n"),
progname));
exit(1);
}
static void help(apr_pool_t *pool) {
apr_size_t i;
svn_error_clear(svn_cmdline_fputs(_("usage: svnserve [options]\n"
"\n"
"Valid options:\n"),
stdout, pool));
for (i = 0; svnserve__options[i].name && svnserve__options[i].optch; i++) {
const char *optstr;
svn_opt_format_option(&optstr, svnserve__options + i, TRUE, pool);
svn_error_clear(svn_cmdline_fprintf(stdout, pool, " %s\n", optstr));
}
svn_error_clear(svn_cmdline_fprintf(stdout, pool, "\n"));
exit(0);
}
static svn_error_t * version(apr_pool_t *pool) {
const char *fs_desc_start
= _("The following repository back-end (FS) modules are available:\n\n");
svn_stringbuf_t *version_footer;
version_footer = svn_stringbuf_create(fs_desc_start, pool);
SVN_ERR(svn_fs_print_modules(version_footer, pool));
#if defined(SVN_HAVE_SASL)
svn_stringbuf_appendcstr(version_footer,
_("\nCyrus SASL authentication is available.\n"));
#endif
return svn_opt_print_help(NULL, "svnserve", TRUE, FALSE, version_footer->data,
NULL, NULL, NULL, NULL, pool);
}
#if APR_HAS_FORK
static void sigchld_handler(int signo) {
}
#endif
static apr_status_t redirect_stdout(void *arg) {
apr_pool_t *pool = arg;
apr_file_t *out_file, *err_file;
apr_status_t apr_err;
if ((apr_err = apr_file_open_stdout(&out_file, pool)))
return apr_err;
if ((apr_err = apr_file_open_stderr(&err_file, pool)))
return apr_err;
return apr_file_dup2(out_file, err_file, pool);
}
struct serve_thread_t {
svn_ra_svn_conn_t *conn;
serve_params_t *params;
apr_pool_t *pool;
};
#if APR_HAS_THREADS
static void * APR_THREAD_FUNC serve_thread(apr_thread_t *tid, void *data) {
struct serve_thread_t *d = data;
svn_error_clear(serve(d->conn, d->params, d->pool));
svn_pool_destroy(d->pool);
return NULL;
}
#endif
static svn_error_t *write_pid_file(const char *filename, apr_pool_t *pool) {
apr_file_t *file;
const char *contents = apr_psprintf(pool, "%" APR_PID_T_FMT "\n",
getpid());
SVN_ERR(svn_io_file_open(&file, filename,
APR_WRITE | APR_CREATE | APR_TRUNCATE,
APR_OS_DEFAULT, pool));
SVN_ERR(svn_io_file_write_full(file, contents, strlen(contents), NULL,
pool));
SVN_ERR(svn_io_file_close(file, pool));
return SVN_NO_ERROR;
}
static svn_error_t *
check_lib_versions(void) {
static const svn_version_checklist_t checklist[] = {
{ "svn_subr", svn_subr_version },
{ "svn_repos", svn_repos_version },
{ "svn_fs", svn_fs_version },
{ "svn_delta", svn_delta_version },
{ "svn_ra_svn", svn_ra_svn_version },
{ NULL, NULL }
};
SVN_VERSION_DEFINE(my_version);
return svn_ver_check_list(&my_version, checklist);
}
int main(int argc, const char *argv[]) {
enum run_mode run_mode = run_mode_unspecified;
svn_boolean_t foreground = FALSE;
apr_socket_t *sock, *usock;
apr_file_t *in_file, *out_file;
apr_sockaddr_t *sa;
apr_pool_t *pool;
apr_pool_t *connection_pool;
svn_error_t *err;
apr_getopt_t *os;
int opt;
serve_params_t params;
const char *arg;
apr_status_t status;
svn_ra_svn_conn_t *conn;
apr_proc_t proc;
#if APR_HAS_THREADS
apr_threadattr_t *tattr;
apr_thread_t *tid;
struct serve_thread_t *thread_data;
#endif
enum connection_handling_mode handling_mode = CONNECTION_DEFAULT;
apr_uint16_t port = SVN_RA_SVN_PORT;
const char *host = NULL;
int family = APR_INET;
int mode_opt_count = 0;
const char *config_filename = NULL;
const char *pid_filename = NULL;
svn_node_kind_t kind;
if (svn_cmdline_init("svnserve", stderr) != EXIT_SUCCESS)
return EXIT_FAILURE;
pool = svn_pool_create(NULL);
#if defined(SVN_HAVE_SASL)
SVN_INT_ERR(cyrus_init(pool));
#endif
err = check_lib_versions();
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnserve: ");
err = svn_fs_initialize(pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnserve: ");
err = svn_cmdline__getopt_init(&os, argc, argv, pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnserve: ");
params.root = "/";
params.tunnel = FALSE;
params.tunnel_user = NULL;
params.read_only = FALSE;
params.cfg = NULL;
params.pwdb = NULL;
params.authzdb = NULL;
while (1) {
status = apr_getopt_long(os, svnserve__options, &opt, &arg);
if (APR_STATUS_IS_EOF(status))
break;
if (status != APR_SUCCESS)
usage(argv[0], pool);
switch (opt) {
case 'h':
help(pool);
break;
case SVNSERVE_OPT_VERSION:
SVN_INT_ERR(version(pool));
exit(0);
break;
case 'd':
if (run_mode != run_mode_daemon) {
run_mode = run_mode_daemon;
mode_opt_count++;
}
break;
case SVNSERVE_OPT_FOREGROUND:
foreground = TRUE;
break;
case 'i':
if (run_mode != run_mode_inetd) {
run_mode = run_mode_inetd;
mode_opt_count++;
}
break;
case SVNSERVE_OPT_LISTEN_PORT:
port = atoi(arg);
break;
case SVNSERVE_OPT_LISTEN_HOST:
host = arg;
break;
case 't':
if (run_mode != run_mode_tunnel) {
run_mode = run_mode_tunnel;
mode_opt_count++;
}
break;
case SVNSERVE_OPT_TUNNEL_USER:
params.tunnel_user = arg;
break;
case 'X':
if (run_mode != run_mode_listen_once) {
run_mode = run_mode_listen_once;
mode_opt_count++;
}
break;
case 'r':
SVN_INT_ERR(svn_utf_cstring_to_utf8(&params.root, arg, pool));
err = svn_io_check_resolved_path(params.root, &kind, pool);
if (err)
return svn_cmdline_handle_exit_error(err, pool, "svnserve: ");
if (kind != svn_node_dir) {
svn_error_clear
(svn_cmdline_fprintf
(stderr, pool,
_("svnserve: Root path '%s' does not exist "
"or is not a directory.\n"), params.root));
return EXIT_FAILURE;
}
params.root = svn_path_internal_style(params.root, pool);
SVN_INT_ERR(svn_path_get_absolute(&params.root, params.root, pool));
break;
case 'R':
params.read_only = TRUE;
break;
case 'T':
handling_mode = connection_mode_thread;
break;
#if defined(WIN32)
case SVNSERVE_OPT_SERVICE:
if (run_mode != run_mode_service) {
run_mode = run_mode_service;
mode_opt_count++;
}
break;
#endif
case SVNSERVE_OPT_CONFIG_FILE:
SVN_INT_ERR(svn_utf_cstring_to_utf8(&config_filename, arg, pool));
config_filename = svn_path_internal_style(config_filename, pool);
SVN_INT_ERR(svn_path_get_absolute(&config_filename, config_filename,
pool));
break;
case SVNSERVE_OPT_PID_FILE:
SVN_INT_ERR(svn_utf_cstring_to_utf8(&pid_filename, arg, pool));
pid_filename = svn_path_internal_style(pid_filename, pool);
SVN_INT_ERR(svn_path_get_absolute(&pid_filename, pid_filename,
pool));
break;
}
}
if (os->ind != argc)
usage(argv[0], pool);
if (mode_opt_count != 1) {
svn_error_clear(svn_cmdline_fputs
(_("You must specify exactly one of -d, -i, -t or -X.\n"),
stderr, pool));
usage(argv[0], pool);
}
if (config_filename)
SVN_INT_ERR(load_configs(&params.cfg, &params.pwdb, &params.authzdb,
config_filename, TRUE,
svn_path_dirname(config_filename, pool),
pool));
if (params.tunnel_user && run_mode != run_mode_tunnel) {
svn_error_clear
(svn_cmdline_fprintf
(stderr, pool,
_("Option --tunnel-user is only valid in tunnel mode.\n")));
exit(1);
}
if (run_mode == run_mode_inetd || run_mode == run_mode_tunnel) {
params.tunnel = (run_mode == run_mode_tunnel);
apr_pool_cleanup_register(pool, pool, apr_pool_cleanup_null,
redirect_stdout);
status = apr_file_open_stdin(&in_file, pool);
if (status) {
err = svn_error_wrap_apr(status, _("Can't open stdin"));
return svn_cmdline_handle_exit_error(err, pool, "svnserve: ");
}
status = apr_file_open_stdout(&out_file, pool);
if (status) {
err = svn_error_wrap_apr(status, _("Can't open stdout"));
return svn_cmdline_handle_exit_error(err, pool, "svnserve: ");
}
conn = svn_ra_svn_create_conn(NULL, in_file, out_file, pool);
svn_error_clear(serve(conn, &params, pool));
exit(0);
}
#if defined(WIN32)
if (run_mode == run_mode_service) {
err = winservice_start();
if (err) {
svn_handle_error2(err, stderr, FALSE, "svnserve: ");
if (err->apr_err ==
APR_FROM_OS_ERROR(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)) {
svn_error_clear(svn_cmdline_fprintf(stderr, pool,
_("svnserve: The --service flag is only valid if the"
" process is started by the Service Control Manager.\n")));
}
svn_error_clear(err);
exit(1);
}
}
#endif
#if APR_HAVE_IPV6
#if defined(MAX_SECS_TO_LINGER)
status = apr_socket_create(&sock, APR_INET6, SOCK_STREAM, pool);
#else
status = apr_socket_create(&sock, APR_INET6, SOCK_STREAM, APR_PROTO_TCP,
pool);
#endif
if (status == 0) {
apr_socket_close(sock);
family = APR_UNSPEC;
}
#endif
status = apr_sockaddr_info_get(&sa, host, family, port, 0, pool);
if (status) {
err = svn_error_wrap_apr(status, _("Can't get address info"));
return svn_cmdline_handle_exit_error(err, pool, "svnserve: ");
}
#if defined(MAX_SECS_TO_LINGER)
status = apr_socket_create(&sock, sa->family, SOCK_STREAM, pool);
#else
status = apr_socket_create(&sock, sa->family, SOCK_STREAM, APR_PROTO_TCP,
pool);
#endif
if (status) {
err = svn_error_wrap_apr(status, _("Can't create server socket"));
return svn_cmdline_handle_exit_error(err, pool, "svnserve: ");
}
apr_socket_opt_set(sock, APR_SO_REUSEADDR, 1);
status = apr_socket_bind(sock, sa);
if (status) {
err = svn_error_wrap_apr(status, _("Can't bind server socket"));
return svn_cmdline_handle_exit_error(err, pool, "svnserve: ");
}
apr_socket_listen(sock, 7);
#if APR_HAS_FORK
if (run_mode != run_mode_listen_once && !foreground)
apr_proc_detach(APR_PROC_DETACH_DAEMONIZE);
apr_signal(SIGCHLD, sigchld_handler);
#endif
#if defined(SIGPIPE)
apr_signal(SIGPIPE, SIG_IGN);
#endif
#if defined(SIGXFSZ)
apr_signal(SIGXFSZ, SIG_IGN);
#endif
if (pid_filename)
SVN_INT_ERR(write_pid_file(pid_filename, pool));
#if defined(WIN32)
status = apr_os_sock_get(&winservice_svnserve_accept_socket, sock);
if (status)
winservice_svnserve_accept_socket = INVALID_SOCKET;
if (run_mode == run_mode_service)
winservice_running();
#endif
while (1) {
#if defined(WIN32)
if (winservice_is_stopping())
return ERROR_SUCCESS;
#endif
connection_pool = svn_pool_create(NULL);
status = apr_socket_accept(&usock, sock, connection_pool);
if (handling_mode == connection_mode_fork) {
while (apr_proc_wait_all_procs(&proc, NULL, NULL, APR_NOWAIT,
connection_pool) == APR_CHILD_DONE)
;
}
if (APR_STATUS_IS_EINTR(status)) {
svn_pool_destroy(connection_pool);
continue;
}
if (status) {
err = svn_error_wrap_apr
(status, _("Can't accept client connection"));
return svn_cmdline_handle_exit_error(err, pool, "svnserve: ");
}
conn = svn_ra_svn_create_conn(usock, NULL, NULL, connection_pool);
if (run_mode == run_mode_listen_once) {
err = serve(conn, &params, connection_pool);
if (err && err->apr_err != SVN_ERR_RA_SVN_CONNECTION_CLOSED)
svn_handle_error2(err, stdout, FALSE, "svnserve: ");
svn_error_clear(err);
apr_socket_close(usock);
apr_socket_close(sock);
exit(0);
}
switch (handling_mode) {
case connection_mode_fork:
#if APR_HAS_FORK
status = apr_proc_fork(&proc, connection_pool);
if (status == APR_INCHILD) {
apr_socket_close(sock);
svn_error_clear(serve(conn, &params, connection_pool));
apr_socket_close(usock);
exit(0);
} else if (status == APR_INPARENT) {
apr_socket_close(usock);
} else {
apr_socket_close(usock);
}
svn_pool_destroy(connection_pool);
#endif
break;
case connection_mode_thread:
#if APR_HAS_THREADS
status = apr_threadattr_create(&tattr, connection_pool);
if (status) {
err = svn_error_wrap_apr(status, _("Can't create threadattr"));
svn_handle_error2(err, stderr, FALSE, "svnserve: ");
svn_error_clear(err);
exit(1);
}
status = apr_threadattr_detach_set(tattr, 1);
if (status) {
err = svn_error_wrap_apr(status, _("Can't set detached state"));
svn_handle_error2(err, stderr, FALSE, "svnserve: ");
svn_error_clear(err);
exit(1);
}
thread_data = apr_palloc(connection_pool, sizeof(*thread_data));
thread_data->conn = conn;
thread_data->params = &params;
thread_data->pool = connection_pool;
status = apr_thread_create(&tid, tattr, serve_thread, thread_data,
connection_pool);
if (status) {
err = svn_error_wrap_apr(status, _("Can't create thread"));
svn_handle_error2(err, stderr, FALSE, "svnserve: ");
svn_error_clear(err);
exit(1);
}
#endif
break;
case connection_mode_single:
svn_error_clear(serve(conn, &params, connection_pool));
svn_pool_destroy(connection_pool);
}
}
}
