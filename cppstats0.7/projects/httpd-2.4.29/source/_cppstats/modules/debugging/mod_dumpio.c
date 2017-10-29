#include "httpd.h"
#include "http_connection.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "apr_strings.h"
module AP_MODULE_DECLARE_DATA dumpio_module ;
typedef struct dumpio_conf_t {
int enable_input;
int enable_output;
} dumpio_conf_t;
#define dumpio_MAX_STRING_LEN (MAX_STRING_LEN / 4 - 80)
static void dumpit(ap_filter_t *f, apr_bucket *b, dumpio_conf_t *ptr) {
conn_rec *c = f->c;
ap_log_cerror(APLOG_MARK, APLOG_TRACE7, 0, c,
"mod_dumpio: %s (%s-%s): %" APR_SIZE_T_FMT " bytes",
f->frec->name,
(APR_BUCKET_IS_METADATA(b)) ? "metadata" : "data",
b->type->name,
b->length) ;
if (!(APR_BUCKET_IS_METADATA(b))) {
#if APR_CHARSET_EBCDIC
char xlatebuf[dumpio_MAX_STRING_LEN + 1];
#endif
const char *buf;
apr_size_t nbytes;
apr_status_t rv = apr_bucket_read(b, &buf, &nbytes, APR_BLOCK_READ);
if (rv == APR_SUCCESS) {
while (nbytes) {
apr_size_t logbytes = nbytes;
if (logbytes > dumpio_MAX_STRING_LEN)
logbytes = dumpio_MAX_STRING_LEN;
nbytes -= logbytes;
#if APR_CHARSET_EBCDIC
memcpy(xlatebuf, buf, logbytes);
ap_xlate_proto_from_ascii(xlatebuf, logbytes);
xlatebuf[logbytes] = '\0';
ap_log_cerror(APLOG_MARK, APLOG_TRACE7, 0, c,
"mod_dumpio: %s (%s-%s): %s", f->frec->name,
(APR_BUCKET_IS_METADATA(b)) ? "metadata" : "data",
b->type->name, xlatebuf);
#else
ap_log_cerror(APLOG_MARK, APLOG_TRACE7, 0, c,
"mod_dumpio: %s (%s-%s): %.*s", f->frec->name,
(APR_BUCKET_IS_METADATA(b)) ? "metadata" : "data",
b->type->name, (int)logbytes, buf);
#endif
buf += logbytes;
}
} else {
ap_log_cerror(APLOG_MARK, APLOG_TRACE7, rv, c,
"mod_dumpio: %s (%s-%s): %s", f->frec->name,
(APR_BUCKET_IS_METADATA(b)) ? "metadata" : "data",
b->type->name, "error reading data");
}
}
}
#define whichmode( mode ) ( (( mode ) == AP_MODE_READBYTES) ? "readbytes" : (( mode ) == AP_MODE_GETLINE) ? "getline" : (( mode ) == AP_MODE_EATCRLF) ? "eatcrlf" : (( mode ) == AP_MODE_SPECULATIVE) ? "speculative" : (( mode ) == AP_MODE_EXHAUSTIVE) ? "exhaustive" : (( mode ) == AP_MODE_INIT) ? "init" : "unknown" )
static int dumpio_input_filter (ap_filter_t *f, apr_bucket_brigade *bb,
ap_input_mode_t mode, apr_read_type_e block, apr_off_t readbytes) {
apr_bucket *b;
apr_status_t ret;
conn_rec *c = f->c;
dumpio_conf_t *ptr = f->ctx;
ap_log_cerror(APLOG_MARK, APLOG_TRACE7, 0, c,
"mod_dumpio: %s [%s-%s] %" APR_OFF_T_FMT " readbytes",
f->frec->name,
whichmode(mode),
((block) == APR_BLOCK_READ) ? "blocking" : "nonblocking",
readbytes);
ret = ap_get_brigade(f->next, bb, mode, block, readbytes);
if (ret == APR_SUCCESS) {
for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = APR_BUCKET_NEXT(b)) {
dumpit(f, b, ptr);
}
} else {
ap_log_cerror(APLOG_MARK, APLOG_TRACE7, 0, c,
"mod_dumpio: %s - %d", f->frec->name, ret) ;
return ret;
}
return APR_SUCCESS ;
}
static int dumpio_output_filter (ap_filter_t *f, apr_bucket_brigade *bb) {
apr_bucket *b;
conn_rec *c = f->c;
dumpio_conf_t *ptr = f->ctx;
ap_log_cerror(APLOG_MARK, APLOG_TRACE7, 0, c, "mod_dumpio: %s", f->frec->name);
for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = APR_BUCKET_NEXT(b)) {
if (APR_BUCKET_IS_EOS(b)) {
apr_bucket *flush = apr_bucket_flush_create(f->c->bucket_alloc);
APR_BUCKET_INSERT_BEFORE(b, flush);
}
dumpit(f, b, ptr);
}
return ap_pass_brigade(f->next, bb) ;
}
static int dumpio_pre_conn(conn_rec *c, void *csd) {
dumpio_conf_t *ptr;
ptr = (dumpio_conf_t *) ap_get_module_config(c->base_server->module_config,
&dumpio_module);
if (ptr->enable_input)
ap_add_input_filter("DUMPIO_IN", ptr, NULL, c);
if (ptr->enable_output)
ap_add_output_filter("DUMPIO_OUT", ptr, NULL, c);
return OK;
}
static void dumpio_register_hooks(apr_pool_t *p) {
ap_register_output_filter("DUMPIO_OUT", dumpio_output_filter,
NULL, AP_FTYPE_CONNECTION + 3) ;
ap_register_input_filter("DUMPIO_IN", dumpio_input_filter,
NULL, AP_FTYPE_CONNECTION + 3) ;
ap_hook_pre_connection(dumpio_pre_conn, NULL, NULL, APR_HOOK_MIDDLE);
}
static void *dumpio_create_sconfig(apr_pool_t *p, server_rec *s) {
dumpio_conf_t *ptr = apr_pcalloc(p, sizeof *ptr);
ptr->enable_input = 0;
ptr->enable_output = 0;
return ptr;
}
static const char *dumpio_enable_input(cmd_parms *cmd, void *dummy, int arg) {
dumpio_conf_t *ptr = ap_get_module_config(cmd->server->module_config,
&dumpio_module);
ptr->enable_input = arg;
return NULL;
}
static const char *dumpio_enable_output(cmd_parms *cmd, void *dummy, int arg) {
dumpio_conf_t *ptr = ap_get_module_config(cmd->server->module_config,
&dumpio_module);
ptr->enable_output = arg;
return NULL;
}
static const command_rec dumpio_cmds[] = {
AP_INIT_FLAG("DumpIOInput", dumpio_enable_input, NULL,
RSRC_CONF, "Enable I/O Dump on Input Data"),
AP_INIT_FLAG("DumpIOOutput", dumpio_enable_output, NULL,
RSRC_CONF, "Enable I/O Dump on Output Data"),
{ NULL }
};
AP_DECLARE_MODULE(dumpio) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
dumpio_create_sconfig,
NULL,
dumpio_cmds,
dumpio_register_hooks
};
