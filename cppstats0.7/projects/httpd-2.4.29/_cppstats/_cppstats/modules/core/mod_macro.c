#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "apr.h"
#include "apr_strings.h"
#include "apr_hash.h"
#undef MOD_MACRO_DEBUG
#if defined(debug)
#undef debug
#endif
#if defined(MOD_MACRO_DEBUG)
#define debug(stmt) stmt
#else
#define debug(stmt)
#endif
module AP_MODULE_DECLARE_DATA macro_module;
typedef struct {
char *name;
apr_array_header_t *arguments;
apr_array_header_t *contents;
char *location;
} ap_macro_t;
#define BEGIN_MACRO "<Macro"
#define END_MACRO "</Macro>"
#define USE_MACRO "Use"
#define UNDEF_MACRO "UndefMacro"
static apr_hash_t *ap_macros = NULL;
#define empty_string_p(p) (!(p) || *(p) == '\0')
#define trim(line) while (*(line) == ' ' || *(line) == '\t') (line)++
static apr_array_header_t *get_arguments(apr_pool_t * pool, const char *line) {
apr_array_header_t *args = apr_array_make(pool, 1, sizeof(char *));
trim(line);
while (*line) {
char *arg = ap_getword_conf(pool, &line);
char **new = apr_array_push(args);
*new = arg;
trim(line);
}
return args;
}
static void warn_if_non_blank(const char * what,
char * ptr,
ap_configfile_t * cfg) {
char * p;
for (p=ptr; *p; p++) {
if (*p == '#')
break;
if (*p != ' ' && *p != '\t') {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, APLOGNO(02989)
"%s on line %d of %s: %s",
what, cfg->line_number, cfg->name, ptr);
break;
}
}
}
static char *get_lines_till_end_token(apr_pool_t * pool,
ap_configfile_t * config_file,
const char *end_token,
const char *begin_token,
const char *where,
apr_array_header_t ** plines) {
apr_array_header_t *lines = apr_array_make(pool, 1, sizeof(char *));
char line[MAX_STRING_LEN];
int macro_nesting = 1, any_nesting = 1;
int line_number_start = config_file->line_number;
while (!ap_cfg_getline(line, MAX_STRING_LEN, config_file)) {
char *ptr = line;
char *first, **new;
if (*line == '#')
continue;
first = ap_getword_conf_nc(pool, &ptr);
if (first) {
if (!strncmp(first, "</", 2)) {
any_nesting--;
if (any_nesting < 0) {
ap_log_error(APLOG_MARK, APLOG_WARNING,
0, NULL, APLOGNO(02793)
"bad (negative) nesting on line %d of %s",
config_file->line_number - line_number_start,
where);
}
} else if (!strncmp(first, "<", 1)) {
any_nesting++;
}
if (!strcasecmp(first, end_token)) {
char * endp = (char *) ap_strrchr_c(line, '>');
if (endp == NULL) {
return "end directive missing closing '>'";
}
warn_if_non_blank(
APLOGNO(02794) "non blank chars found after directive closing",
endp+1, config_file);
macro_nesting--;
if (!macro_nesting) {
if (any_nesting) {
ap_log_error(APLOG_MARK,
APLOG_WARNING, 0, NULL, APLOGNO(02795)
"bad cumulated nesting (%+d) in %s",
any_nesting, where);
}
*plines = lines;
return NULL;
}
} else if (begin_token && !strcasecmp(first, begin_token)) {
macro_nesting++;
}
}
new = apr_array_push(lines);
*new = apr_psprintf(pool, "%s" APR_EOL_STR, line);
}
return apr_psprintf(pool, "expected token not found: %s", end_token);
}
#define ESCAPE_ARG '@'
#define ARG_PREFIX "$%@"
#define ARG_CONTENT "abcdefghijklmnopqrstuvwxyz" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "0123456789_" ARG_PREFIX
static int looks_like_an_argument(const char *word) {
return ap_strchr(ARG_PREFIX, *word) != 0;
}
static const char *check_macro_arguments(apr_pool_t * pool,
const ap_macro_t * macro) {
char **tab = (char **) macro->arguments->elts;
int nelts = macro->arguments->nelts;
int i;
for (i = 0; i < nelts; i++) {
size_t ltabi = strlen(tab[i]);
int j;
if (ltabi == 0) {
return apr_psprintf(pool,
"macro '%s' (%s): empty argument #%d name",
macro->name, macro->location, i + 1);
} else if (!looks_like_an_argument(tab[i])) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, APLOGNO(02796)
"macro '%s' (%s) "
"argument name '%s' (#%d) without expected prefix, "
"better prefix argument names with one of '%s'.",
macro->name, macro->location,
tab[i], i + 1, ARG_PREFIX);
}
for (j = i + 1; j < nelts; j++) {
size_t ltabj = strlen(tab[j]);
if (!strcmp(tab[i], tab[j])) {
return apr_psprintf(pool,
"argument name conflict in macro '%s' (%s): "
"argument '%s': #%d and #%d, "
"change argument names!",
macro->name, macro->location,
tab[i], i + 1, j + 1);
}
if (ltabi && ltabj &&
!strncmp(tab[i], tab[j], ltabi < ltabj ? ltabi : ltabj)) {
ap_log_error(APLOG_MARK, APLOG_WARNING,
0, NULL, APLOGNO(02797)
"macro '%s' (%s): "
"argument name prefix conflict (%s #%d and %s #%d), "
"be careful about your macro definition!",
macro->name, macro->location,
tab[i], i + 1, tab[j], j + 1);
}
}
}
return NULL;
}
static void check_macro_use_arguments(const char *where,
const apr_array_header_t * array) {
char **tab = (char **) array->elts;
int i;
for (i = 0; i < array->nelts; i++) {
if (empty_string_p(tab[i])) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, APLOGNO(02798)
"%s: empty argument #%d", where, i + 1);
}
}
}
#define DELIM '"'
#define ESCAPE '\\'
static int number_of_escapes(const char delim, const char *str) {
int nesc = 0;
const char *s = str;
while (*s) {
if (*s == ESCAPE || *s == delim)
nesc++;
s++;
}
debug(fprintf(stderr, "escapes: %d ---%s---\n", nesc, str));
return nesc;
}
static char *substitute(char *buf,
const int bufsize,
const char *name,
const char *replacement, const int do_esc) {
int lbuf = strlen(buf),
lname = strlen(name),
lrepl = strlen(replacement),
lsubs = lrepl +
(do_esc ? (2 + number_of_escapes(DELIM, replacement)) : 0),
shift = lsubs - lname, size = lbuf + shift, i, j;
ap_assert(!strncmp(buf, name, lname));
if (!strcmp(name, replacement))
return NULL;
debug(fprintf(stderr,
"substitute(%s,%s,%s,%d,sh=%d,lbuf=%d,lrepl=%d,lsubs=%d)\n",
buf, name, replacement, do_esc, shift, lbuf, lrepl, lsubs));
if (size >= bufsize) {
return "cannot substitute, buffer size too small";
}
if (shift != 0) {
memmove(buf + lname + shift, buf + lname, lbuf - lname + 1);
}
j = 0;
if (do_esc)
buf[j++] = DELIM;
for (i = 0; i < lrepl; i++, j++) {
if (do_esc && (replacement[i] == DELIM || replacement[i] == ESCAPE))
buf[j++] = ESCAPE;
buf[j] = replacement[i];
}
if (do_esc)
buf[j++] = DELIM;
return NULL;
}
static char *next_substitution(const char *buf,
const apr_array_header_t * args, int *whichone) {
char *chosen = NULL, **tab = (char **) args->elts;
size_t lchosen = 0;
int i;
for (i = 0; i < args->nelts; i++) {
char *found = ap_strstr((char *) buf, tab[i]);
size_t lfound = strlen(tab[i]);
if (found && (!chosen || found < chosen ||
(found == chosen && lchosen < lfound))) {
chosen = found;
lchosen = lfound;
*whichone = i;
}
}
return chosen;
}
static const char *substitute_macro_args(
char *buf,
int bufsize,
const ap_macro_t * macro,
const apr_array_header_t * replacements,
apr_array_header_t * used) {
char *ptr = buf,
**atab = (char **) macro->arguments->elts,
**rtab = (char **) replacements->elts;
int whichone = -1;
if (used) {
ap_assert(used->nalloc >= replacements->nelts);
}
debug(fprintf(stderr, "1#%s", buf));
while ((ptr = next_substitution(ptr, macro->arguments, &whichone))) {
const char *errmsg = substitute(ptr, buf - ptr + bufsize,
atab[whichone], rtab[whichone],
atab[whichone][0] == ESCAPE_ARG);
if (errmsg) {
return errmsg;
}
ptr += strlen(rtab[whichone]);
if (used) {
used->elts[whichone] = 1;
}
}
debug(fprintf(stderr, "2#%s", buf));
return NULL;
}
static const char *process_content(apr_pool_t * pool,
const ap_macro_t * macro,
const apr_array_header_t * replacements,
apr_array_header_t * used,
apr_array_header_t ** result) {
apr_array_header_t *contents = macro->contents;
char line[MAX_STRING_LEN];
int i;
if (result) {
*result = apr_array_make(pool, contents->nelts, sizeof(char *));
}
for (i = 0; i < contents->nelts; i++) {
const char *errmsg;
strncpy(line, ((char **) contents->elts)[i], MAX_STRING_LEN - 1);
errmsg = substitute_macro_args(line, MAX_STRING_LEN,
macro, replacements, used);
if (errmsg) {
return apr_psprintf(pool,
"while processing line %d of macro '%s' (%s) %s",
i + 1, macro->name, macro->location, errmsg);
}
if (result) {
char **new = apr_array_push(*result);
*new = apr_pstrdup(pool, line);
}
}
return NULL;
}
static const char *check_macro_contents(apr_pool_t * pool,
const ap_macro_t * macro) {
int nelts = macro->arguments->nelts;
char **names = (char **) macro->arguments->elts;
apr_array_header_t *used;
int i;
const char *errmsg;
if (macro->contents->nelts == 0) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, APLOGNO(02799)
"macro '%s' (%s): empty contents!",
macro->name, macro->location);
return NULL;
}
used = apr_array_make(pool, nelts, sizeof(char));
for (i = 0; i < nelts; i++) {
used->elts[i] = 0;
}
errmsg = process_content(pool, macro, macro->arguments, used, NULL);
if (errmsg) {
return errmsg;
}
for (i = 0; i < nelts; i++) {
if (!used->elts[i]) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, APLOGNO(02800)
"macro '%s' (%s): argument '%s' (#%d) never used",
macro->name, macro->location, names[i], i + 1);
}
}
return NULL;
}
typedef struct {
int index;
int char_index;
int length;
apr_array_header_t *contents;
ap_configfile_t *next;
ap_configfile_t **upper;
} array_contents_t;
static int next_one(array_contents_t * ml) {
if (ml->next) {
ap_assert(ml->upper);
*(ml->upper) = ml->next;
return 1;
}
return 0;
}
static apr_status_t array_getch(char *ch, void *param) {
array_contents_t *ml = (array_contents_t *) param;
char **tab = (char **) ml->contents->elts;
while (ml->char_index >= ml->length) {
if (ml->index >= ml->contents->nelts) {
if (ml->next && ml->next->getch && next_one(ml)) {
apr_status_t rc = ml->next->getch(ch, ml->next->param);
if (*ch==LF)
ml->next->line_number++;
return rc;
}
return APR_EOF;
}
ml->index++;
ml->char_index = 0;
ml->length = ml->index >= ml->contents->nelts ?
0 : strlen(tab[ml->index]);
}
*ch = tab[ml->index][ml->char_index++];
return APR_SUCCESS;
}
static apr_status_t array_getstr(void *buf, size_t bufsize, void *param) {
array_contents_t *ml = (array_contents_t *) param;
char *buffer = (char *) buf;
char next = '\0';
size_t i = 0;
apr_status_t rc = APR_SUCCESS;
while (i < bufsize - 1 && next != LF &&
((rc = array_getch(&next, param)) == APR_SUCCESS)) {
buffer[i++] = next;
}
if (rc == APR_EOF) {
if (next_one(ml)) {
ap_assert(ml->next->getstr);
ml->next->line_number++;
return ml->next->getstr(buf, bufsize, ml->next->param);
}
return APR_EOF;
}
buffer[i] = '\0';
return APR_SUCCESS;
}
static apr_status_t array_close(void *param) {
array_contents_t *ml = (array_contents_t *) param;
ml->index = ml->contents->nelts;
ml->char_index = ml->length;
return APR_SUCCESS;
}
static ap_configfile_t *make_array_config(apr_pool_t * pool,
apr_array_header_t * contents,
const char *where,
ap_configfile_t * cfg,
ap_configfile_t ** upper) {
array_contents_t *ls =
(array_contents_t *) apr_palloc(pool, sizeof(array_contents_t));
ap_assert(ls!=NULL);
ls->index = 0;
ls->char_index = 0;
ls->contents = contents;
ls->length = ls->contents->nelts < 1 ?
0 : strlen(((char **) ls->contents->elts)[0]);
ls->next = cfg;
ls->upper = upper;
return ap_pcfg_open_custom(pool, where, (void *) ls,
array_getch, array_getstr, array_close);
}
static const char *macro_section(cmd_parms * cmd,
void *dummy, const char *arg) {
apr_pool_t *pool;
char *endp, *name, *where;
const char *errmsg;
ap_macro_t *macro;
debug(fprintf(stderr, "macro_section: arg='%s'\n", arg));
if (ap_macros == NULL)
ap_macros = apr_hash_make(cmd->temp_pool);
ap_assert(ap_macros != NULL);
pool = apr_hash_pool_get(ap_macros);
endp = (char *) ap_strrchr_c(arg, '>');
if (endp == NULL) {
return BEGIN_MACRO "> directive missing closing '>'";
}
if (endp == arg) {
return BEGIN_MACRO " macro definition: empty name";
}
warn_if_non_blank(APLOGNO(02801) "non blank chars found after "
BEGIN_MACRO " closing '>'",
endp+1, cmd->config_file);
*endp = '\0';
name = ap_getword_conf(pool, &arg);
if (empty_string_p(name)) {
return BEGIN_MACRO " macro definition: name not found";
}
ap_str_tolower(name);
macro = apr_hash_get(ap_macros, name, APR_HASH_KEY_STRING);
if (macro != NULL) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, APLOGNO(02802)
"macro '%s' multiply defined: "
"%s, redefined on line %d of \"%s\"",
macro->name, macro->location,
cmd->config_file->line_number, cmd->config_file->name);
} else {
macro = (ap_macro_t *) apr_palloc(pool, sizeof(ap_macro_t));
macro->name = name;
}
debug(fprintf(stderr, "macro_section: name=%s\n", name));
macro->location = apr_psprintf(pool,
"defined on line %d of \"%s\"",
cmd->config_file->line_number,
cmd->config_file->name);
debug(fprintf(stderr, "macro_section: location=%s\n", macro->location));
where =
apr_psprintf(pool, "macro '%s' (%s)", macro->name, macro->location);
if (looks_like_an_argument(name)) {
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, APLOGNO(02803)
"%s better prefix a macro name with any of '%s'",
where, ARG_PREFIX);
}
macro->arguments = get_arguments(pool, arg);
errmsg = check_macro_arguments(cmd->temp_pool, macro);
if (errmsg) {
return errmsg;
}
errmsg = get_lines_till_end_token(pool, cmd->config_file,
END_MACRO, BEGIN_MACRO,
where, &macro->contents);
if (errmsg) {
return apr_psprintf(cmd->temp_pool,
"%s" APR_EOL_STR "\tcontents error: %s",
where, errmsg);
}
errmsg = check_macro_contents(cmd->temp_pool, macro);
if (errmsg) {
return apr_psprintf(cmd->temp_pool,
"%s" APR_EOL_STR "\tcontents checking error: %s",
where, errmsg);
}
apr_hash_set(ap_macros, name, APR_HASH_KEY_STRING, macro);
return NULL;
}
static const char *use_macro(cmd_parms * cmd, void *dummy, const char *arg) {
char *name, *recursion, *where;
const char *errmsg;
ap_macro_t *macro;
apr_array_header_t *replacements;
apr_array_header_t *contents;
debug(fprintf(stderr, "use_macro -%s-\n", arg));
if (ap_macros == NULL) {
return "no macro defined before " USE_MACRO;
}
name = ap_getword_conf(cmd->temp_pool, &arg);
ap_str_tolower(name);
if (empty_string_p(name)) {
return "no macro name specified with " USE_MACRO;
}
macro = apr_hash_get(ap_macros, name, APR_HASH_KEY_STRING);
if (!macro) {
return apr_psprintf(cmd->temp_pool, "macro '%s' undefined", name);
}
recursion =
apr_pstrcat(cmd->temp_pool, "macro '", macro->name, "'", NULL);
if (ap_strstr((char *) cmd->config_file->name, recursion)) {
return apr_psprintf(cmd->temp_pool,
"recursive use of macro '%s' is invalid",
macro->name);
}
replacements = get_arguments(cmd->temp_pool, arg);
if (macro->arguments->nelts != replacements->nelts) {
return apr_psprintf(cmd->temp_pool,
"macro '%s' (%s) used "
"with %d arguments instead of %d",
macro->name, macro->location,
replacements->nelts, macro->arguments->nelts);
}
where = apr_psprintf(cmd->temp_pool,
"macro '%s' (%s) used on line %d of \"%s\"",
macro->name, macro->location,
cmd->config_file->line_number,
cmd->config_file->name);
check_macro_use_arguments(where, replacements);
errmsg = process_content(cmd->temp_pool, macro, replacements,
NULL, &contents);
if (errmsg) {
return apr_psprintf(cmd->temp_pool,
"%s error while substituting: %s",
where, errmsg);
}
cmd->config_file = make_array_config(cmd->temp_pool, contents, where,
cmd->config_file, &cmd->config_file);
return NULL;
}
static const char *undef_macro(cmd_parms * cmd, void *dummy, const char *arg) {
char *name;
ap_macro_t *macro;
if (ap_macros == NULL) {
return "no macro defined before " UNDEF_MACRO;
}
if (empty_string_p(arg)) {
return "no macro name specified with " UNDEF_MACRO;
}
name = apr_pstrdup(cmd->temp_pool, arg);
ap_str_tolower(name);
macro = apr_hash_get(ap_macros, name, APR_HASH_KEY_STRING);
if (macro == NULL) {
return apr_psprintf(cmd->temp_pool,
"cannot remove undefined macro '%s'", name);
}
apr_hash_set(ap_macros, name, APR_HASH_KEY_STRING, NULL);
return NULL;
}
static int macro_pre_config(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp) {
ap_macros = NULL;
return OK;
}
static const command_rec macro_cmds[] = {
AP_INIT_RAW_ARGS(BEGIN_MACRO, macro_section, NULL, EXEC_ON_READ | OR_ALL,
"Beginning of a macro definition section."),
AP_INIT_RAW_ARGS(USE_MACRO, use_macro, NULL, EXEC_ON_READ | OR_ALL,
"Use of a macro."),
AP_INIT_TAKE1(UNDEF_MACRO, undef_macro, NULL, EXEC_ON_READ | OR_ALL,
"Remove a macro definition."),
{NULL}
};
static void macro_hooks(apr_pool_t *p) {
ap_hook_pre_config(macro_pre_config, NULL, NULL, APR_HOOK_MIDDLE);
}
AP_DECLARE_MODULE(macro) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
macro_cmds,
macro_hooks
};