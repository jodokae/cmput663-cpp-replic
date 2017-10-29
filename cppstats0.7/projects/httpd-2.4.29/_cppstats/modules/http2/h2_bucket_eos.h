#if !defined(mod_http2_h2_bucket_stream_eos_h)
#define mod_http2_h2_bucket_stream_eos_h
struct h2_stream;
extern const apr_bucket_type_t h2_bucket_type_eos;
#define H2_BUCKET_IS_H2EOS(e) (e->type == &h2_bucket_type_eos)
apr_bucket *h2_bucket_eos_make(apr_bucket *b, struct h2_stream *stream);
apr_bucket *h2_bucket_eos_create(apr_bucket_alloc_t *list,
struct h2_stream *stream);
#endif
