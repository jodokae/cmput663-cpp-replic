#include <httpd.h>
#include <http_config.h>
#include <http_protocol.h>
#include <http_request.h>
#include <http_log.h>
#include <util_filter.h>
#include <ap_config.h>
#include <apr_strings.h>
#include <expat.h>
#include "mod_dav_svn.h"
#include "svn_string.h"
#include "svn_config.h"
module AP_MODULE_DECLARE_DATA dontdothat_module;
typedef struct {
const char *config_file;
const char *base_path;
int no_replay;
} dontdothat_config_rec;
static void *create_dontdothat_dir_config(apr_pool_t *pool, char *dir) {
dontdothat_config_rec *cfg = apr_pcalloc(pool, sizeof(*cfg));
cfg->base_path = dir;
cfg->no_replay = 1;
return cfg;
}
static const command_rec dontdothat_cmds[] = {
AP_INIT_TAKE1("DontDoThatConfigFile", ap_set_file_slot,
(void *) APR_OFFSETOF(dontdothat_config_rec, config_file),
OR_ALL,
"Text file containing actions to take for specific requests"),
AP_INIT_FLAG("DontDoThatDisallowReplay", ap_set_flag_slot,
(void *) APR_OFFSETOF(dontdothat_config_rec, no_replay),
OR_ALL, "Disallow replay requests as if they are other recursive requests."),
{ NULL }
};
typedef enum {
STATE_BEGINNING,
STATE_IN_UPDATE,
STATE_IN_SRC_PATH,
STATE_IN_DST_PATH,
STATE_IN_RECURSIVE
} parse_state_t;
typedef struct {
svn_boolean_t let_it_go;
svn_boolean_t no_soup_for_you;
XML_Parser xmlp;
parse_state_t state;
svn_stringbuf_t *buffer;
dontdothat_config_rec *cfg;
apr_array_header_t *allow_recursive_ops;
apr_array_header_t *no_recursive_ops;
svn_boolean_t path_failed;
svn_error_t *err;
request_rec *r;
} dontdothat_filter_ctx;
static svn_boolean_t
matches(const char *wc, const char *p) {
for (;;) {
switch (*wc) {
case '*':
if (wc[1] != '/' && wc[1] != '\0')
abort();
while (*p && p[1] != '/')
++p;
if (! *p) {
if (wc[1] == '\0')
return TRUE;
else
return FALSE;
}
break;
case '\0':
if (*p != '\0')
return FALSE;
else
return TRUE;
default:
if (*wc != *p)
return FALSE;
else
break;
}
++wc;
++p;
if (! *p && *wc)
return FALSE;
}
}
static svn_boolean_t
is_this_legal(dontdothat_filter_ctx *ctx, const char *uri) {
const char *relative_path;
const char *cleaned_uri;
const char *repos_name;
int trailing_slash;
dav_error *derr;
uri = ap_strstr_c(uri, "://");
if (uri)
uri = ap_strchr_c(uri + 3, '/');
if (uri) {
const char *repos_path;
derr = dav_svn_split_uri(ctx->r,
uri,
ctx->cfg->base_path,
&cleaned_uri,
&trailing_slash,
&repos_name,
&relative_path,
&repos_path);
if (! derr) {
int idx;
if (! repos_path)
repos_path = "";
repos_path = apr_psprintf(ctx->r->pool, "/%s", repos_path);
for (idx = 0; idx < ctx->allow_recursive_ops->nelts; ++idx) {
const char *wc = APR_ARRAY_IDX(ctx->allow_recursive_ops,
idx,
const char *);
if (matches(wc, repos_path)) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, ctx->r,
"mod_dontdothat: rule %s allows %s",
wc, repos_path);
return TRUE;
}
}
for (idx = 0; idx < ctx->no_recursive_ops->nelts; ++idx) {
const char *wc = APR_ARRAY_IDX(ctx->no_recursive_ops,
idx,
const char *);
if (matches(wc, repos_path)) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, ctx->r,
"mod_dontdothat: rule %s forbids %s",
wc, repos_path);
return FALSE;
}
}
}
}
return TRUE;
}
static apr_status_t
dontdothat_filter(ap_filter_t *f,
apr_bucket_brigade *bb,
ap_input_mode_t mode,
apr_read_type_e block,
apr_off_t readbytes) {
dontdothat_filter_ctx *ctx = f->ctx;
apr_status_t rv;
apr_bucket *e;
if (mode != AP_MODE_READBYTES)
return ap_get_brigade(f->next, bb, mode, block, readbytes);
rv = ap_get_brigade(f->next, bb, mode, block, readbytes);
if (rv)
return rv;
for (e = APR_BRIGADE_FIRST(bb);
e != APR_BRIGADE_SENTINEL(bb);
e = APR_BUCKET_NEXT(e)) {
svn_boolean_t last = APR_BUCKET_IS_EOS(e);
const char *str;
apr_size_t len;
if (last) {
str = "";
len = 0;
} else {
rv = apr_bucket_read(e, &str, &len, APR_BLOCK_READ);
if (rv)
return rv;
}
if (! XML_Parse(ctx->xmlp, str, len, last)) {
ctx->let_it_go = TRUE;
ctx->no_soup_for_you = TRUE;
}
if (ctx->no_soup_for_you) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r,
"mod_dontdothat: client broke the rules, "
"returning error");
bb = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
e = ap_bucket_error_create(403, "No Soup For You!",
f->r->pool, f->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, e);
e = apr_bucket_eos_create(f->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, e);
ap_remove_input_filter(f);
return ap_pass_brigade(f->r->output_filters, bb);
} else if (ctx->let_it_go || last) {
ap_remove_input_filter(f);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, f->r,
"mod_dontdothat: letting request go through");
return rv;
}
}
return rv;
}
static void
cdata(void *baton, const char *data, int len) {
dontdothat_filter_ctx *ctx = baton;
if (ctx->no_soup_for_you || ctx->let_it_go)
return;
switch (ctx->state) {
case STATE_IN_SRC_PATH:
case STATE_IN_DST_PATH:
case STATE_IN_RECURSIVE:
if (! ctx->buffer)
ctx->buffer = svn_stringbuf_ncreate(data, len, ctx->r->pool);
else
svn_stringbuf_appendbytes(ctx->buffer, data, len);
break;
default:
break;
}
}
static void
start_element(void *baton, const char *name, const char **attrs) {
dontdothat_filter_ctx *ctx = baton;
const char *sep;
if (ctx->no_soup_for_you || ctx->let_it_go)
return;
sep = ap_strchr_c(name, ':');
if (sep)
name = sep + 1;
switch (ctx->state) {
case STATE_BEGINNING:
if (strcmp(name, "update-report") == 0)
ctx->state = STATE_IN_UPDATE;
else if (strcmp(name, "replay-report") == 0 && ctx->cfg->no_replay) {
if (! is_this_legal(ctx, ctx->r->unparsed_uri))
ctx->no_soup_for_you = TRUE;
else
ctx->let_it_go = TRUE;
} else
ctx->let_it_go = TRUE;
break;
case STATE_IN_UPDATE:
if (strcmp(name, "src-path") == 0) {
ctx->state = STATE_IN_SRC_PATH;
if (ctx->buffer)
ctx->buffer->len = 0;
} else if (strcmp(name, "dst-path") == 0) {
ctx->state = STATE_IN_DST_PATH;
if (ctx->buffer)
ctx->buffer->len = 0;
} else if (strcmp(name, "recursive") == 0) {
ctx->state = STATE_IN_RECURSIVE;
if (ctx->buffer)
ctx->buffer->len = 0;
} else
;
break;
default:
break;
}
}
static void
end_element(void *baton, const char *name) {
dontdothat_filter_ctx *ctx = baton;
const char *sep;
if (ctx->no_soup_for_you || ctx->let_it_go)
return;
sep = ap_strchr_c(name, ':');
if (sep)
name = sep + 1;
switch (ctx->state) {
case STATE_IN_SRC_PATH:
ctx->state = STATE_IN_UPDATE;
svn_stringbuf_strip_whitespace(ctx->buffer);
if (! ctx->path_failed && ! is_this_legal(ctx, ctx->buffer->data))
ctx->path_failed = TRUE;
break;
case STATE_IN_DST_PATH:
ctx->state = STATE_IN_UPDATE;
svn_stringbuf_strip_whitespace(ctx->buffer);
if (! ctx->path_failed && ! is_this_legal(ctx, ctx->buffer->data))
ctx->path_failed = TRUE;
break;
case STATE_IN_RECURSIVE:
ctx->state = STATE_IN_UPDATE;
svn_stringbuf_strip_whitespace(ctx->buffer);
if (strcmp(ctx->buffer->data, "no") == 0) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, ctx->r,
"mod_dontdothat: letting nonrecursive request go");
ctx->let_it_go = TRUE;
}
break;
case STATE_IN_UPDATE:
if (strcmp(name, "update-report") == 0) {
if (ctx->path_failed)
ctx->no_soup_for_you = TRUE;
else
ctx->let_it_go = TRUE;
} else
;
break;
default:
abort();
}
}
static svn_boolean_t
is_valid_wildcard(const char *wc) {
while (*wc) {
if (*wc == '*') {
if (wc[1] && wc[1] != '/')
return FALSE;
}
++wc;
}
return TRUE;
}
static svn_boolean_t
config_enumerator(const char *wildcard,
const char *action,
void *baton,
apr_pool_t *pool) {
dontdothat_filter_ctx *ctx = baton;
if (strcmp(action, "deny") == 0) {
if (is_valid_wildcard(wildcard))
APR_ARRAY_PUSH(ctx->no_recursive_ops, const char *) = wildcard;
else
ctx->err = svn_error_createf(APR_EINVAL,
NULL,
"'%s' is an invalid wildcard",
wildcard);
} else if (strcmp(action, "allow") == 0) {
if (is_valid_wildcard(wildcard))
APR_ARRAY_PUSH(ctx->allow_recursive_ops, const char *) = wildcard;
else
ctx->err = svn_error_createf(APR_EINVAL,
NULL,
"'%s' is an invalid wildcard",
wildcard);
} else {
ctx->err = svn_error_createf(APR_EINVAL,
NULL,
"'%s' is not a valid action",
action);
}
if (ctx->err)
return FALSE;
else
return TRUE;
}
static apr_status_t
clean_up_parser(void *baton) {
XML_Parser xmlp = baton;
XML_ParserFree(xmlp);
return APR_SUCCESS;
}
static void
dontdothat_insert_filters(request_rec *r) {
dontdothat_config_rec *cfg = ap_get_module_config(r->per_dir_config,
&dontdothat_module);
if (! cfg->config_file)
return;
if (strcmp("REPORT", r->method) == 0) {
dontdothat_filter_ctx *ctx = apr_pcalloc(r->pool, sizeof(*ctx));
svn_config_t *config;
svn_error_t *err;
ctx->r = r;
ctx->cfg = cfg;
ctx->allow_recursive_ops = apr_array_make(r->pool, 5, sizeof(char *));
ctx->no_recursive_ops = apr_array_make(r->pool, 5, sizeof(char *));
err = svn_config_read(&config, cfg->config_file, TRUE, r->pool);
if (err) {
char buff[256];
ap_log_rerror(APLOG_MARK, APLOG_ERR,
((err->apr_err >= APR_OS_START_USERERR &&
err->apr_err < APR_OS_START_CANONERR) ?
0 : err->apr_err),
r, "Failed to load DontDoThatConfigFile: %s",
svn_err_best_message(err, buff, sizeof(buff)));
svn_error_clear(err);
return;
}
svn_config_enumerate2(config,
"recursive-actions",
config_enumerator,
ctx,
r->pool);
if (ctx->err) {
char buff[256];
ap_log_rerror(APLOG_MARK, APLOG_ERR,
((ctx->err->apr_err >= APR_OS_START_USERERR &&
ctx->err->apr_err < APR_OS_START_CANONERR) ?
0 : ctx->err->apr_err),
r, "Failed to parse DontDoThatConfigFile: %s",
svn_err_best_message(ctx->err, buff, sizeof(buff)));
svn_error_clear(ctx->err);
return;
}
ctx->state = STATE_BEGINNING;
ctx->xmlp = XML_ParserCreate(NULL);
apr_pool_cleanup_register(r->pool, ctx->xmlp,
clean_up_parser,
apr_pool_cleanup_null);
XML_SetUserData(ctx->xmlp, ctx);
XML_SetElementHandler(ctx->xmlp, start_element, end_element);
XML_SetCharacterDataHandler(ctx->xmlp, cdata);
ap_add_input_filter("DONTDOTHAT_FILTER", ctx, r, r->connection);
}
}
static void
dontdothat_register_hooks(apr_pool_t *pool) {
ap_hook_insert_filter(dontdothat_insert_filters, NULL, NULL, APR_HOOK_FIRST);
ap_register_input_filter("DONTDOTHAT_FILTER",
dontdothat_filter,
NULL,
AP_FTYPE_RESOURCE);
}
module AP_MODULE_DECLARE_DATA dontdothat_module = {
STANDARD20_MODULE_STUFF,
create_dontdothat_dir_config,
NULL,
NULL,
NULL,
dontdothat_cmds,
dontdothat_register_hooks
};
