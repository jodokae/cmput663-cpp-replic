#if !defined(__mod_h2__h2_conn_io__)
#define __mod_h2__h2_conn_io__
struct h2_config;
struct h2_session;
typedef struct {
conn_rec *c;
apr_bucket_brigade *output;
int is_tls;
apr_time_t cooldown_usecs;
apr_int64_t warmup_size;
apr_size_t write_size;
apr_time_t last_write;
apr_int64_t bytes_read;
apr_int64_t bytes_written;
int buffer_output;
apr_size_t flush_threshold;
unsigned int is_flushed : 1;
char *scratch;
apr_size_t ssize;
apr_size_t slen;
} h2_conn_io;
apr_status_t h2_conn_io_init(h2_conn_io *io, conn_rec *c,
const struct h2_config *cfg);
apr_status_t h2_conn_io_write(h2_conn_io *io,
const char *buf,
size_t length);
apr_status_t h2_conn_io_pass(h2_conn_io *io, apr_bucket_brigade *bb);
apr_status_t h2_conn_io_flush(h2_conn_io *io);
int h2_conn_io_needs_flush(h2_conn_io *io);
#endif
