#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "apr_general.h"
#include "apr_strings.h"
#include "apr_strmatch.h"
#include "apr_lib.h"
#include "util_filter.h"
#include "util_varbuf.h"
#include "apr_buckets.h"
#include "http_request.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#define AP_SUBST_MAX_LINE_LENGTH (1024*1024)
static const char substitute_filter_name[] = "SUBSTITUTE";
module AP_MODULE_DECLARE_DATA substitute_module;
typedef struct subst_pattern_t {
const apr_strmatch_pattern *pattern;
const ap_regex_t *regexp;
const char *replacement;
apr_size_t replen;
apr_size_t patlen;
int flatten;
} subst_pattern_t;
typedef struct {
apr_array_header_t *patterns;
apr_size_t max_line_length;
int max_line_length_set;
int inherit_before;
} subst_dir_conf;
typedef struct {
apr_bucket_brigade *linebb;
apr_bucket_brigade *linesbb;
apr_bucket_brigade *passbb;
apr_bucket_brigade *pattbb;
apr_pool_t *tpool;
} substitute_module_ctx;
static void *create_substitute_dcfg(apr_pool_t *p, char *d) {
subst_dir_conf *dcfg =
(subst_dir_conf *) apr_palloc(p, sizeof(subst_dir_conf));
dcfg->patterns = apr_array_make(p, 10, sizeof(subst_pattern_t));
dcfg->max_line_length = AP_SUBST_MAX_LINE_LENGTH;
dcfg->max_line_length_set = 0;
dcfg->inherit_before = -1;
return dcfg;
}
static void *merge_substitute_dcfg(apr_pool_t *p, void *basev, void *overv) {
subst_dir_conf *a =
(subst_dir_conf *) apr_palloc(p, sizeof(subst_dir_conf));
subst_dir_conf *base = (subst_dir_conf *) basev;
subst_dir_conf *over = (subst_dir_conf *) overv;
a->inherit_before = (over->inherit_before != -1)
? over->inherit_before
: base->inherit_before;
if (a->inherit_before == 1) {
a->patterns = apr_array_append(p, base->patterns,
over->patterns);
} else {
a->patterns = apr_array_append(p, over->patterns,
base->patterns);
}
a->max_line_length = over->max_line_length_set ?
over->max_line_length : base->max_line_length;
a->max_line_length_set = over->max_line_length_set
| base->max_line_length_set;
return a;
}
#define AP_MAX_BUCKETS 1000
#define SEDRMPATBCKT(b, offset, tmp_b, patlen) do { apr_bucket_split(b, offset); tmp_b = APR_BUCKET_NEXT(b); apr_bucket_split(tmp_b, patlen); b = APR_BUCKET_NEXT(tmp_b); apr_bucket_delete(tmp_b); } while (0)
static apr_status_t do_pattmatch(ap_filter_t *f, apr_bucket *inb,
apr_bucket_brigade *mybb,
apr_pool_t *pool) {
int i;
int force_quick = 0;
ap_regmatch_t regm[AP_MAX_REG_MATCH];
apr_size_t bytes;
apr_size_t len;
const char *buff;
struct ap_varbuf vb;
apr_bucket *b;
apr_bucket *tmp_b;
subst_dir_conf *cfg =
(subst_dir_conf *) ap_get_module_config(f->r->per_dir_config,
&substitute_module);
subst_pattern_t *script;
APR_BRIGADE_INSERT_TAIL(mybb, inb);
ap_varbuf_init(pool, &vb, 0);
script = (subst_pattern_t *) cfg->patterns->elts;
if (cfg->patterns->nelts == 1) {
force_quick = 1;
}
for (i = 0; i < cfg->patterns->nelts; i++) {
for (b = APR_BRIGADE_FIRST(mybb);
b != APR_BRIGADE_SENTINEL(mybb);
b = APR_BUCKET_NEXT(b)) {
if (APR_BUCKET_IS_METADATA(b)) {
continue;
}
if (apr_bucket_read(b, &buff, &bytes, APR_BLOCK_READ)
== APR_SUCCESS) {
int have_match = 0;
vb.strlen = 0;
if (script->pattern) {
const char *repl;
apr_size_t space_left = cfg->max_line_length;
apr_size_t repl_len = strlen(script->replacement);
while ((repl = apr_strmatch(script->pattern, buff, bytes))) {
have_match = 1;
len = (apr_size_t) (repl - buff);
if (script->flatten && !force_quick) {
if (vb.strlen + len + repl_len > cfg->max_line_length)
return APR_ENOMEM;
ap_varbuf_strmemcat(&vb, buff, len);
ap_varbuf_strmemcat(&vb, script->replacement, repl_len);
} else {
if (space_left < len + repl_len)
return APR_ENOMEM;
space_left -= len + repl_len;
SEDRMPATBCKT(b, len, tmp_b, script->patlen);
tmp_b = apr_bucket_transient_create(script->replacement,
script->replen,
f->r->connection->bucket_alloc);
APR_BUCKET_INSERT_BEFORE(b, tmp_b);
}
len += script->patlen;
bytes -= len;
buff += len;
}
if (have_match) {
if (script->flatten && !force_quick) {
char *copy = ap_varbuf_pdup(pool, &vb, NULL, 0,
buff, bytes, &len);
tmp_b = apr_bucket_pool_create(copy, len, pool,
f->r->connection->bucket_alloc);
APR_BUCKET_INSERT_BEFORE(b, tmp_b);
apr_bucket_delete(b);
b = tmp_b;
} else {
if (space_left < b->length)
return APR_ENOMEM;
}
}
} else if (script->regexp) {
int left = bytes;
const char *pos = buff;
char *repl;
apr_size_t space_left = cfg->max_line_length;
while (!ap_regexec_len(script->regexp, pos, left,
AP_MAX_REG_MATCH, regm, 0)) {
apr_status_t rv;
have_match = 1;
if (script->flatten && !force_quick) {
if (vb.strlen + regm[0].rm_so >= cfg->max_line_length)
return APR_ENOMEM;
if (regm[0].rm_so > 0)
ap_varbuf_strmemcat(&vb, pos, regm[0].rm_so);
rv = ap_varbuf_regsub(&vb, script->replacement, pos,
AP_MAX_REG_MATCH, regm,
cfg->max_line_length - vb.strlen);
if (rv != APR_SUCCESS)
return rv;
} else {
apr_size_t repl_len;
if (space_left <= regm[0].rm_so)
return APR_ENOMEM;
space_left -= regm[0].rm_so;
rv = ap_pregsub_ex(pool, &repl,
script->replacement, pos,
AP_MAX_REG_MATCH, regm,
space_left);
if (rv != APR_SUCCESS)
return rv;
repl_len = strlen(repl);
space_left -= repl_len;
len = (apr_size_t) (regm[0].rm_eo - regm[0].rm_so);
SEDRMPATBCKT(b, regm[0].rm_so, tmp_b, len);
tmp_b = apr_bucket_transient_create(repl, repl_len,
f->r->connection->bucket_alloc);
APR_BUCKET_INSERT_BEFORE(b, tmp_b);
}
pos += regm[0].rm_eo;
left -= regm[0].rm_eo;
}
if (have_match && script->flatten && !force_quick) {
char *copy;
copy = ap_varbuf_pdup(pool, &vb, NULL, 0, pos, left,
&len);
tmp_b = apr_bucket_pool_create(copy, len, pool,
f->r->connection->bucket_alloc);
APR_BUCKET_INSERT_BEFORE(b, tmp_b);
apr_bucket_delete(b);
b = tmp_b;
}
} else {
ap_assert(0);
continue;
}
}
}
script++;
}
ap_varbuf_free(&vb);
return APR_SUCCESS;
}
static apr_status_t substitute_filter(ap_filter_t *f, apr_bucket_brigade *bb) {
apr_size_t bytes;
apr_size_t len;
apr_size_t fbytes;
const char *buff;
const char *nl = NULL;
char *bflat;
apr_bucket *b;
apr_bucket *tmp_b;
apr_bucket_brigade *tmp_bb = NULL;
apr_status_t rv;
subst_dir_conf *cfg =
(subst_dir_conf *) ap_get_module_config(f->r->per_dir_config,
&substitute_module);
substitute_module_ctx *ctx = f->ctx;
if (!ctx) {
f->ctx = ctx = apr_pcalloc(f->r->pool, sizeof(*ctx));
ctx->linebb = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
ctx->linesbb = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
ctx->pattbb = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
ctx->passbb = apr_brigade_create(f->r->pool, f->c->bucket_alloc);
apr_pool_create(&(ctx->tpool), f->r->pool);
apr_table_unset(f->r->headers_out, "Content-Length");
}
if (APR_BRIGADE_EMPTY(bb))
return APR_SUCCESS;
while ((b = APR_BRIGADE_FIRST(bb)) && (b != APR_BRIGADE_SENTINEL(bb))) {
if (APR_BUCKET_IS_EOS(b)) {
if (!APR_BRIGADE_EMPTY(ctx->linebb)) {
rv = apr_brigade_pflatten(ctx->linebb, &bflat,
&fbytes, ctx->tpool);
if (rv != APR_SUCCESS)
goto err;
if (fbytes > cfg->max_line_length) {
rv = APR_ENOMEM;
goto err;
}
tmp_b = apr_bucket_transient_create(bflat, fbytes,
f->r->connection->bucket_alloc);
rv = do_pattmatch(f, tmp_b, ctx->pattbb, ctx->tpool);
if (rv != APR_SUCCESS)
goto err;
APR_BRIGADE_CONCAT(ctx->passbb, ctx->pattbb);
apr_brigade_cleanup(ctx->linebb);
}
APR_BUCKET_REMOVE(b);
APR_BRIGADE_INSERT_TAIL(ctx->passbb, b);
}
else if (APR_BUCKET_IS_METADATA(b)) {
APR_BUCKET_REMOVE(b);
APR_BRIGADE_INSERT_TAIL(ctx->passbb, b);
} else {
rv = apr_bucket_read(b, &buff, &bytes, APR_BLOCK_READ);
if (rv != APR_SUCCESS || bytes == 0) {
apr_bucket_delete(b);
} else {
int num = 0;
while (bytes > 0) {
nl = memchr(buff, '\n', bytes);
if (nl) {
len = (apr_size_t) (nl - buff) + 1;
apr_bucket_split(b, len);
bytes -= len;
buff += len;
tmp_b = APR_BUCKET_NEXT(b);
APR_BUCKET_REMOVE(b);
if (!APR_BRIGADE_EMPTY(ctx->linebb)) {
APR_BRIGADE_INSERT_TAIL(ctx->linebb, b);
rv = apr_brigade_pflatten(ctx->linebb, &bflat,
&fbytes, ctx->tpool);
if (rv != APR_SUCCESS)
goto err;
if (fbytes > cfg->max_line_length) {
rv = APR_ENOMEM;
goto err;
}
b = apr_bucket_transient_create(bflat, fbytes,
f->r->connection->bucket_alloc);
apr_brigade_cleanup(ctx->linebb);
}
rv = do_pattmatch(f, b, ctx->pattbb, ctx->tpool);
if (rv != APR_SUCCESS)
goto err;
for (b = APR_BRIGADE_FIRST(ctx->pattbb);
b != APR_BRIGADE_SENTINEL(ctx->pattbb);
b = APR_BUCKET_NEXT(b)) {
num++;
}
APR_BRIGADE_CONCAT(ctx->passbb, ctx->pattbb);
if (num > AP_MAX_BUCKETS) {
b = apr_bucket_flush_create(
f->r->connection->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(ctx->passbb, b);
rv = ap_pass_brigade(f->next, ctx->passbb);
apr_brigade_cleanup(ctx->passbb);
num = 0;
apr_pool_clear(ctx->tpool);
if (rv != APR_SUCCESS)
goto err;
}
b = tmp_b;
} else {
APR_BUCKET_REMOVE(b);
APR_BRIGADE_INSERT_TAIL(ctx->linebb, b);
bytes = 0;
}
}
}
}
if (!APR_BRIGADE_EMPTY(ctx->passbb)) {
rv = ap_pass_brigade(f->next, ctx->passbb);
apr_brigade_cleanup(ctx->passbb);
if (rv != APR_SUCCESS)
goto err;
}
apr_pool_clear(ctx->tpool);
}
if (!APR_BRIGADE_EMPTY(ctx->linebb)) {
ap_save_brigade(f, &(ctx->linesbb), &(ctx->linebb), f->r->pool);
tmp_bb = ctx->linebb;
ctx->linebb = ctx->linesbb;
ctx->linesbb = tmp_bb;
}
return APR_SUCCESS;
err:
if (rv == APR_ENOMEM)
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r, APLOGNO(01328) "Line too long, URI %s",
f->r->uri);
apr_pool_clear(ctx->tpool);
return rv;
}
static const char *set_pattern(cmd_parms *cmd, void *cfg, const char *line) {
char *from = NULL;
char *to = NULL;
char *flags = NULL;
char *ourline;
char delim;
subst_pattern_t *nscript;
int is_pattern = 0;
int ignore_case = 0;
int flatten = 1;
ap_regex_t *r = NULL;
if (apr_tolower(*line) != 's') {
return "Bad Substitute format, must be an s/// pattern";
}
ourline = apr_pstrdup(cmd->pool, line);
delim = *++ourline;
if (delim)
from = ++ourline;
if (from) {
if (*ourline != delim) {
while (*++ourline && *ourline != delim);
}
if (*ourline) {
*ourline = '\0';
to = ++ourline;
}
}
if (to) {
if (*ourline != delim) {
while (*++ourline && *ourline != delim);
}
if (*ourline) {
*ourline = '\0';
flags = ++ourline;
}
}
if (!delim || !from || !*from || !to) {
return "Bad Substitute format, must be a complete s/// pattern";
}
if (flags) {
while (*flags) {
delim = apr_tolower(*flags);
if (delim == 'i')
ignore_case = 1;
else if (delim == 'n')
is_pattern = 1;
else if (delim == 'f')
flatten = 1;
else if (delim == 'q')
flatten = 0;
else
return "Bad Substitute flag, only s///[infq] are supported";
flags++;
}
}
if (!is_pattern) {
r = ap_pregcomp(cmd->pool, from, AP_REG_EXTENDED |
(ignore_case ? AP_REG_ICASE : 0));
if (!r)
return "Substitute could not compile regex";
}
nscript = apr_array_push(((subst_dir_conf *) cfg)->patterns);
nscript->pattern = NULL;
nscript->regexp = NULL;
nscript->replacement = NULL;
nscript->patlen = 0;
if (is_pattern) {
nscript->patlen = strlen(from);
nscript->pattern = apr_strmatch_precompile(cmd->pool, from,
!ignore_case);
} else {
nscript->regexp = r;
}
nscript->replacement = to;
nscript->replen = strlen(to);
nscript->flatten = flatten;
return NULL;
}
#define KBYTE 1024
#define MBYTE 1048576
#define GBYTE 1073741824
static const char *set_max_line_length(cmd_parms *cmd, void *cfg, const char *arg) {
subst_dir_conf *dcfg = (subst_dir_conf *)cfg;
apr_off_t max;
char *end;
apr_status_t rv;
rv = apr_strtoff(&max, arg, &end, 10);
if (rv == APR_SUCCESS) {
if ((*end == 'K' || *end == 'k') && !end[1]) {
max *= KBYTE;
} else if ((*end == 'M' || *end == 'm') && !end[1]) {
max *= MBYTE;
} else if ((*end == 'G' || *end == 'g') && !end[1]) {
max *= GBYTE;
} else if (*end &&
((*end != 'B' && *end != 'b') || end[1])) {
rv = APR_EGENERAL;
}
}
if (rv != APR_SUCCESS || max < 0) {
return "SubstituteMaxLineLength must be a non-negative integer optionally "
"suffixed with 'b', 'k', 'm' or 'g'.";
}
dcfg->max_line_length = (apr_size_t)max;
dcfg->max_line_length_set = 1;
return NULL;
}
#define PROTO_FLAGS AP_FILTER_PROTO_CHANGE|AP_FILTER_PROTO_CHANGE_LENGTH
static void register_hooks(apr_pool_t *pool) {
ap_register_output_filter(substitute_filter_name, substitute_filter,
NULL, AP_FTYPE_RESOURCE);
}
static const command_rec substitute_cmds[] = {
AP_INIT_TAKE1("Substitute", set_pattern, NULL, OR_FILEINFO,
"Pattern to filter the response content (s/foo/bar/[inf])"),
AP_INIT_TAKE1("SubstituteMaxLineLength", set_max_line_length, NULL, OR_FILEINFO,
"Maximum line length"),
AP_INIT_FLAG("SubstituteInheritBefore", ap_set_flag_slot,
(void *)APR_OFFSETOF(subst_dir_conf, inherit_before), OR_FILEINFO,
"Apply inherited patterns before those of the current context"),
{NULL}
};
AP_DECLARE_MODULE(substitute) = {
STANDARD20_MODULE_STUFF,
create_substitute_dcfg,
merge_substitute_dcfg,
NULL,
NULL,
substitute_cmds,
register_hooks
};