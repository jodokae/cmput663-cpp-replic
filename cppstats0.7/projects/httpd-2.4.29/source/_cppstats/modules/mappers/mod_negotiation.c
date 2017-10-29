#include "apr.h"
#include "apr_strings.h"
#include "apr_file_io.h"
#include "apr_lib.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_core.h"
#include "http_log.h"
#include "util_script.h"
#define MAP_FILE_MAGIC_TYPE "application/x-type-map"
typedef struct {
int forcelangpriority;
apr_array_header_t *language_priority;
} neg_dir_config;
#define FLP_UNDEF 0
#define FLP_NONE 1
#define FLP_PREFER 2
#define FLP_FALLBACK 4
#define FLP_DEFAULT FLP_PREFER
#define DISCARD_ALL_ENCODINGS 1
#define DISCARD_ALL_BUT_HTML 2
module AP_MODULE_DECLARE_DATA negotiation_module;
static void *create_neg_dir_config(apr_pool_t *p, char *dummy) {
neg_dir_config *new = (neg_dir_config *) apr_palloc(p,
sizeof(neg_dir_config));
new->forcelangpriority = FLP_UNDEF;
new->language_priority = NULL;
return new;
}
static void *merge_neg_dir_configs(apr_pool_t *p, void *basev, void *addv) {
neg_dir_config *base = (neg_dir_config *) basev;
neg_dir_config *add = (neg_dir_config *) addv;
neg_dir_config *new = (neg_dir_config *) apr_palloc(p,
sizeof(neg_dir_config));
new->forcelangpriority = (add->forcelangpriority != FLP_UNDEF)
? add->forcelangpriority
: base->forcelangpriority;
new->language_priority = add->language_priority
? add->language_priority
: base->language_priority;
return new;
}
static const char *set_language_priority(cmd_parms *cmd, void *n_,
const char *lang) {
neg_dir_config *n = n_;
const char **langp;
if (!n->language_priority)
n->language_priority = apr_array_make(cmd->pool, 4, sizeof(char *));
langp = (const char **) apr_array_push(n->language_priority);
*langp = lang;
return NULL;
}
static const char *set_force_priority(cmd_parms *cmd, void *n_, const char *w) {
neg_dir_config *n = n_;
if (!strcasecmp(w, "None")) {
if (n->forcelangpriority & ~FLP_NONE) {
return "Cannot combine ForceLanguagePriority options with None";
}
n->forcelangpriority = FLP_NONE;
} else if (!strcasecmp(w, "Prefer")) {
if (n->forcelangpriority & FLP_NONE) {
return "Cannot combine ForceLanguagePriority options None and "
"Prefer";
}
n->forcelangpriority |= FLP_PREFER;
} else if (!strcasecmp(w, "Fallback")) {
if (n->forcelangpriority & FLP_NONE) {
return "Cannot combine ForceLanguagePriority options None and "
"Fallback";
}
n->forcelangpriority |= FLP_FALLBACK;
} else {
return apr_pstrcat(cmd->pool, "Invalid ForceLanguagePriority option ",
w, NULL);
}
return NULL;
}
static const char *cache_negotiated_docs(cmd_parms *cmd, void *dummy,
int arg) {
ap_set_module_config(cmd->server->module_config, &negotiation_module,
(arg ? "Cache" : NULL));
return NULL;
}
static int do_cache_negotiated_docs(server_rec *s) {
return (ap_get_module_config(s->module_config,
&negotiation_module) != NULL);
}
static const command_rec negotiation_cmds[] = {
AP_INIT_FLAG("CacheNegotiatedDocs", cache_negotiated_docs, NULL, RSRC_CONF,
"Either 'on' or 'off' (default)"),
AP_INIT_ITERATE("LanguagePriority", set_language_priority, NULL,
OR_FILEINFO,
"space-delimited list of MIME language abbreviations"),
AP_INIT_ITERATE("ForceLanguagePriority", set_force_priority, NULL,
OR_FILEINFO,
"Force LanguagePriority elections, either None, or "
"Fallback and/or Prefer"),
{NULL}
};
typedef struct accept_rec {
char *name;
float quality;
float level;
char *charset;
} accept_rec;
typedef struct var_rec {
request_rec *sub_req;
const char *mime_type;
const char *file_name;
apr_off_t body;
const char *content_encoding;
apr_array_header_t *content_languages;
const char *content_charset;
const char *description;
float lang_quality;
float encoding_quality;
float charset_quality;
float mime_type_quality;
float source_quality;
float level;
apr_off_t bytes;
int lang_index;
int is_pseudo_html;
float level_matched;
int mime_stars;
int definite;
} var_rec;
typedef struct {
apr_pool_t *pool;
request_rec *r;
neg_dir_config *conf;
char *dir_name;
int accept_q;
float default_lang_quality;
apr_array_header_t *accepts;
apr_array_header_t *accept_encodings;
apr_array_header_t *accept_charsets;
apr_array_header_t *accept_langs;
apr_array_header_t *avail_vars;
int count_multiviews_variants;
int is_transparent;
int dont_fiddle_headers;
int ua_supports_trans;
int send_alternates;
int may_choose;
int use_rvsa;
} negotiation_state;
static void clean_var_rec(var_rec *mime_info) {
mime_info->sub_req = NULL;
mime_info->mime_type = "";
mime_info->file_name = "";
mime_info->body = 0;
mime_info->content_encoding = NULL;
mime_info->content_languages = NULL;
mime_info->content_charset = "";
mime_info->description = "";
mime_info->is_pseudo_html = 0;
mime_info->level = 0.0f;
mime_info->level_matched = 0.0f;
mime_info->bytes = -1;
mime_info->lang_index = -1;
mime_info->mime_stars = 0;
mime_info->definite = 1;
mime_info->charset_quality = 1.0f;
mime_info->encoding_quality = 1.0f;
mime_info->lang_quality = 1.0f;
mime_info->mime_type_quality = 1.0f;
mime_info->source_quality = 0.0f;
}
static void set_mime_fields(var_rec *var, accept_rec *mime_info) {
var->mime_type = mime_info->name;
var->source_quality = mime_info->quality;
var->level = mime_info->level;
var->content_charset = mime_info->charset;
var->is_pseudo_html = (!strcmp(var->mime_type, "text/html")
|| !strcmp(var->mime_type, INCLUDES_MAGIC_TYPE)
|| !strcmp(var->mime_type, INCLUDES_MAGIC_TYPE3));
}
static void set_vlist_validator(request_rec *r, request_rec *vlistr) {
ap_update_mtime(vlistr, vlistr->finfo.mtime);
r->vlist_validator = ap_make_etag(vlistr, 0);
}
static float atoq(const char *string) {
if (!string || !*string) {
return 1.0f;
}
while (apr_isspace(*string)) {
++string;
}
if (*string != '.' && *string++ != '0') {
return 1.0f;
}
if (*string == '.') {
int i = 0;
if (*++string >= '0' && *string <= '9') {
i += (*string - '0') * 100;
if (*++string >= '0' && *string <= '9') {
i += (*string - '0') * 10;
if (*++string > '0' && *string <= '9') {
i += (*string - '0');
}
}
}
return (float)i / 1000.0f;
}
return 0.0f;
}
static const char *get_entry(apr_pool_t *p, accept_rec *result,
const char *accept_line) {
result->quality = 1.0f;
result->level = 0.0f;
result->charset = "";
result->name = ap_get_token(p, &accept_line, 0);
ap_str_tolower(result->name);
if (!strcmp(result->name, "text/html") && (result->level == 0.0)) {
result->level = 2.0f;
} else if (!strcmp(result->name, INCLUDES_MAGIC_TYPE)) {
result->level = 2.0f;
} else if (!strcmp(result->name, INCLUDES_MAGIC_TYPE3)) {
result->level = 3.0f;
}
while (*accept_line == ';') {
char *parm;
char *cp;
char *end;
++accept_line;
parm = ap_get_token(p, &accept_line, 1);
for (cp = parm; (*cp && !apr_isspace(*cp) && *cp != '='); ++cp) {
*cp = apr_tolower(*cp);
}
if (!*cp) {
continue;
}
*cp++ = '\0';
while (apr_isspace(*cp) || *cp == '=') {
++cp;
}
if (*cp == '"') {
++cp;
for (end = cp;
(*end && *end != '\n' && *end != '\r' && *end != '\"');
end++);
} else {
for (end = cp; (*end && !apr_isspace(*end)); end++);
}
if (*end) {
*end = '\0';
}
ap_str_tolower(cp);
if (parm[0] == 'q'
&& (parm[1] == '\0' || (parm[1] == 's' && parm[2] == '\0'))) {
result->quality = atoq(cp);
} else if (parm[0] == 'l' && !strcmp(&parm[1], "evel")) {
result->level = (float)atoi(cp);
} else if (!strcmp(parm, "charset")) {
result->charset = cp;
}
}
if (*accept_line == ',') {
++accept_line;
}
return accept_line;
}
static apr_array_header_t *do_header_line(apr_pool_t *p,
const char *accept_line) {
apr_array_header_t *accept_recs;
if (!accept_line) {
return NULL;
}
accept_recs = apr_array_make(p, 40, sizeof(accept_rec));
while (*accept_line) {
accept_rec *new = (accept_rec *) apr_array_push(accept_recs);
accept_line = get_entry(p, new, accept_line);
}
return accept_recs;
}
static apr_array_header_t *do_languages_line(apr_pool_t *p,
const char **lang_line) {
apr_array_header_t *lang_recs = apr_array_make(p, 2, sizeof(char *));
if (!lang_line) {
return lang_recs;
}
while (**lang_line) {
char **new = (char **) apr_array_push(lang_recs);
*new = ap_get_token(p, lang_line, 0);
ap_str_tolower(*new);
if (**lang_line == ',' || **lang_line == ';') {
++(*lang_line);
}
}
return lang_recs;
}
static negotiation_state *parse_accept_headers(request_rec *r) {
negotiation_state *new =
(negotiation_state *) apr_pcalloc(r->pool, sizeof(negotiation_state));
accept_rec *elts;
apr_table_t *hdrs = r->headers_in;
int i;
new->pool = r->pool;
new->r = r;
new->conf = (neg_dir_config *)ap_get_module_config(r->per_dir_config,
&negotiation_module);
new->dir_name = ap_make_dirstr_parent(r->pool, r->filename);
new->accepts = do_header_line(r->pool, apr_table_get(hdrs, "Accept"));
if (new->accepts) {
elts = (accept_rec *) new->accepts->elts;
for (i = 0; i < new->accepts->nelts; ++i) {
if (elts[i].quality < 1.0) {
new->accept_q = 1;
}
}
}
new->accept_encodings =
do_header_line(r->pool, apr_table_get(hdrs, "Accept-Encoding"));
new->accept_langs =
do_header_line(r->pool, apr_table_get(hdrs, "Accept-Language"));
new->accept_charsets =
do_header_line(r->pool, apr_table_get(hdrs, "Accept-Charset"));
new->avail_vars = apr_array_make(r->pool, 40, sizeof(var_rec));
return new;
}
static void parse_negotiate_header(request_rec *r, negotiation_state *neg) {
const char *negotiate = apr_table_get(r->headers_in, "Negotiate");
char *tok;
neg->ua_supports_trans = 0;
neg->send_alternates = 0;
neg->may_choose = 1;
neg->use_rvsa = 0;
neg->dont_fiddle_headers = 0;
if (!negotiate)
return;
if (strcmp(negotiate, "trans") == 0) {
const char *ua = apr_table_get(r->headers_in, "User-Agent");
if (ua && (strncmp(ua, "Lynx", 4) == 0))
return;
}
neg->may_choose = 0;
while ((tok = ap_get_list_item(neg->pool, &negotiate)) != NULL) {
if (strcmp(tok, "trans") == 0 ||
strcmp(tok, "vlist") == 0 ||
strcmp(tok, "guess-small") == 0 ||
apr_isdigit(tok[0]) ||
strcmp(tok, "*") == 0) {
neg->ua_supports_trans = 1;
neg->send_alternates = 1;
if (strcmp(tok, "1.0") == 0) {
neg->may_choose = 1;
neg->use_rvsa = 1;
neg->dont_fiddle_headers = 1;
} else if (tok[0] == '*') {
neg->may_choose = 1;
neg->dont_fiddle_headers = 1;
}
}
}
#if defined(NEG_DEBUG)
ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL, APLOGNO(00680)
"dont_fiddle_headers=%d use_rvsa=%d ua_supports_trans=%d "
"send_alternates=%d, may_choose=%d",
neg->dont_fiddle_headers, neg->use_rvsa,
neg->ua_supports_trans, neg->send_alternates, neg->may_choose);
#endif
}
static void maybe_add_default_accepts(negotiation_state *neg,
int prefer_scripts) {
accept_rec *new_accept;
if (!neg->accepts) {
neg->accepts = apr_array_make(neg->pool, 4, sizeof(accept_rec));
new_accept = (accept_rec *) apr_array_push(neg->accepts);
new_accept->name = "*/*";
new_accept->quality = 1.0f;
new_accept->level = 0.0f;
}
new_accept = (accept_rec *) apr_array_push(neg->accepts);
new_accept->name = CGI_MAGIC_TYPE;
if (neg->use_rvsa) {
new_accept->quality = 0;
} else {
new_accept->quality = prefer_scripts ? 2.0f : 0.001f;
}
new_accept->level = 0.0f;
}
enum header_state {
header_eof, header_seen, header_sep
};
static enum header_state get_header_line(char *buffer, int len, apr_file_t *map) {
char *buf_end = buffer + len;
char *cp;
char c;
do {
if (apr_file_gets(buffer, MAX_STRING_LEN, map) != APR_SUCCESS) {
return header_eof;
}
} while (buffer[0] == '#');
for (cp = buffer; apr_isspace(*cp); ++cp) {
continue;
}
if (*cp == '\0') {
return header_sep;
}
cp += strlen(cp);
if (!strncasecmp(buffer, "Body:", 5))
return header_seen;
while (apr_file_getc(&c, map) != APR_EOF) {
if (c == '#') {
while (apr_file_getc(&c, map) != APR_EOF && c != '\n') {
continue;
}
} else if (apr_isspace(c)) {
while (c != '\n' && apr_isspace(c)) {
if (apr_file_getc(&c, map) != APR_SUCCESS) {
break;
}
}
apr_file_ungetc(c, map);
if (c == '\n') {
return header_seen;
}
while ( cp < buf_end - 2
&& (apr_file_getc(&c, map)) != APR_EOF
&& c != '\n') {
*cp++ = c;
}
*cp++ = '\n';
*cp = '\0';
} else {
apr_file_ungetc(c, map);
return header_seen;
}
}
return header_seen;
}
static apr_off_t get_body(char *buffer, apr_size_t *len, const char *tag,
apr_file_t *map) {
char *endbody;
apr_size_t bodylen;
apr_off_t pos;
--*len;
if (apr_file_read(map, buffer, len) != APR_SUCCESS) {
return -1;
}
buffer[*len] = '\0';
endbody = ap_strstr(buffer, tag);
if (!endbody) {
return -1;
}
bodylen = endbody - buffer;
endbody += strlen(tag);
while (*endbody) {
if (*endbody == '\n') {
++endbody;
break;
}
++endbody;
}
pos = -(apr_off_t)(*len - (endbody - buffer));
if (apr_file_seek(map, APR_CUR, &pos) != APR_SUCCESS) {
return -1;
}
*len = bodylen;
return pos - (endbody - buffer);
}
static void strip_paren_comments(char *hdr) {
while (*hdr) {
if (*hdr == '"') {
hdr = strchr(hdr, '"');
if (hdr == NULL) {
return;
}
++hdr;
} else if (*hdr == '(') {
while (*hdr && *hdr != ')') {
*hdr++ = ' ';
}
if (*hdr) {
*hdr++ = ' ';
}
} else {
++hdr;
}
}
}
static char *lcase_header_name_return_body(char *header, request_rec *r) {
char *cp = header;
for ( ; *cp && *cp != ':' ; ++cp) {
*cp = apr_tolower(*cp);
}
if (!*cp) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00681)
"Syntax error in type map, no ':' in %s for header %s",
r->filename, header);
return NULL;
}
do {
++cp;
} while (apr_isspace(*cp));
if (!*cp) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00682)
"Syntax error in type map --- no header body: %s for %s",
r->filename, header);
return NULL;
}
return cp;
}
static int read_type_map(apr_file_t **map, negotiation_state *neg,
request_rec *rr) {
request_rec *r = neg->r;
apr_file_t *map_ = NULL;
apr_status_t status;
char buffer[MAX_STRING_LEN];
enum header_state hstate;
struct var_rec mime_info;
int has_content;
if (!map)
map = &map_;
neg->count_multiviews_variants = 0;
if ((status = apr_file_open(map, rr->filename, APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, neg->pool)) != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(00683)
"cannot access type map file: %s", rr->filename);
if (APR_STATUS_IS_ENOTDIR(status) || APR_STATUS_IS_ENOENT(status)) {
return HTTP_NOT_FOUND;
} else {
return HTTP_FORBIDDEN;
}
}
clean_var_rec(&mime_info);
has_content = 0;
do {
hstate = get_header_line(buffer, MAX_STRING_LEN, *map);
if (hstate == header_seen) {
char *body1 = lcase_header_name_return_body(buffer, neg->r);
const char *body;
if (body1 == NULL) {
return HTTP_INTERNAL_SERVER_ERROR;
}
strip_paren_comments(body1);
body = body1;
if (!strncmp(buffer, "uri:", 4)) {
mime_info.file_name = ap_get_token(neg->pool, &body, 0);
} else if (!strncmp(buffer, "content-type:", 13)) {
struct accept_rec accept_info;
get_entry(neg->pool, &accept_info, body);
set_mime_fields(&mime_info, &accept_info);
has_content = 1;
} else if (!strncmp(buffer, "content-length:", 15)) {
char *errp;
apr_off_t number;
body1 = ap_get_token(neg->pool, &body, 0);
if (apr_strtoff(&number, body1, &errp, 10) != APR_SUCCESS
|| *errp || number < 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00684)
"Parse error in type map, Content-Length: "
"'%s' in %s is invalid.",
body1, r->filename);
break;
}
mime_info.bytes = number;
has_content = 1;
} else if (!strncmp(buffer, "content-language:", 17)) {
mime_info.content_languages = do_languages_line(neg->pool,
&body);
has_content = 1;
} else if (!strncmp(buffer, "content-encoding:", 17)) {
mime_info.content_encoding = ap_get_token(neg->pool, &body, 0);
has_content = 1;
} else if (!strncmp(buffer, "description:", 12)) {
char *desc = apr_pstrdup(neg->pool, body);
char *cp;
for (cp = desc; *cp; ++cp) {
if (*cp=='\n') *cp=' ';
}
if (cp>desc) *(cp-1)=0;
mime_info.description = desc;
} else if (!strncmp(buffer, "body:", 5)) {
char *tag = apr_pstrdup(neg->pool, body);
char *eol = strchr(tag, '\0');
apr_size_t len = MAX_STRING_LEN;
while (--eol >= tag && apr_isspace(*eol))
*eol = '\0';
if ((mime_info.body = get_body(buffer, &len, tag, *map)) < 0) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00685)
"Syntax error in type map, no end tag '%s'"
"found in %s for Body: content.",
tag, r->filename);
break;
}
mime_info.bytes = len;
mime_info.file_name = apr_filepath_name_get(rr->filename);
}
} else {
if (*mime_info.file_name && has_content) {
void *new_var = apr_array_push(neg->avail_vars);
memcpy(new_var, (void *) &mime_info, sizeof(var_rec));
}
clean_var_rec(&mime_info);
has_content = 0;
}
} while (hstate != header_eof);
if (map_)
apr_file_close(map_);
set_vlist_validator(r, rr);
return OK;
}
static int variantsortf(var_rec *a, var_rec *b) {
if (a->source_quality < b->source_quality)
return 1;
if (a->source_quality > b->source_quality)
return -1;
return strcmp(a->file_name, b->file_name);
}
static int read_types_multi(negotiation_state *neg) {
request_rec *r = neg->r;
char *filp;
int prefix_len;
apr_dir_t *dirp;
apr_finfo_t dirent;
apr_status_t status;
struct var_rec mime_info;
struct accept_rec accept_info;
void *new_var;
int anymatch = 0;
clean_var_rec(&mime_info);
if (r->proxyreq || !r->filename
|| !ap_os_is_path_absolute(neg->pool, r->filename)) {
return DECLINED;
}
if (!(filp = strrchr(r->filename, '/'))) {
return DECLINED;
}
++filp;
prefix_len = strlen(filp);
if ((status = apr_dir_open(&dirp, neg->dir_name,
neg->pool)) != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, APLOGNO(00686)
"cannot read directory for multi: %s", neg->dir_name);
return HTTP_FORBIDDEN;
}
while (apr_dir_read(&dirent, APR_FINFO_DIRENT, dirp) == APR_SUCCESS) {
apr_array_header_t *exception_list;
request_rec *sub_req;
#if defined(CASE_BLIND_FILESYSTEM)
if (strncasecmp(dirent.name, filp, prefix_len)) {
#else
if (strncmp(dirent.name, filp, prefix_len)) {
#endif
continue;
}
if (dirent.name[prefix_len] != '.') {
continue;
}
if ((dirent.valid & APR_FINFO_TYPE) && (dirent.filetype == APR_DIR))
continue;
anymatch = 1;
sub_req = ap_sub_req_lookup_dirent(&dirent, r, AP_SUBREQ_MERGE_ARGS,
NULL);
if (sub_req->finfo.filetype != APR_REG) {
continue;
}
if (sub_req->handler && !sub_req->content_type) {
ap_set_content_type(sub_req, CGI_MAGIC_TYPE);
}
exception_list =
(apr_array_header_t *)apr_table_get(sub_req->notes,
"ap-mime-exceptions-list");
if (!exception_list) {
ap_destroy_sub_req(sub_req);
continue;
}
{
int nexcept = exception_list->nelts;
char **cur_except = (char**)exception_list->elts;
char *segstart = filp, *segend, saveend;
while (*segstart && nexcept) {
if (!(segend = strchr(segstart, '.')))
segend = strchr(segstart, '\0');
saveend = *segend;
*segend = '\0';
#if defined(CASE_BLIND_FILESYSTEM)
if (strcasecmp(segstart, *cur_except) == 0) {
#else
if (strcmp(segstart, *cur_except) == 0) {
#endif
--nexcept;
++cur_except;
}
if (!saveend)
break;
*segend = saveend;
segstart = segend + 1;
}
if (nexcept) {
ap_destroy_sub_req(sub_req);
continue;
}
}
if (sub_req->status != HTTP_OK || (!sub_req->content_type)) {
ap_destroy_sub_req(sub_req);
continue;
}
if (((sub_req->content_type) &&
!strcmp(sub_req->content_type, MAP_FILE_MAGIC_TYPE)) ||
((sub_req->handler) &&
!strcmp(sub_req->handler, "type-map"))) {
apr_dir_close(dirp);
neg->avail_vars->nelts = 0;
if (sub_req->status != HTTP_OK) {
return sub_req->status;
}
return read_type_map(NULL, neg, sub_req);
}
mime_info.sub_req = sub_req;
mime_info.file_name = apr_pstrdup(neg->pool, dirent.name);
if (sub_req->content_encoding) {
mime_info.content_encoding = sub_req->content_encoding;
}
if (sub_req->content_languages) {
mime_info.content_languages = sub_req->content_languages;
}
get_entry(neg->pool, &accept_info, sub_req->content_type);
set_mime_fields(&mime_info, &accept_info);
new_var = apr_array_push(neg->avail_vars);
memcpy(new_var, (void *) &mime_info, sizeof(var_rec));
neg->count_multiviews_variants++;
clean_var_rec(&mime_info);
}
apr_dir_close(dirp);
if (anymatch && !neg->avail_vars->nelts) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00687)
"Negotiation: discovered file(s) matching request: %s"
" (None could be negotiated).",
r->filename);
return HTTP_NOT_FOUND;
}
set_vlist_validator(r, r);
qsort((void *) neg->avail_vars->elts, neg->avail_vars->nelts,
sizeof(var_rec), (int (*)(const void *, const void *)) variantsortf);
return OK;
}
static int mime_match(accept_rec *accept_r, var_rec *avail) {
const char *accept_type = accept_r->name;
const char *avail_type = avail->mime_type;
int len = strlen(accept_type);
if ((len == 1 && accept_type[0] == '*')
|| (len == 3 && !strncmp(accept_type, "*/*", 3))) {
if (avail->mime_stars < 1) {
avail->mime_stars = 1;
}
return 1;
} else if (len > 2 && accept_type[len - 2] == '/'
&& accept_type[len - 1] == '*'
&& !strncmp(accept_type, avail_type, len - 2)
&& avail_type[len - 2] == '/') {
if (avail->mime_stars < 2) {
avail->mime_stars = 2;
}
return 1;
} else if (!strcmp(accept_type, avail_type)
|| (!strcmp(accept_type, "text/html")
&& (!strcmp(avail_type, INCLUDES_MAGIC_TYPE)
|| !strcmp(avail_type, INCLUDES_MAGIC_TYPE3)))) {
if (accept_r->level >= avail->level) {
avail->level_matched = avail->level;
avail->mime_stars = 3;
return 1;
}
}
return OK;
}
static int level_cmp(var_rec *var1, var_rec *var2) {
if (var1->is_pseudo_html && !var2->is_pseudo_html) {
return 0;
}
if (!var1->is_pseudo_html && strcmp(var1->mime_type, var2->mime_type)) {
return 0;
}
if (var1->level_matched > var2->level_matched) {
return 1;
}
if (var1->level_matched < var2->level_matched) {
return -1;
}
if (var1->level < var2->level) {
return 1;
}
if (var1->level > var2->level) {
return -1;
}
return 0;
}
static int find_lang_index(apr_array_header_t *accept_langs, char *lang) {
const char **alang;
int i;
if (!lang || !accept_langs) {
return -1;
}
alang = (const char **) accept_langs->elts;
for (i = 0; i < accept_langs->nelts; ++i) {
if (!strncmp(lang, *alang, strlen(*alang))) {
return i;
}
alang += (accept_langs->elt_size / sizeof(char*));
}
return -1;
}
static void set_default_lang_quality(negotiation_state *neg) {
var_rec *avail_recs = (var_rec *) neg->avail_vars->elts;
int j;
if (!neg->dont_fiddle_headers) {
for (j = 0; j < neg->avail_vars->nelts; ++j) {
var_rec *variant = &avail_recs[j];
if (variant->content_languages &&
variant->content_languages->nelts) {
neg->default_lang_quality = 0.0001f;
return;
}
}
}
neg->default_lang_quality = 1.0f;
}
static void set_language_quality(negotiation_state *neg, var_rec *variant) {
int forcepriority = neg->conf->forcelangpriority;
if (forcepriority == FLP_UNDEF) {
forcepriority = FLP_DEFAULT;
}
if (!variant->content_languages || !variant->content_languages->nelts) {
if (!neg->dont_fiddle_headers) {
variant->lang_quality = neg->default_lang_quality;
}
if (!neg->accept_langs) {
return;
}
return;
} else {
if (!neg->accept_langs) {
variant->definite = 0;
} else {
accept_rec *accs = (accept_rec *) neg->accept_langs->elts;
accept_rec *best = NULL, *star = NULL;
accept_rec *bestthistag;
char *lang, *p;
float fiddle_q = 0.0f;
int any_match_on_star = 0;
int i, j;
apr_size_t alen, longest_lang_range_len;
for (j = 0; j < variant->content_languages->nelts; ++j) {
p = NULL;
bestthistag = NULL;
longest_lang_range_len = 0;
lang = ((char **) (variant->content_languages->elts))[j];
for (i = 0; i < neg->accept_langs->nelts; ++i) {
if (!strcmp(accs[i].name, "*")) {
if (!star) {
star = &accs[i];
}
continue;
}
alen = strlen(accs[i].name);
if ((strlen(lang) >= alen) &&
!strncmp(lang, accs[i].name, alen) &&
((lang[alen] == 0) || (lang[alen] == '-')) ) {
if (alen > longest_lang_range_len) {
longest_lang_range_len = alen;
bestthistag = &accs[i];
}
}
if (!bestthistag && !neg->dont_fiddle_headers) {
if ((p = strchr(accs[i].name, '-'))) {
int plen = p - accs[i].name;
if (!strncmp(lang, accs[i].name, plen)) {
fiddle_q = 0.001f;
}
}
}
}
if (!best ||
(bestthistag && bestthistag->quality > best->quality)) {
best = bestthistag;
}
if (!bestthistag && star) {
any_match_on_star = 1;
}
}
if ( any_match_on_star &&
((best && star->quality > best->quality) ||
(!best)) ) {
best = star;
variant->definite = 0;
}
variant->lang_quality = best ? best->quality : fiddle_q;
}
}
if (((forcepriority & FLP_PREFER)
&& (variant->lang_index < 0))
|| ((forcepriority & FLP_FALLBACK)
&& !variant->lang_quality)) {
int bestidx = -1;
int j;
for (j = 0; j < variant->content_languages->nelts; ++j) {
char *lang = ((char **) (variant->content_languages->elts))[j];
int idx;
idx = find_lang_index(neg->conf->language_priority, lang);
if ((idx >= 0) && ((bestidx == -1) || (idx < bestidx))) {
bestidx = idx;
}
}
if (bestidx >= 0) {
if (variant->lang_quality) {
if (forcepriority & FLP_PREFER) {
variant->lang_index = bestidx;
}
} else {
if (forcepriority & FLP_FALLBACK) {
variant->lang_index = bestidx;
variant->lang_quality = .0001f;
variant->definite = 0;
}
}
}
}
}
static apr_off_t find_content_length(negotiation_state *neg, var_rec *variant) {
apr_finfo_t statb;
if (variant->bytes < 0) {
if ( variant->sub_req
&& (variant->sub_req->finfo.valid & APR_FINFO_SIZE)) {
variant->bytes = variant->sub_req->finfo.size;
} else {
char *fullname = ap_make_full_path(neg->pool, neg->dir_name,
variant->file_name);
if (apr_stat(&statb, fullname,
APR_FINFO_SIZE, neg->pool) == APR_SUCCESS) {
variant->bytes = statb.size;
}
}
}
return variant->bytes;
}
static void set_accept_quality(negotiation_state *neg, var_rec *variant) {
int i;
accept_rec *accept_recs;
float q = 0.0f;
int q_definite = 1;
if (!neg->accepts) {
if (variant->mime_type && *variant->mime_type)
variant->definite = 0;
return;
}
accept_recs = (accept_rec *) neg->accepts->elts;
for (i = 0; i < neg->accepts->nelts; ++i) {
accept_rec *type = &accept_recs[i];
int prev_mime_stars;
prev_mime_stars = variant->mime_stars;
if (!mime_match(type, variant)) {
continue;
} else {
if (prev_mime_stars == variant->mime_stars) {
continue;
}
}
if (!neg->dont_fiddle_headers && !neg->accept_q &&
variant->mime_stars == 1) {
q = 0.01f;
} else if (!neg->dont_fiddle_headers && !neg->accept_q &&
variant->mime_stars == 2) {
q = 0.02f;
} else {
q = type->quality;
}
q_definite = (variant->mime_stars == 3);
}
variant->mime_type_quality = q;
variant->definite = variant->definite && q_definite;
}
static void set_charset_quality(negotiation_state *neg, var_rec *variant) {
int i;
accept_rec *accept_recs;
const char *charset = variant->content_charset;
accept_rec *star = NULL;
if (!neg->accept_charsets) {
if (charset && *charset)
variant->definite = 0;
return;
}
accept_recs = (accept_rec *) neg->accept_charsets->elts;
if (charset == NULL || !*charset) {
if (!(!strncmp(variant->mime_type, "text/", 5)
|| !strcmp(variant->mime_type, INCLUDES_MAGIC_TYPE)
|| !strcmp(variant->mime_type, INCLUDES_MAGIC_TYPE3)
))
return;
if (neg->dont_fiddle_headers)
return;
charset = "iso-8859-1";
}
for (i = 0; i < neg->accept_charsets->nelts; ++i) {
accept_rec *type = &accept_recs[i];
if (!strcmp(type->name, charset)) {
variant->charset_quality = type->quality;
return;
} else if (strcmp(type->name, "*") == 0) {
star = type;
}
}
if (star) {
variant->charset_quality = star->quality;
variant->definite = 0;
return;
}
if (strcmp(charset, "iso-8859-1") == 0) {
variant->charset_quality = 1.0f;
} else {
variant->charset_quality = 0.0f;
}
}
static int is_identity_encoding(const char *enc) {
return (!enc || !enc[0] || !strcmp(enc, "7bit") || !strcmp(enc, "8bit")
|| !strcmp(enc, "binary"));
}
static void set_encoding_quality(negotiation_state *neg, var_rec *variant) {
accept_rec *accept_recs;
const char *enc = variant->content_encoding;
accept_rec *star = NULL;
float value_if_not_found = 0.0f;
int i;
if (!neg->accept_encodings) {
if (!enc || is_identity_encoding(enc))
variant->encoding_quality = 1.0f;
else
variant->encoding_quality = 0.5f;
return;
}
if (!enc || is_identity_encoding(enc)) {
enc = "identity";
value_if_not_found = 0.0001f;
}
accept_recs = (accept_rec *) neg->accept_encodings->elts;
if (enc[0] == 'x' && enc[1] == '-') {
enc += 2;
}
for (i = 0; i < neg->accept_encodings->nelts; ++i) {
char *name = accept_recs[i].name;
if (name[0] == 'x' && name[1] == '-') {
name += 2;
}
if (!strcmp(name, enc)) {
variant->encoding_quality = accept_recs[i].quality;
return;
}
if (strcmp(name, "*") == 0) {
star = &accept_recs[i];
}
}
if (star) {
variant->encoding_quality = star->quality;
return;
}
variant->encoding_quality = value_if_not_found;
}
enum algorithm_results {
alg_choice = 1,
alg_list
};
static int is_variant_better_rvsa(negotiation_state *neg, var_rec *variant,
var_rec *best, float *p_bestq) {
float bestq = *p_bestq, q;
if (variant->encoding_quality == 0.0f)
return 0;
q = variant->mime_type_quality *
variant->source_quality *
variant->charset_quality *
variant->lang_quality;
#if defined(NEG_DEBUG)
ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL, APLOGNO(00688)
"Variant: file=%s type=%s lang=%s sourceq=%1.3f "
"mimeq=%1.3f langq=%1.3f charq=%1.3f encq=%1.3f "
"q=%1.5f definite=%d",
(variant->file_name ? variant->file_name : ""),
(variant->mime_type ? variant->mime_type : ""),
(variant->content_languages
? apr_array_pstrcat(neg->pool, variant->content_languages, ',')
: ""),
variant->source_quality,
variant->mime_type_quality,
variant->lang_quality,
variant->charset_quality,
variant->encoding_quality,
q,
variant->definite);
#endif
if (q <= 0.0f) {
return 0;
}
if (q > bestq) {
*p_bestq = q;
return 1;
}
if (q == bestq) {
if (variant->encoding_quality > best->encoding_quality) {
*p_bestq = q;
return 1;
}
}
return 0;
}
static int is_variant_better(negotiation_state *neg, var_rec *variant,
var_rec *best, float *p_bestq) {
float bestq = *p_bestq, q;
int levcmp;
#if defined(NEG_DEBUG)
ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL, APLOGNO(00689)
"Variant: file=%s type=%s lang=%s sourceq=%1.3f "
"mimeq=%1.3f langq=%1.3f langidx=%d charq=%1.3f encq=%1.3f ",
(variant->file_name ? variant->file_name : ""),
(variant->mime_type ? variant->mime_type : ""),
(variant->content_languages
? apr_array_pstrcat(neg->pool, variant->content_languages, ',')
: ""),
variant->source_quality,
variant->mime_type_quality,
variant->lang_quality,
variant->lang_index,
variant->charset_quality,
variant->encoding_quality);
#endif
if (variant->encoding_quality == 0.0f ||
variant->lang_quality == 0.0f ||
variant->source_quality == 0.0f ||
variant->charset_quality == 0.0f ||
variant->mime_type_quality == 0.0f) {
return 0;
}
q = variant->mime_type_quality * variant->source_quality;
if (q == 0.0 || q < bestq) {
return 0;
}
if (q > bestq || !best) {
*p_bestq = q;
return 1;
}
if (variant->lang_quality < best->lang_quality) {
return 0;
}
if (variant->lang_quality > best->lang_quality) {
*p_bestq = q;
return 1;
}
if (best->lang_index != -1 &&
(variant->lang_index == -1 || variant->lang_index > best->lang_index)) {
return 0;
}
if (variant->lang_index != -1 &&
(best->lang_index == -1 || variant->lang_index < best->lang_index)) {
*p_bestq = q;
return 1;
}
levcmp = level_cmp(variant, best);
if (levcmp == -1) {
return 0;
}
if (levcmp == 1) {
*p_bestq = q;
return 1;
}
if (variant->charset_quality < best->charset_quality) {
return 0;
}
if (variant->charset_quality > best->charset_quality ||
((variant->content_charset != NULL &&
*variant->content_charset != '\0' &&
strcmp(variant->content_charset, "iso-8859-1") != 0) &&
(best->content_charset == NULL ||
*best->content_charset == '\0' ||
strcmp(best->content_charset, "iso-8859-1") == 0))) {
*p_bestq = q;
return 1;
}
if (variant->encoding_quality < best->encoding_quality) {
return 0;
}
if (variant->encoding_quality > best->encoding_quality) {
*p_bestq = q;
return 1;
}
if (find_content_length(neg, variant) >= find_content_length(neg, best)) {
return 0;
}
*p_bestq = q;
return 1;
}
static int variant_has_language(var_rec *variant, const char *lang) {
if ( !lang
|| !variant->content_languages) {
return 0;
}
if (ap_array_str_contains(variant->content_languages, lang)) {
return 1;
}
return 0;
}
static int discard_variant_by_env(var_rec *variant, int discard) {
if ( is_identity_encoding(variant->content_encoding)
|| !strcmp(variant->content_encoding, "identity")) {
return 0;
}
return ( (discard == DISCARD_ALL_ENCODINGS)
|| (discard == DISCARD_ALL_BUT_HTML
&& (!variant->mime_type
|| strncmp(variant->mime_type, "text/html", 9))));
}
static int best_match(negotiation_state *neg, var_rec **pbest) {
int j;
var_rec *best;
float bestq = 0.0f;
enum algorithm_results algorithm_result;
int may_discard = 0;
var_rec *avail_recs = (var_rec *) neg->avail_vars->elts;
const char *preferred_language = apr_table_get(neg->r->subprocess_env,
"prefer-language");
if (apr_table_get(neg->r->subprocess_env, "no-gzip")) {
may_discard = DISCARD_ALL_ENCODINGS;
}
else {
const char *env_value = apr_table_get(neg->r->subprocess_env,
"gzip-only-text/html");
if (env_value && !strcmp(env_value, "1")) {
may_discard = DISCARD_ALL_BUT_HTML;
}
}
set_default_lang_quality(neg);
do {
best = NULL;
for (j = 0; j < neg->avail_vars->nelts; ++j) {
var_rec *variant = &avail_recs[j];
if ( may_discard
&& discard_variant_by_env(variant, may_discard)) {
continue;
}
if ( preferred_language
&& !variant_has_language(variant, preferred_language)) {
continue;
}
set_accept_quality(neg, variant);
if (preferred_language) {
variant->lang_quality = 1.0f;
variant->definite = 1;
} else {
set_language_quality(neg, variant);
}
set_encoding_quality(neg, variant);
set_charset_quality(neg, variant);
if (neg->may_choose) {
if (neg->use_rvsa) {
if (is_variant_better_rvsa(neg, variant, best, &bestq)) {
best = variant;
}
} else {
if (is_variant_better(neg, variant, best, &bestq)) {
best = variant;
}
}
}
}
if (neg->use_rvsa) {
algorithm_result = (best && best->definite) && (bestq > 0) ?
alg_choice : alg_list;
} else {
algorithm_result = bestq > 0 ? alg_choice : alg_list;
}
if (preferred_language && (!best || algorithm_result != alg_choice)) {
preferred_language = NULL;
continue;
}
break;
} while (1);
*pbest = best;
return algorithm_result;
}
static void set_neg_headers(request_rec *r, negotiation_state *neg,
int alg_result) {
apr_table_t *hdrs;
var_rec *avail_recs = (var_rec *) neg->avail_vars->elts;
const char *sample_type = NULL;
const char *sample_language = NULL;
const char *sample_encoding = NULL;
const char *sample_charset = NULL;
char *lang;
char *qstr;
apr_off_t len;
apr_array_header_t *arr;
int max_vlist_array = (neg->avail_vars->nelts * 21);
int first_variant = 1;
int vary_by_type = 0;
int vary_by_language = 0;
int vary_by_charset = 0;
int vary_by_encoding = 0;
int j;
if (neg->send_alternates && neg->avail_vars->nelts)
arr = apr_array_make(r->pool, max_vlist_array, sizeof(char *));
else
arr = NULL;
hdrs = r->err_headers_out;
for (j = 0; j < neg->avail_vars->nelts; ++j) {
var_rec *variant = &avail_recs[j];
if (variant->content_languages && variant->content_languages->nelts) {
lang = apr_array_pstrcat(r->pool, variant->content_languages, ',');
} else {
lang = NULL;
}
if (first_variant) {
sample_type = variant->mime_type;
sample_charset = variant->content_charset;
sample_language = lang;
sample_encoding = variant->content_encoding;
} else {
if (!vary_by_type &&
strcmp(sample_type ? sample_type : "",
variant->mime_type ? variant->mime_type : "")) {
vary_by_type = 1;
}
if (!vary_by_charset &&
strcmp(sample_charset ? sample_charset : "",
variant->content_charset ?
variant->content_charset : "")) {
vary_by_charset = 1;
}
if (!vary_by_language &&
strcmp(sample_language ? sample_language : "",
lang ? lang : "")) {
vary_by_language = 1;
}
if (!vary_by_encoding &&
strcmp(sample_encoding ? sample_encoding : "",
variant->content_encoding ?
variant->content_encoding : "")) {
vary_by_encoding = 1;
}
}
first_variant = 0;
if (!neg->send_alternates)
continue;
*((const char **) apr_array_push(arr)) = "{\"";
*((const char **) apr_array_push(arr)) = ap_escape_path_segment(r->pool, variant->file_name);
*((const char **) apr_array_push(arr)) = "\" ";
qstr = (char *) apr_palloc(r->pool, 6);
apr_snprintf(qstr, 6, "%1.3f", variant->source_quality);
if (qstr[4] == '0') {
qstr[4] = '\0';
if (qstr[3] == '0') {
qstr[3] = '\0';
if (qstr[2] == '0') {
qstr[1] = '\0';
}
}
}
*((const char **) apr_array_push(arr)) = qstr;
if (variant->mime_type && *variant->mime_type) {
*((const char **) apr_array_push(arr)) = " {type ";
*((const char **) apr_array_push(arr)) = variant->mime_type;
*((const char **) apr_array_push(arr)) = "}";
}
if (variant->content_charset && *variant->content_charset) {
*((const char **) apr_array_push(arr)) = " {charset ";
*((const char **) apr_array_push(arr)) = variant->content_charset;
*((const char **) apr_array_push(arr)) = "}";
}
if (lang) {
*((const char **) apr_array_push(arr)) = " {language ";
*((const char **) apr_array_push(arr)) = lang;
*((const char **) apr_array_push(arr)) = "}";
}
if (variant->content_encoding && *variant->content_encoding) {
*((const char **) apr_array_push(arr)) = " {encoding ";
*((const char **) apr_array_push(arr)) = variant->content_encoding;
*((const char **) apr_array_push(arr)) = "}";
}
if (!(variant->sub_req && variant->sub_req->handler)
&& (len = find_content_length(neg, variant)) >= 0) {
*((const char **) apr_array_push(arr)) = " {length ";
*((const char **) apr_array_push(arr)) = apr_off_t_toa(r->pool,
len);
*((const char **) apr_array_push(arr)) = "}";
}
*((const char **) apr_array_push(arr)) = "}";
*((const char **) apr_array_push(arr)) = ", ";
}
if (neg->send_alternates && neg->avail_vars->nelts) {
arr->nelts--;
apr_table_mergen(hdrs, "Alternates",
apr_array_pstrcat(r->pool, arr, '\0'));
}
if (neg->is_transparent || vary_by_type || vary_by_language ||
vary_by_charset || vary_by_encoding) {
apr_table_mergen(hdrs, "Vary", 2 + apr_pstrcat(r->pool,
neg->is_transparent ? ", negotiate" : "",
vary_by_type ? ", accept" : "",
vary_by_language ? ", accept-language" : "",
vary_by_charset ? ", accept-charset" : "",
vary_by_encoding ? ", accept-encoding" : "", NULL));
}
if (neg->is_transparent) {
apr_table_setn(hdrs, "TCN",
alg_result == alg_list ? "list" : "choice");
}
}
static char *make_variant_list(request_rec *r, negotiation_state *neg) {
apr_array_header_t *arr;
int i;
int max_vlist_array = (neg->avail_vars->nelts * 15) + 2;
arr = apr_array_make(r->pool, max_vlist_array, sizeof(char *));
*((const char **) apr_array_push(arr)) = "Available variants:\n<ul>\n";
for (i = 0; i < neg->avail_vars->nelts; ++i) {
var_rec *variant = &((var_rec *) neg->avail_vars->elts)[i];
const char *filename = variant->file_name ? variant->file_name : "";
apr_array_header_t *languages = variant->content_languages;
const char *description = variant->description
? variant->description
: "";
*((const char **) apr_array_push(arr)) = "<li><a href=\"";
*((const char **) apr_array_push(arr)) = ap_escape_path_segment(r->pool, filename);
*((const char **) apr_array_push(arr)) = "\">";
*((const char **) apr_array_push(arr)) = ap_escape_html(r->pool, filename);
*((const char **) apr_array_push(arr)) = "</a> ";
*((const char **) apr_array_push(arr)) = description;
if (variant->mime_type && *variant->mime_type) {
*((const char **) apr_array_push(arr)) = ", type ";
*((const char **) apr_array_push(arr)) = variant->mime_type;
}
if (languages && languages->nelts) {
*((const char **) apr_array_push(arr)) = ", language ";
*((const char **) apr_array_push(arr)) = apr_array_pstrcat(r->pool,
languages, ',');
}
if (variant->content_charset && *variant->content_charset) {
*((const char **) apr_array_push(arr)) = ", charset ";
*((const char **) apr_array_push(arr)) = variant->content_charset;
}
if (variant->content_encoding) {
*((const char **) apr_array_push(arr)) = ", encoding ";
*((const char **) apr_array_push(arr)) = variant->content_encoding;
}
*((const char **) apr_array_push(arr)) = "</li>\n";
}
*((const char **) apr_array_push(arr)) = "</ul>\n";
return apr_array_pstrcat(r->pool, arr, '\0');
}
static void store_variant_list(request_rec *r, negotiation_state *neg) {
if (r->main == NULL) {
apr_table_setn(r->notes, "variant-list", make_variant_list(r, neg));
} else {
apr_table_setn(r->main->notes, "variant-list",
make_variant_list(r->main, neg));
}
}
static int setup_choice_response(request_rec *r, negotiation_state *neg,
var_rec *variant) {
request_rec *sub_req;
const char *sub_vary;
if (!variant->sub_req) {
int status;
sub_req = ap_sub_req_lookup_file(variant->file_name, r, r->output_filters);
status = sub_req->status;
if (status != HTTP_OK &&
!apr_table_get(sub_req->err_headers_out, "TCN")) {
ap_destroy_sub_req(sub_req);
return status;
}
variant->sub_req = sub_req;
} else {
sub_req = variant->sub_req;
}
if (neg->is_transparent &&
apr_table_get(sub_req->err_headers_out, "TCN")) {
return HTTP_VARIANT_ALSO_VARIES;
}
if (sub_req->handler && strcmp(sub_req->handler, "type-map") == 0) {
return HTTP_VARIANT_ALSO_VARIES;
}
if ((sub_vary = apr_table_get(sub_req->err_headers_out, "Vary")) != NULL) {
apr_table_setn(r->err_headers_out, "Variant-Vary", sub_vary);
apr_table_setn(r->err_headers_out, "Vary", sub_vary);
apr_table_unset(sub_req->err_headers_out, "Vary");
}
apr_table_setn(r->err_headers_out, "Content-Location",
ap_escape_path_segment(r->pool, variant->file_name));
set_neg_headers(r, neg, alg_choice);
return 0;
}
static int do_negotiation(request_rec *r, negotiation_state *neg,
var_rec **bestp, int prefer_scripts) {
var_rec *avail_recs = (var_rec *) neg->avail_vars->elts;
int alg_result;
int res;
int j;
if (r->method_number == M_GET) {
neg->is_transparent = 1;
if (r->path_info && *r->path_info)
neg->is_transparent = 0;
for (j = 0; j < neg->avail_vars->nelts; ++j) {
var_rec *variant = &avail_recs[j];
if (ap_strchr_c(variant->file_name, '/'))
neg->is_transparent = 0;
if (variant->body) {
neg->is_transparent = 0;
}
}
}
if (neg->is_transparent) {
parse_negotiate_header(r, neg);
} else {
neg->may_choose = 1;
}
maybe_add_default_accepts(neg, prefer_scripts);
alg_result = best_match(neg, bestp);
if (alg_result == alg_list) {
neg->send_alternates = 1;
set_neg_headers(r, neg, alg_result);
store_variant_list(r, neg);
if (neg->is_transparent && neg->ua_supports_trans) {
return HTTP_MULTIPLE_CHOICES;
}
if (!*bestp) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00690)
"no acceptable variant: %s", r->filename);
return HTTP_NOT_ACCEPTABLE;
}
}
if (neg->is_transparent) {
if ((res = setup_choice_response(r, neg, *bestp)) != 0) {
return res;
}
} else {
set_neg_headers(r, neg, alg_result);
}
if ((!do_cache_negotiated_docs(r->server)
&& (r->proto_num < HTTP_VERSION(1,1)))
&& neg->count_multiviews_variants != 1) {
r->no_cache = 1;
}
return OK;
}
static int handle_map_file(request_rec *r) {
negotiation_state *neg;
apr_file_t *map;
var_rec *best;
int res;
char *udir;
const char *new_req;
if (strcmp(r->handler, MAP_FILE_MAGIC_TYPE) && strcmp(r->handler, "type-map")) {
return DECLINED;
}
neg = parse_accept_headers(r);
if ((res = read_type_map(&map, neg, r))) {
return res;
}
res = do_negotiation(r, neg, &best, 0);
if (res != 0) {
return res;
}
if (best->body) {
conn_rec *c = r->connection;
apr_bucket_brigade *bb;
apr_bucket *e;
ap_allow_standard_methods(r, REPLACE_ALLOW, M_GET, M_OPTIONS,
M_POST, -1);
if (r->method_number != M_GET && r->method_number != M_POST) {
return HTTP_METHOD_NOT_ALLOWED;
}
ap_set_accept_ranges(r);
ap_set_content_length(r, best->bytes);
if (best->mime_type && *best->mime_type) {
if (best->content_charset && *best->content_charset) {
ap_set_content_type(r, apr_pstrcat(r->pool,
best->mime_type,
"; charset=",
best->content_charset,
NULL));
} else {
ap_set_content_type(r, apr_pstrdup(r->pool, best->mime_type));
}
}
if (best->content_languages && best->content_languages->nelts) {
r->content_languages = apr_array_copy(r->pool,
best->content_languages);
}
if (best->content_encoding && *best->content_encoding) {
r->content_encoding = apr_pstrdup(r->pool,
best->content_encoding);
}
if ((res = ap_meets_conditions(r)) != OK) {
return res;
}
if ((res = ap_discard_request_body(r)) != OK) {
return res;
}
bb = apr_brigade_create(r->pool, c->bucket_alloc);
apr_brigade_insert_file(bb, map, best->body, best->bytes, r->pool);
e = apr_bucket_eos_create(c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, e);
return ap_pass_brigade_fchk(r, bb, NULL);
}
if (r->path_info && *r->path_info) {
r->uri[ap_find_path_info(r->uri, r->path_info)] = '\0';
}
udir = ap_make_dirstr_parent(r->pool, r->uri);
udir = ap_escape_uri(r->pool, udir);
if (r->args) {
if (r->path_info) {
new_req = apr_pstrcat(r->pool, udir, best->file_name,
r->path_info, "?", r->args, NULL);
} else {
new_req = apr_pstrcat(r->pool, udir, best->file_name,
"?", r->args, NULL);
}
} else {
new_req = apr_pstrcat(r->pool, udir, best->file_name,
r->path_info, NULL);
}
ap_internal_redirect(new_req, r);
return OK;
}
static int handle_multi(request_rec *r) {
negotiation_state *neg;
var_rec *best, *avail_recs;
request_rec *sub_req;
int res;
int j;
if (r->finfo.filetype != APR_NOFILE
|| !(ap_allow_options(r) & OPT_MULTI)) {
return DECLINED;
}
neg = parse_accept_headers(r);
if ((res = read_types_multi(neg))) {
return_from_multi:
avail_recs = (var_rec *) neg->avail_vars->elts;
for (j = 0; j < neg->avail_vars->nelts; ++j) {
var_rec *variant = &avail_recs[j];
if (variant->sub_req) {
ap_destroy_sub_req(variant->sub_req);
}
}
return res;
}
if (neg->avail_vars->nelts == 0) {
return DECLINED;
}
res = do_negotiation(r, neg, &best,
(r->method_number != M_GET) || r->args ||
(r->path_info && *r->path_info));
if (res != 0)
goto return_from_multi;
if (!(sub_req = best->sub_req)) {
sub_req = ap_sub_req_lookup_file(best->file_name, r, r->output_filters);
if (sub_req->status != HTTP_OK) {
res = sub_req->status;
ap_destroy_sub_req(sub_req);
goto return_from_multi;
}
}
if (sub_req->args == NULL) {
sub_req->args = r->args;
}
ap_internal_fast_redirect(sub_req, r);
r->mtime = 0;
avail_recs = (var_rec *) neg->avail_vars->elts;
for (j = 0; j < neg->avail_vars->nelts; ++j) {
var_rec *variant = &avail_recs[j];
if (variant != best && variant->sub_req) {
ap_destroy_sub_req(variant->sub_req);
}
}
return OK;
}
static int fix_encoding(request_rec *r) {
const char *enc = r->content_encoding;
char *x_enc = NULL;
apr_array_header_t *accept_encodings;
accept_rec *accept_recs;
int i;
if (!enc || !*enc) {
return DECLINED;
}
if (enc[0] == 'x' && enc[1] == '-') {
enc += 2;
}
if ((accept_encodings = do_header_line(r->pool,
apr_table_get(r->headers_in, "Accept-Encoding"))) == NULL) {
return DECLINED;
}
accept_recs = (accept_rec *) accept_encodings->elts;
for (i = 0; i < accept_encodings->nelts; ++i) {
char *name = accept_recs[i].name;
if (!strcmp(name, enc)) {
r->content_encoding = name;
return OK;
}
if (name[0] == 'x' && name[1] == '-' && !strcmp(name+2, enc)) {
x_enc = name;
}
}
if (x_enc) {
r->content_encoding = x_enc;
return OK;
}
return DECLINED;
}
static void register_hooks(apr_pool_t *p) {
ap_hook_fixups(fix_encoding,NULL,NULL,APR_HOOK_MIDDLE);
ap_hook_type_checker(handle_multi,NULL,NULL,APR_HOOK_FIRST);
ap_hook_handler(handle_map_file,NULL,NULL,APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(negotiation) = {
STANDARD20_MODULE_STUFF,
create_neg_dir_config,
merge_neg_dir_configs,
NULL,
NULL,
negotiation_cmds,
register_hooks
};