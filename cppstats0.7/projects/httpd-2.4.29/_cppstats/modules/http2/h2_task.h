#if !defined(__mod_h2__h2_task__)
#define __mod_h2__h2_task__
#include <http_core.h>
struct h2_bucket_beam;
struct h2_conn;
struct h2_mplx;
struct h2_task;
struct h2_req_engine;
struct h2_request;
struct h2_response_parser;
struct h2_stream;
struct h2_worker;
typedef struct h2_task h2_task;
struct h2_task {
const char *id;
int stream_id;
conn_rec *c;
apr_pool_t *pool;
const struct h2_request *request;
apr_interval_time_t timeout;
int rst_error;
struct {
struct h2_bucket_beam *beam;
unsigned int eos : 1;
apr_bucket_brigade *bb;
apr_bucket_brigade *bbchunk;
apr_off_t chunked_total;
} input;
struct {
struct h2_bucket_beam *beam;
unsigned int opened : 1;
unsigned int sent_response : 1;
unsigned int copy_files : 1;
struct h2_response_parser *rparser;
apr_bucket_brigade *bb;
apr_size_t max_buffer;
} output;
struct h2_mplx *mplx;
unsigned int filters_set : 1;
unsigned int frozen : 1;
unsigned int thawed : 1;
unsigned int worker_started : 1;
unsigned int worker_done : 1;
apr_time_t started_at;
apr_time_t done_at;
apr_bucket *eor;
struct h2_req_engine *engine;
struct h2_req_engine *assigned;
};
h2_task *h2_task_create(conn_rec *slave, int stream_id,
const h2_request *req, struct h2_mplx *m,
struct h2_bucket_beam *input,
apr_interval_time_t timeout,
apr_size_t output_max_mem);
void h2_task_destroy(h2_task *task);
apr_status_t h2_task_do(h2_task *task, apr_thread_t *thread, int worker_id);
void h2_task_redo(h2_task *task);
int h2_task_can_redo(h2_task *task);
void h2_task_rst(h2_task *task, int error);
void h2_task_register_hooks(void);
apr_status_t h2_task_init(apr_pool_t *pool, server_rec *s);
extern APR_OPTIONAL_FN_TYPE(ap_logio_add_bytes_in) *h2_task_logio_add_bytes_in;
extern APR_OPTIONAL_FN_TYPE(ap_logio_add_bytes_out) *h2_task_logio_add_bytes_out;
apr_status_t h2_task_freeze(h2_task *task);
apr_status_t h2_task_thaw(h2_task *task);
int h2_task_has_thawed(h2_task *task);
#endif