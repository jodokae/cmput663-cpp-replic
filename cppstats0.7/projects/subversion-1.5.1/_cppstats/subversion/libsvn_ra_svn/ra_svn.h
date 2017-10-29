#if !defined(RA_SVN_H)
#define RA_SVN_H
#if defined(__cplusplus)
extern "C" {
#endif
#include <apr_network_io.h>
#include <apr_file_io.h>
#include <apr_thread_proc.h>
#include "svn_ra.h"
#include "svn_ra_svn.h"
typedef svn_boolean_t (*ra_svn_pending_fn_t)(void *baton);
typedef void (*ra_svn_timeout_fn_t)(void *baton, apr_interval_time_t timeout);
typedef struct svn_ra_svn__stream_st svn_ra_svn__stream_t;
typedef svn_error_t *(*ra_svn_block_handler_t)(svn_ra_svn_conn_t *conn,
apr_pool_t *pool,
void *baton);
#define SVN_RA_SVN__READBUF_SIZE 4096
#define SVN_RA_SVN__WRITEBUF_SIZE 4096
typedef struct svn_ra_svn__session_baton_t svn_ra_svn__session_baton_t;
struct svn_ra_svn_conn_st {
svn_ra_svn__stream_t *stream;
svn_ra_svn__session_baton_t *session;
#if defined(SVN_HAVE_SASL)
apr_socket_t *sock;
svn_boolean_t encrypted;
#endif
char read_buf[SVN_RA_SVN__READBUF_SIZE];
char *read_ptr;
char *read_end;
char write_buf[SVN_RA_SVN__WRITEBUF_SIZE];
int write_pos;
const char *uuid;
const char *repos_root;
ra_svn_block_handler_t block_handler;
void *block_baton;
apr_hash_t *capabilities;
apr_pool_t *pool;
};
struct svn_ra_svn__session_baton_t {
apr_pool_t *pool;
svn_ra_svn_conn_t *conn;
svn_boolean_t is_tunneled;
const char *url;
const char *user;
const char *hostname;
const char *realm_prefix;
const char **tunnel_argv;
const svn_ra_callbacks2_t *callbacks;
void *callbacks_baton;
apr_off_t bytes_read, bytes_written;
};
void svn_ra_svn__set_block_handler(svn_ra_svn_conn_t *conn,
ra_svn_block_handler_t callback,
void *baton);
svn_boolean_t svn_ra_svn__input_waiting(svn_ra_svn_conn_t *conn,
apr_pool_t *pool);
svn_error_t *svn_ra_svn__cram_client(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
const char *user, const char *password,
const char **message);
svn_error_t *svn_ra_svn__handle_failure_status(apr_array_header_t *params,
apr_pool_t *pool);
svn_ra_svn__stream_t *svn_ra_svn__stream_from_sock(apr_socket_t *sock,
apr_pool_t *pool);
svn_ra_svn__stream_t *svn_ra_svn__stream_from_files(apr_file_t *in_file,
apr_file_t *out_file,
apr_pool_t *pool);
svn_ra_svn__stream_t *svn_ra_svn__stream_create(void *baton,
svn_read_fn_t read_cb,
svn_write_fn_t write_cb,
ra_svn_timeout_fn_t timeout_cb,
ra_svn_pending_fn_t pending_cb,
apr_pool_t *pool);
svn_error_t *svn_ra_svn__stream_write(svn_ra_svn__stream_t *stream,
const char *data, apr_size_t *len);
svn_error_t *svn_ra_svn__stream_read(svn_ra_svn__stream_t *stream,
char *data, apr_size_t *len);
void svn_ra_svn__stream_timeout(svn_ra_svn__stream_t *stream,
apr_interval_time_t interval);
svn_boolean_t svn_ra_svn__stream_pending(svn_ra_svn__stream_t *stream);
svn_error_t *
svn_ra_svn__do_cyrus_auth(svn_ra_svn__session_baton_t *sess,
apr_array_header_t *mechlist,
const char *realm, apr_pool_t *pool);
svn_error_t *
svn_ra_svn__do_internal_auth(svn_ra_svn__session_baton_t *sess,
apr_array_header_t *mechlist,
const char *realm, apr_pool_t *pool);
svn_error_t *svn_ra_svn__auth_response(svn_ra_svn_conn_t *conn,
apr_pool_t *pool,
const char *mech, const char *mech_arg);
svn_error_t *svn_ra_svn__sasl_init(void);
#if defined(__cplusplus)
}
#endif
#endif