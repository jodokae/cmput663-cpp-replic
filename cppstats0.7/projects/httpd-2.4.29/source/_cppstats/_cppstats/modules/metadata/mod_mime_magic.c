#include "apr.h"
#include "apr_strings.h"
#include "apr_lib.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "util_script.h"
#if defined(HAVE_UTIME_H)
#include <utime.h>
#endif
#define MODNAME "mod_mime_magic"
#define MIME_MAGIC_DEBUG 0
#define MIME_BINARY_UNKNOWN "application/octet-stream"
#define MIME_TEXT_UNKNOWN "text/plain"
#define MAXMIMESTRING 256
#define HOWMANY 4096
#define SMALL_HOWMANY 1024
#define MAXDESC 50
#define MAXstring 64
struct magic {
struct magic *next;
int lineno;
short flag;
#define INDIR 1
#define UNSIGNED 2
short cont_level;
struct {
char type;
long offset;
} in;
long offset;
unsigned char reln;
char type;
char vallen;
#define BYTE 1
#define SHORT 2
#define LONG 4
#define STRING 5
#define DATE 6
#define BESHORT 7
#define BELONG 8
#define BEDATE 9
#define LESHORT 10
#define LELONG 11
#define LEDATE 12
union VALUETYPE {
unsigned char b;
unsigned short h;
unsigned long l;
char s[MAXstring];
unsigned char hs[2];
unsigned char hl[4];
} value;
unsigned long mask;
char nospflag;
char desc[MAXDESC];
};
#define RECORDSIZE 512
#define NAMSIZ 100
#define TUNMLEN 32
#define TGNMLEN 32
union record {
char charptr[RECORDSIZE];
struct header {
char name[NAMSIZ];
char mode[8];
char uid[8];
char gid[8];
char size[12];
char mtime[12];
char chksum[8];
char linkflag;
char linkname[NAMSIZ];
char magic[8];
char uname[TUNMLEN];
char gname[TGNMLEN];
char devmajor[8];
char devminor[8];
} header;
};
#define TMAGIC "ustar "
static int ascmagic(request_rec *, unsigned char *, apr_size_t);
static int is_tar(unsigned char *, apr_size_t);
static int softmagic(request_rec *, unsigned char *, apr_size_t);
static int tryit(request_rec *, unsigned char *, apr_size_t, int);
static int zmagic(request_rec *, unsigned char *, apr_size_t);
static int getvalue(server_rec *, struct magic *, char **);
static int hextoint(int);
static char *getstr(server_rec *, char *, char *, int, int *);
static int parse(server_rec *, apr_pool_t *p, char *, int);
static int match(request_rec *, unsigned char *, apr_size_t);
static int mget(request_rec *, union VALUETYPE *, unsigned char *,
struct magic *, apr_size_t);
static int mcheck(request_rec *, union VALUETYPE *, struct magic *);
static void mprint(request_rec *, union VALUETYPE *, struct magic *);
static int uncompress(request_rec *, int,
unsigned char **, apr_size_t);
static long from_oct(int, char *);
static int fsmagic(request_rec *r, const char *fn);
#define L_HTML 0
#define L_C 1
#define L_FORT 2
#define L_MAKE 3
#define L_PLI 4
#define L_MACH 5
#define L_ENG 6
#define L_PAS 7
#define L_MAIL 8
#define L_NEWS 9
static const char *types[] = {
"text/html",
"text/plain",
"text/plain",
"text/plain",
"text/plain",
"text/plain",
"text/plain",
"text/plain",
"message/rfc822",
"message/news",
"application/binary",
0
};
static const struct names {
const char *name;
short type;
} names[] = {
{
"<html>", L_HTML
},
{
"<HTML>", L_HTML
},
{
"<head>", L_HTML
},
{
"<HEAD>", L_HTML
},
{
"<title>", L_HTML
},
{
"<TITLE>", L_HTML
},
{
"<h1>", L_HTML
},
{
"<H1>", L_HTML
},
{
"<!--", L_HTML
},
{
"<!DOCTYPE HTML", L_HTML
},
{
"/*", L_C
},
{
"#include", L_C
},
{
"char", L_C
},
{
"The", L_ENG
},
{
"the", L_ENG
},
{
"double", L_C
},
{
"extern", L_C
},
{
"float", L_C
},
{
"real", L_C
},
{
"struct", L_C
},
{
"union", L_C
},
{
"CFLAGS", L_MAKE
},
{
"LDFLAGS", L_MAKE
},
{
"all:", L_MAKE
},
{
".PRECIOUS", L_MAKE
},
#if defined(NOTDEF)
{
"subroutine", L_FORT
},
{
"function", L_FORT
},
{
"block", L_FORT
},
{
"common", L_FORT
},
{
"dimension", L_FORT
},
{
"integer", L_FORT
},
{
"data", L_FORT
},
#endif
{
".ascii", L_MACH
},
{
".asciiz", L_MACH
},
{
".byte", L_MACH
},
{
".even", L_MACH
},
{
".globl", L_MACH
},
{
"clr", L_MACH
},
{
"(input,", L_PAS
},
{
"dcl", L_PLI
},
{
"Received:", L_MAIL
},
{
">From", L_MAIL
},
{
"Return-Path:", L_MAIL
},
{
"Cc:", L_MAIL
},
{
"Newsgroups:", L_NEWS
},
{
"Path:", L_NEWS
},
{
"Organization:", L_NEWS
},
{
NULL, 0
}
};
#define NNAMES ((sizeof(names)/sizeof(struct names)) - 1)
typedef struct magic_rsl_s {
const char *str;
struct magic_rsl_s *next;
} magic_rsl;
typedef struct {
const char *magicfile;
struct magic *magic;
struct magic *last;
} magic_server_config_rec;
typedef struct {
magic_rsl *head;
magic_rsl *tail;
unsigned suf_recursion;
} magic_req_rec;
module AP_MODULE_DECLARE_DATA mime_magic_module;
static void *create_magic_server_config(apr_pool_t *p, server_rec *d) {
return apr_pcalloc(p, sizeof(magic_server_config_rec));
}
static void *merge_magic_server_config(apr_pool_t *p, void *basev, void *addv) {
magic_server_config_rec *base = (magic_server_config_rec *) basev;
magic_server_config_rec *add = (magic_server_config_rec *) addv;
magic_server_config_rec *new = (magic_server_config_rec *)
apr_palloc(p, sizeof(magic_server_config_rec));
new->magicfile = add->magicfile ? add->magicfile : base->magicfile;
new->magic = NULL;
new->last = NULL;
return new;
}
static const char *set_magicfile(cmd_parms *cmd, void *dummy, const char *arg) {
magic_server_config_rec *conf = (magic_server_config_rec *)
ap_get_module_config(cmd->server->module_config,
&mime_magic_module);
if (!conf) {
return MODNAME ": server structure not allocated";
}
conf->magicfile = arg;
return NULL;
}
static const command_rec mime_magic_cmds[] = {
AP_INIT_TAKE1("MimeMagicFile", set_magicfile, NULL, RSRC_CONF,
"Path to MIME Magic file (in file(1) format)"),
{NULL}
};
static magic_req_rec *magic_set_config(request_rec *r) {
magic_req_rec *req_dat = (magic_req_rec *) apr_palloc(r->pool,
sizeof(magic_req_rec));
req_dat->head = req_dat->tail = (magic_rsl *) NULL;
ap_set_module_config(r->request_config, &mime_magic_module, req_dat);
return req_dat;
}
static int magic_rsl_add(request_rec *r, const char *str) {
magic_req_rec *req_dat = (magic_req_rec *)
ap_get_module_config(r->request_config, &mime_magic_module);
magic_rsl *rsl;
if (!req_dat) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_EINVAL, r, APLOGNO(01507)
MODNAME ": request config should not be NULL");
if (!(req_dat = magic_set_config(r))) {
return -1;
}
}
rsl = (magic_rsl *) apr_palloc(r->pool, sizeof(magic_rsl));
rsl->str = str;
rsl->next = (magic_rsl *) NULL;
if (req_dat->head && req_dat->tail) {
req_dat->tail->next = rsl;
req_dat->tail = rsl;
} else {
req_dat->head = req_dat->tail = rsl;
}
return 0;
}
static int magic_rsl_puts(request_rec *r, const char *str) {
return magic_rsl_add(r, str);
}
static int magic_rsl_printf(request_rec *r, char *str,...) {
va_list ap;
char buf[MAXMIMESTRING];
va_start(ap, str);
apr_vsnprintf(buf, sizeof(buf), str, ap);
va_end(ap);
return magic_rsl_add(r, apr_pstrdup(r->pool, buf));
}
static int magic_rsl_putchar(request_rec *r, char c) {
char str[2];
str[0] = c;
str[1] = '\0';
return magic_rsl_add(r, str);
}
static char *rsl_strdup(request_rec *r, int start_frag, int start_pos, int len) {
char *result;
int cur_frag,
cur_pos,
res_pos;
magic_rsl *frag;
magic_req_rec *req_dat = (magic_req_rec *)
ap_get_module_config(r->request_config, &mime_magic_module);
result = (char *) apr_palloc(r->pool, len + 1);
res_pos = 0;
for (frag = req_dat->head, cur_frag = 0;
frag->next;
frag = frag->next, cur_frag++) {
if (cur_frag < start_frag)
continue;
for (cur_pos = (cur_frag == start_frag) ? start_pos : 0;
frag->str[cur_pos];
cur_pos++) {
if (cur_frag >= start_frag
&& cur_pos >= start_pos
&& res_pos <= len) {
result[res_pos++] = frag->str[cur_pos];
if (res_pos > len) {
break;
}
}
}
}
result[res_pos] = 0;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01508)
MODNAME ": rsl_strdup() %d chars: %s", res_pos - 1, result);
#endif
return result;
}
typedef enum {
rsl_leading_space, rsl_type, rsl_subtype, rsl_separator, rsl_encoding
} rsl_states;
static int magic_rsl_to_request(request_rec *r) {
int cur_frag,
cur_pos,
type_frag,
type_pos,
type_len,
encoding_frag,
encoding_pos,
encoding_len;
char *tmp;
magic_rsl *frag;
rsl_states state;
magic_req_rec *req_dat = (magic_req_rec *)
ap_get_module_config(r->request_config, &mime_magic_module);
if (!req_dat || !req_dat->head) {
return DECLINED;
}
state = rsl_leading_space;
type_frag = type_pos = type_len = 0;
encoding_frag = encoding_pos = encoding_len = 0;
for (frag = req_dat->head, cur_frag = 0;
frag && frag->next;
frag = frag->next, cur_frag++) {
for (cur_pos = 0; frag->str[cur_pos]; cur_pos++) {
if (apr_isspace(frag->str[cur_pos])) {
if (state == rsl_leading_space) {
continue;
} else if (state == rsl_type) {
return DECLINED;
} else if (state == rsl_subtype) {
state++;
continue;
} else if (state == rsl_separator) {
continue;
} else if (state == rsl_encoding) {
frag = req_dat->tail;
break;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01509)
MODNAME ": bad state %d (ws)", state);
return DECLINED;
}
} else if (state == rsl_type &&
frag->str[cur_pos] == '/') {
type_len++;
state++;
} else {
if (state == rsl_leading_space) {
state++;
type_frag = cur_frag;
type_pos = cur_pos;
type_len = 1;
continue;
} else if (state == rsl_type ||
state == rsl_subtype) {
type_len++;
continue;
} else if (state == rsl_separator) {
state++;
encoding_frag = cur_frag;
encoding_pos = cur_pos;
encoding_len = 1;
continue;
} else if (state == rsl_encoding) {
encoding_len++;
continue;
} else {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01510)
MODNAME ": bad state %d (ns)", state);
return DECLINED;
}
}
}
}
if (state != rsl_subtype && state != rsl_separator &&
state != rsl_encoding) {
return DECLINED;
}
tmp = rsl_strdup(r, type_frag, type_pos, type_len);
ap_content_type_tolower(tmp);
ap_set_content_type(r, tmp);
if (state == rsl_encoding) {
tmp = rsl_strdup(r, encoding_frag,
encoding_pos, encoding_len);
ap_str_tolower(tmp);
r->content_encoding = tmp;
}
if (!r->content_type ||
(state == rsl_encoding && !r->content_encoding)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01511)
MODNAME ": unexpected state %d; could be caused by bad "
"data in magic file",
state);
return HTTP_INTERNAL_SERVER_ERROR;
}
return OK;
}
static int magic_process(request_rec *r) {
apr_file_t *fd = NULL;
unsigned char buf[HOWMANY + 1];
apr_size_t nbytes = 0;
int result;
switch ((result = fsmagic(r, r->filename))) {
case DONE:
magic_rsl_putchar(r, '\n');
return OK;
case OK:
break;
default:
return result;
}
if (apr_file_open(&fd, r->filename, APR_READ, APR_OS_DEFAULT, r->pool) != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01512)
MODNAME ": can't read `%s'", r->filename);
return DECLINED;
}
nbytes = sizeof(buf) - 1;
if ((result = apr_file_read(fd, (char *) buf, &nbytes)) != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, result, r, APLOGNO(01513)
MODNAME ": read failed: %s", r->filename);
return HTTP_INTERNAL_SERVER_ERROR;
}
if (nbytes == 0) {
return DECLINED;
} else {
buf[nbytes++] = '\0';
result = tryit(r, buf, nbytes, 1);
if (result != OK) {
return result;
}
}
(void) apr_file_close(fd);
(void) magic_rsl_putchar(r, '\n');
return OK;
}
static int tryit(request_rec *r, unsigned char *buf, apr_size_t nb,
int checkzmagic) {
if (checkzmagic == 1) {
if (zmagic(r, buf, nb) == 1)
return OK;
}
if (softmagic(r, buf, nb) == 1)
return OK;
if (ascmagic(r, buf, nb) == 1)
return OK;
return DECLINED;
}
#define EATAB {while (apr_isspace(*l)) ++l;}
static int apprentice(server_rec *s, apr_pool_t *p) {
apr_file_t *f = NULL;
apr_status_t result;
char line[BUFSIZ + 1];
int errs = 0;
int lineno;
#if MIME_MAGIC_DEBUG
int rule = 0;
struct magic *m, *prevm;
#endif
magic_server_config_rec *conf = (magic_server_config_rec *)
ap_get_module_config(s->module_config, &mime_magic_module);
const char *fname = ap_server_root_relative(p, conf->magicfile);
if (!fname) {
ap_log_error(APLOG_MARK, APLOG_ERR, APR_EBADPATH, s, APLOGNO(01514)
MODNAME ": Invalid magic file path %s", conf->magicfile);
return -1;
}
if ((result = apr_file_open(&f, fname, APR_READ | APR_BUFFERED,
APR_OS_DEFAULT, p)) != APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, result, s, APLOGNO(01515)
MODNAME ": can't read magic file %s", fname);
return -1;
}
conf->magic = conf->last = NULL;
for (lineno = 1; apr_file_gets(line, BUFSIZ, f) == APR_SUCCESS; lineno++) {
int ws_offset;
char *last = line + strlen(line) - 1;
while (last >= line
&& apr_isspace(*last)) {
*last = '\0';
--last;
}
ws_offset = 0;
while (line[ws_offset] && apr_isspace(line[ws_offset])) {
ws_offset++;
}
if (line[ws_offset] == 0) {
continue;
}
if (line[ws_offset] == '#')
continue;
#if MIME_MAGIC_DEBUG
rule++;
#endif
if (parse(s, p, line + ws_offset, lineno) != 0)
++errs;
}
(void) apr_file_close(f);
#if MIME_MAGIC_DEBUG
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01516)
MODNAME ": apprentice conf=%x file=%s m=%s m->next=%s last=%s",
conf,
conf->magicfile ? conf->magicfile : "NULL",
conf->magic ? "set" : "NULL",
(conf->magic && conf->magic->next) ? "set" : "NULL",
conf->last ? "set" : "NULL");
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01517)
MODNAME ": apprentice read %d lines, %d rules, %d errors",
lineno, rule, errs);
#endif
#if MIME_MAGIC_DEBUG
prevm = 0;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01518)
MODNAME ": apprentice test");
for (m = conf->magic; m; m = m->next) {
if (apr_isprint((((unsigned long) m) >> 24) & 255) &&
apr_isprint((((unsigned long) m) >> 16) & 255) &&
apr_isprint((((unsigned long) m) >> 8) & 255) &&
apr_isprint(((unsigned long) m) & 255)) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01519)
MODNAME ": apprentice: POINTER CLOBBERED! "
"m=\"%c%c%c%c\" line=%d",
(((unsigned long) m) >> 24) & 255,
(((unsigned long) m) >> 16) & 255,
(((unsigned long) m) >> 8) & 255,
((unsigned long) m) & 255,
prevm ? prevm->lineno : -1);
break;
}
prevm = m;
}
#endif
return (errs ? -1 : 0);
}
static unsigned long signextend(server_rec *s, struct magic *m, unsigned long v) {
if (!(m->flag & UNSIGNED))
switch (m->type) {
case BYTE:
v = (char) v;
break;
case SHORT:
case BESHORT:
case LESHORT:
v = (short) v;
break;
case DATE:
case BEDATE:
case LEDATE:
case LONG:
case BELONG:
case LELONG:
v = (long) v;
break;
case STRING:
break;
default:
ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, APLOGNO(01520)
MODNAME ": can't happen: m->type=%d", m->type);
return -1;
}
return v;
}
static int parse(server_rec *serv, apr_pool_t *p, char *l, int lineno) {
struct magic *m;
char *t, *s;
magic_server_config_rec *conf = (magic_server_config_rec *)
ap_get_module_config(serv->module_config, &mime_magic_module);
m = (struct magic *) apr_pcalloc(p, sizeof(struct magic));
m->next = NULL;
if (!conf->magic || !conf->last) {
conf->magic = conf->last = m;
} else {
conf->last->next = m;
conf->last = m;
}
m->flag = 0;
m->cont_level = 0;
m->lineno = lineno;
while (*l == '>') {
++l;
m->cont_level++;
}
if (m->cont_level != 0 && *l == '(') {
++l;
m->flag |= INDIR;
}
m->offset = (int) strtol(l, &t, 0);
if (l == t) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, serv, APLOGNO(01521)
MODNAME ": offset %s invalid", l);
}
l = t;
if (m->flag & INDIR) {
m->in.type = LONG;
m->in.offset = 0;
if (*l == '.') {
switch (*++l) {
case 'l':
m->in.type = LONG;
break;
case 's':
m->in.type = SHORT;
break;
case 'b':
m->in.type = BYTE;
break;
default:
ap_log_error(APLOG_MARK, APLOG_ERR, 0, serv, APLOGNO(01522)
MODNAME ": indirect offset type %c invalid", *l);
break;
}
l++;
}
s = l;
if (*l == '+' || *l == '-')
l++;
if (apr_isdigit((unsigned char) *l)) {
m->in.offset = strtol(l, &t, 0);
if (*s == '-')
m->in.offset = -m->in.offset;
} else
t = l;
if (*t++ != ')') {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, serv, APLOGNO(01523)
MODNAME ": missing ')' in indirect offset");
}
l = t;
}
while (apr_isdigit((unsigned char) *l))
++l;
EATAB;
#define NBYTE 4
#define NSHORT 5
#define NLONG 4
#define NSTRING 6
#define NDATE 4
#define NBESHORT 7
#define NBELONG 6
#define NBEDATE 6
#define NLESHORT 7
#define NLELONG 6
#define NLEDATE 6
if (*l == 'u') {
++l;
m->flag |= UNSIGNED;
}
if (strncmp(l, "byte", NBYTE) == 0) {
m->type = BYTE;
l += NBYTE;
} else if (strncmp(l, "short", NSHORT) == 0) {
m->type = SHORT;
l += NSHORT;
} else if (strncmp(l, "long", NLONG) == 0) {
m->type = LONG;
l += NLONG;
} else if (strncmp(l, "string", NSTRING) == 0) {
m->type = STRING;
l += NSTRING;
} else if (strncmp(l, "date", NDATE) == 0) {
m->type = DATE;
l += NDATE;
} else if (strncmp(l, "beshort", NBESHORT) == 0) {
m->type = BESHORT;
l += NBESHORT;
} else if (strncmp(l, "belong", NBELONG) == 0) {
m->type = BELONG;
l += NBELONG;
} else if (strncmp(l, "bedate", NBEDATE) == 0) {
m->type = BEDATE;
l += NBEDATE;
} else if (strncmp(l, "leshort", NLESHORT) == 0) {
m->type = LESHORT;
l += NLESHORT;
} else if (strncmp(l, "lelong", NLELONG) == 0) {
m->type = LELONG;
l += NLELONG;
} else if (strncmp(l, "ledate", NLEDATE) == 0) {
m->type = LEDATE;
l += NLEDATE;
} else {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, serv, APLOGNO(01524)
MODNAME ": type %s invalid", l);
return -1;
}
if (*l == '&') {
++l;
m->mask = signextend(serv, m, strtol(l, &l, 0));
} else
m->mask = ~0L;
EATAB;
switch (*l) {
case '>':
case '<':
case '&':
case '^':
case '=':
m->reln = *l;
++l;
break;
case '!':
if (m->type != STRING) {
m->reln = *l;
++l;
break;
}
default:
if (*l == 'x' && apr_isspace(l[1])) {
m->reln = *l;
++l;
goto GetDesc;
}
m->reln = '=';
break;
}
EATAB;
if (getvalue(serv, m, &l))
return -1;
GetDesc:
EATAB;
if (l[0] == '\b') {
++l;
m->nospflag = 1;
} else if ((l[0] == '\\') && (l[1] == 'b')) {
++l;
++l;
m->nospflag = 1;
} else
m->nospflag = 0;
apr_cpystrn(m->desc, l, sizeof(m->desc));
#if MIME_MAGIC_DEBUG
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, serv, APLOGNO(01525)
MODNAME ": parse line=%d m=%x next=%x cont=%d desc=%s",
lineno, m, m->next, m->cont_level, m->desc);
#endif
return 0;
}
static int getvalue(server_rec *s, struct magic *m, char **p) {
int slen;
if (m->type == STRING) {
*p = getstr(s, *p, m->value.s, sizeof(m->value.s), &slen);
m->vallen = slen;
} else if (m->reln != 'x')
m->value.l = signextend(s, m, strtol(*p, p, 0));
return 0;
}
static char *getstr(server_rec *serv, register char *s, register char *p,
int plen, int *slen) {
char *origs = s, *origp = p;
char *pmax = p + plen - 1;
register int c;
register int val;
while ((c = *s++) != '\0') {
if (apr_isspace(c))
break;
if (p >= pmax) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, serv, APLOGNO(01526)
MODNAME ": string too long: %s", origs);
break;
}
if (c == '\\') {
switch (c = *s++) {
case '\0':
goto out;
default:
*p++ = (char) c;
break;
case 'n':
*p++ = '\n';
break;
case 'r':
*p++ = '\r';
break;
case 'b':
*p++ = '\b';
break;
case 't':
*p++ = '\t';
break;
case 'f':
*p++ = '\f';
break;
case 'v':
*p++ = '\v';
break;
case '0':
case '1':
case '2':
case '3':
case '4':
case '5':
case '6':
case '7':
val = c - '0';
c = *s++;
if (c >= '0' && c <= '7') {
val = (val << 3) | (c - '0');
c = *s++;
if (c >= '0' && c <= '7')
val = (val << 3) | (c - '0');
else
--s;
} else
--s;
*p++ = (char) val;
break;
case 'x':
val = 'x';
c = hextoint(*s++);
if (c >= 0) {
val = c;
c = hextoint(*s++);
if (c >= 0) {
val = (val << 4) + c;
c = hextoint(*s++);
if (c >= 0) {
val = (val << 4) + c;
} else
--s;
} else
--s;
} else
--s;
*p++ = (char) val;
break;
}
} else
*p++ = (char) c;
}
out:
*p = '\0';
*slen = p - origp;
return s;
}
static int hextoint(int c) {
if (apr_isdigit(c))
return c - '0';
if ((c >= 'a') && (c <= 'f'))
return c + 10 - 'a';
if ((c >= 'A') && (c <= 'F'))
return c + 10 - 'A';
return -1;
}
static int fsmagic(request_rec *r, const char *fn) {
switch (r->finfo.filetype) {
case APR_DIR:
magic_rsl_puts(r, DIR_MAGIC_TYPE);
return DONE;
case APR_CHR:
(void) magic_rsl_puts(r, MIME_BINARY_UNKNOWN);
return DONE;
case APR_BLK:
(void) magic_rsl_puts(r, MIME_BINARY_UNKNOWN);
return DONE;
case APR_PIPE:
(void) magic_rsl_puts(r, MIME_BINARY_UNKNOWN);
return DONE;
case APR_LNK:
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01527)
MODNAME ": broken symlink (%s)", fn);
return HTTP_INTERNAL_SERVER_ERROR;
case APR_SOCK:
magic_rsl_puts(r, MIME_BINARY_UNKNOWN);
return DONE;
case APR_REG:
break;
default:
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01528)
MODNAME ": invalid file type %d.", r->finfo.filetype);
return HTTP_INTERNAL_SERVER_ERROR;
}
if (r->finfo.size == 0) {
magic_rsl_puts(r, MIME_TEXT_UNKNOWN);
return DONE;
}
return OK;
}
static int softmagic(request_rec *r, unsigned char *buf, apr_size_t nbytes) {
if (match(r, buf, nbytes))
return 1;
return 0;
}
static int match(request_rec *r, unsigned char *s, apr_size_t nbytes) {
#if MIME_MAGIC_DEBUG
int rule_counter = 0;
#endif
int cont_level = 0;
int need_separator = 0;
union VALUETYPE p;
magic_server_config_rec *conf = (magic_server_config_rec *)
ap_get_module_config(r->server->module_config, &mime_magic_module);
struct magic *m;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01529)
MODNAME ": match conf=%x file=%s m=%s m->next=%s last=%s",
conf,
conf->magicfile ? conf->magicfile : "NULL",
conf->magic ? "set" : "NULL",
(conf->magic && conf->magic->next) ? "set" : "NULL",
conf->last ? "set" : "NULL");
#endif
#if MIME_MAGIC_DEBUG
for (m = conf->magic; m; m = m->next) {
if (apr_isprint((((unsigned long) m) >> 24) & 255) &&
apr_isprint((((unsigned long) m) >> 16) & 255) &&
apr_isprint((((unsigned long) m) >> 8) & 255) &&
apr_isprint(((unsigned long) m) & 255)) {
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01530)
MODNAME ": match: POINTER CLOBBERED! "
"m=\"%c%c%c%c\"",
(((unsigned long) m) >> 24) & 255,
(((unsigned long) m) >> 16) & 255,
(((unsigned long) m) >> 8) & 255,
((unsigned long) m) & 255);
break;
}
}
#endif
for (m = conf->magic; m; m = m->next) {
#if MIME_MAGIC_DEBUG
rule_counter++;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01531)
MODNAME ": line=%d desc=%s", m->lineno, m->desc);
#endif
if (!mget(r, &p, s, m, nbytes) ||
!mcheck(r, &p, m)) {
struct magic *m_cont;
if (!m->next || (m->next->cont_level == 0)) {
continue;
}
m_cont = m->next;
while (m_cont && (m_cont->cont_level != 0)) {
#if MIME_MAGIC_DEBUG
rule_counter++;
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01532)
MODNAME ": line=%d mc=%x mc->next=%x cont=%d desc=%s",
m_cont->lineno, m_cont,
m_cont->next, m_cont->cont_level,
m_cont->desc);
#endif
m = m_cont;
m_cont = m_cont->next;
}
continue;
}
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01533)
MODNAME ": rule matched, line=%d type=%d %s",
m->lineno, m->type,
(m->type == STRING) ? m->value.s : "");
#endif
mprint(r, &p, m);
if (m->desc[0])
need_separator = 1;
cont_level++;
m = m->next;
while (m && (m->cont_level != 0)) {
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01534)
MODNAME ": match line=%d cont=%d type=%d %s",
m->lineno, m->cont_level, m->type,
(m->type == STRING) ? m->value.s : "");
#endif
if (cont_level >= m->cont_level) {
if (cont_level > m->cont_level) {
cont_level = m->cont_level;
}
if (mget(r, &p, s, m, nbytes) &&
mcheck(r, &p, m)) {
if (need_separator
&& (m->nospflag == 0)
&& (m->desc[0] != '\0')
) {
(void) magic_rsl_putchar(r, ' ');
need_separator = 0;
}
mprint(r, &p, m);
if (m->desc[0])
need_separator = 1;
cont_level++;
}
}
m = m->next;
}
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01535)
MODNAME ": matched after %d rules", rule_counter);
#endif
return 1;
}
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01536)
MODNAME ": failed after %d rules", rule_counter);
#endif
return 0;
}
static void mprint(request_rec *r, union VALUETYPE *p, struct magic *m) {
char *pp;
unsigned long v;
char time_str[APR_CTIME_LEN];
switch (m->type) {
case BYTE:
v = p->b;
break;
case SHORT:
case BESHORT:
case LESHORT:
v = p->h;
break;
case LONG:
case BELONG:
case LELONG:
v = p->l;
break;
case STRING:
if (m->reln == '=') {
(void) magic_rsl_printf(r, m->desc, m->value.s);
} else {
(void) magic_rsl_printf(r, m->desc, p->s);
}
return;
case DATE:
case BEDATE:
case LEDATE:
apr_ctime(time_str, apr_time_from_sec(*(time_t *)&p->l));
pp = time_str;
(void) magic_rsl_printf(r, m->desc, pp);
return;
default:
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01537)
MODNAME ": invalid m->type (%d) in mprint().",
m->type);
return;
}
v = signextend(r->server, m, v) & m->mask;
(void) magic_rsl_printf(r, m->desc, (unsigned long) v);
}
static int mconvert(request_rec *r, union VALUETYPE *p, struct magic *m) {
char *rt;
switch (m->type) {
case BYTE:
case SHORT:
case LONG:
case DATE:
return 1;
case STRING:
p->s[sizeof(p->s) - 1] = '\0';
if ((rt = strchr(p->s, '\n')) != NULL)
*rt = '\0';
return 1;
case BESHORT:
p->h = (short) ((p->hs[0] << 8) | (p->hs[1]));
return 1;
case BELONG:
case BEDATE:
p->l = (long)
((p->hl[0] << 24) | (p->hl[1] << 16) | (p->hl[2] << 8) | (p->hl[3]));
return 1;
case LESHORT:
p->h = (short) ((p->hs[1] << 8) | (p->hs[0]));
return 1;
case LELONG:
case LEDATE:
p->l = (long)
((p->hl[3] << 24) | (p->hl[2] << 16) | (p->hl[1] << 8) | (p->hl[0]));
return 1;
default:
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01538)
MODNAME ": invalid type %d in mconvert().", m->type);
return 0;
}
}
static int mget(request_rec *r, union VALUETYPE *p, unsigned char *s,
struct magic *m, apr_size_t nbytes) {
long offset = m->offset;
if (offset + sizeof(union VALUETYPE) > nbytes)
return 0;
memcpy(p, s + offset, sizeof(union VALUETYPE));
if (!mconvert(r, p, m))
return 0;
if (m->flag & INDIR) {
switch (m->in.type) {
case BYTE:
offset = p->b + m->in.offset;
break;
case SHORT:
offset = p->h + m->in.offset;
break;
case LONG:
offset = p->l + m->in.offset;
break;
}
if (offset + sizeof(union VALUETYPE) > nbytes)
return 0;
memcpy(p, s + offset, sizeof(union VALUETYPE));
if (!mconvert(r, p, m))
return 0;
}
return 1;
}
static int mcheck(request_rec *r, union VALUETYPE *p, struct magic *m) {
register unsigned long l = m->value.l;
register unsigned long v;
int matched;
if ((m->value.s[0] == 'x') && (m->value.s[1] == '\0')) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01539)
MODNAME ": BOINK");
return 1;
}
switch (m->type) {
case BYTE:
v = p->b;
break;
case SHORT:
case BESHORT:
case LESHORT:
v = p->h;
break;
case LONG:
case BELONG:
case LELONG:
case DATE:
case BEDATE:
case LEDATE:
v = p->l;
break;
case STRING:
l = 0;
v = 0;
{
register unsigned char *a = (unsigned char *) m->value.s;
register unsigned char *b = (unsigned char *) p->s;
register int len = m->vallen;
while (--len >= 0)
if ((v = *b++ - *a++) != 0)
break;
}
break;
default:
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01540)
MODNAME ": invalid type %d in mcheck().", m->type);
return 0;
}
v = signextend(r->server, m, v) & m->mask;
switch (m->reln) {
case 'x':
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01541)
"%lu == *any* = 1", v);
#endif
matched = 1;
break;
case '!':
matched = v != l;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01542)
"%lu != %lu = %d", v, l, matched);
#endif
break;
case '=':
matched = v == l;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01543)
"%lu == %lu = %d", v, l, matched);
#endif
break;
case '>':
if (m->flag & UNSIGNED) {
matched = v > l;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01544)
"%lu > %lu = %d", v, l, matched);
#endif
} else {
matched = (long) v > (long) l;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01545)
"%ld > %ld = %d", v, l, matched);
#endif
}
break;
case '<':
if (m->flag & UNSIGNED) {
matched = v < l;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01546)
"%lu < %lu = %d", v, l, matched);
#endif
} else {
matched = (long) v < (long) l;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01547)
"%ld < %ld = %d", v, l, matched);
#endif
}
break;
case '&':
matched = (v & l) == l;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01548)
"((%lx & %lx) == %lx) = %d", v, l, l, matched);
#endif
break;
case '^':
matched = (v & l) != l;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01549)
"((%lx & %lx) != %lx) = %d", v, l, l, matched);
#endif
break;
default:
matched = 0;
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(01550)
MODNAME ": mcheck: can't happen: invalid relation %d.",
m->reln);
break;
}
return matched;
}
#define STREQ(a, b) (*(a) == *(b) && strcmp((a), (b)) == 0)
static int ascmagic(request_rec *r, unsigned char *buf, apr_size_t nbytes) {
int has_escapes = 0;
unsigned char *s;
char nbuf[SMALL_HOWMANY + 1];
char *token;
const struct names *p;
int small_nbytes;
char *strtok_state;
if (*buf == '.') {
unsigned char *tp = buf + 1;
while (apr_isspace(*tp))
++tp;
if ((apr_isalnum(*tp) || *tp == '\\') &&
(apr_isalnum(*(tp + 1)) || *tp == '"')) {
magic_rsl_puts(r, "application/x-troff");
return 1;
}
}
if ((*buf == 'c' || *buf == 'C') && apr_isspace(*(buf + 1))) {
magic_rsl_puts(r, "text/plain");
return 1;
}
small_nbytes = (nbytes > SMALL_HOWMANY) ? SMALL_HOWMANY : nbytes;
s = (unsigned char *) memcpy(nbuf, buf, small_nbytes);
s[small_nbytes] = '\0';
has_escapes = (memchr(s, '\033', small_nbytes) != NULL);
while ((token = apr_strtok((char *) s, " \t\n\r\f", &strtok_state)) != NULL) {
s = NULL;
for (p = names; p < names + NNAMES; p++) {
if (STREQ(p->name, token)) {
magic_rsl_puts(r, types[p->type]);
if (has_escapes)
magic_rsl_puts(r, " (with escape sequences)");
return 1;
}
}
}
switch (is_tar(buf, nbytes)) {
case 1:
magic_rsl_puts(r, "application/x-tar");
return 1;
case 2:
magic_rsl_puts(r, "application/x-tar");
return 1;
}
return 0;
}
static struct {
char *magic;
apr_size_t maglen;
char *argv[3];
int silent;
char *encoding;
} compr[] = {
{
"\037\235", 2, {
"gzip", "-dcq", NULL
}, 0, "x-compress"
},
{
"\037\213", 2, {
"gzip", "-dcq", NULL
}, 1, "x-gzip"
},
{
"\037\036", 2, {
"gzip", "-dcq", NULL
}, 0, "x-gzip"
},
};
static int ncompr = sizeof(compr) / sizeof(compr[0]);
static int zmagic(request_rec *r, unsigned char *buf, apr_size_t nbytes) {
unsigned char *newbuf;
int newsize;
int i;
for (i = 0; i < ncompr; i++) {
if (nbytes < compr[i].maglen)
continue;
if (memcmp(buf, compr[i].magic, compr[i].maglen) == 0)
break;
}
if (i == ncompr)
return 0;
if ((newsize = uncompress(r, i, &newbuf, HOWMANY)) > 0) {
r->content_encoding = compr[i].encoding;
newbuf[newsize-1] = '\0';
if (tryit(r, newbuf, newsize, 0) != OK) {
return 0;
}
}
return 1;
}
struct uncompress_parms {
request_rec *r;
int method;
};
static int create_uncompress_child(struct uncompress_parms *parm, apr_pool_t *cntxt,
apr_file_t **pipe_in) {
int rc = 1;
const char *new_argv[4];
request_rec *r = parm->r;
apr_pool_t *child_context = cntxt;
apr_procattr_t *procattr;
apr_proc_t *procnew;
if ((apr_procattr_create(&procattr, child_context) != APR_SUCCESS) ||
(apr_procattr_io_set(procattr, APR_FULL_BLOCK,
APR_FULL_BLOCK, APR_NO_PIPE) != APR_SUCCESS) ||
(apr_procattr_dir_set(procattr,
ap_make_dirstr_parent(r->pool, r->filename)) != APR_SUCCESS) ||
(apr_procattr_cmdtype_set(procattr, APR_PROGRAM_PATH) != APR_SUCCESS)) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_ENOPROC, r, APLOGNO(01551)
"couldn't setup child process: %s", r->filename);
} else {
new_argv[0] = compr[parm->method].argv[0];
new_argv[1] = compr[parm->method].argv[1];
new_argv[2] = r->filename;
new_argv[3] = NULL;
procnew = apr_pcalloc(child_context, sizeof(*procnew));
rc = apr_proc_create(procnew, compr[parm->method].argv[0],
new_argv, NULL, procattr, child_context);
if (rc != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, APR_ENOPROC, r, APLOGNO(01552)
MODNAME ": could not execute `%s'.",
compr[parm->method].argv[0]);
} else {
apr_pool_note_subprocess(child_context, procnew, APR_KILL_AFTER_TIMEOUT);
*pipe_in = procnew->out;
}
}
return (rc);
}
static int uncompress(request_rec *r, int method,
unsigned char **newch, apr_size_t n) {
struct uncompress_parms parm;
apr_file_t *pipe_out = NULL;
apr_pool_t *sub_context;
apr_status_t rv;
parm.r = r;
parm.method = method;
if (apr_pool_create(&sub_context, r->pool) != APR_SUCCESS)
return -1;
if ((rv = create_uncompress_child(&parm, sub_context, &pipe_out)) != APR_SUCCESS) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(01553)
MODNAME ": couldn't spawn uncompress process: %s", r->uri);
return -1;
}
*newch = (unsigned char *) apr_palloc(r->pool, n);
rv = apr_file_read(pipe_out, *newch, &n);
if (n == 0) {
apr_pool_destroy(sub_context);
ap_log_rerror(APLOG_MARK, APLOG_ERR, rv, r, APLOGNO(01554)
MODNAME ": read failed from uncompress of %s", r->filename);
return -1;
}
apr_pool_destroy(sub_context);
return n;
}
#define isodigit(c) (((unsigned char)(c) >= '0') && ((unsigned char)(c) <= '7'))
static int is_tar(unsigned char *buf, apr_size_t nbytes) {
register union record *header = (union record *) buf;
register int i;
register long sum, recsum;
register char *p;
if (nbytes < sizeof(union record))
return 0;
recsum = from_oct(8, header->header.chksum);
sum = 0;
p = header->charptr;
for (i = sizeof(union record); --i >= 0;) {
sum += 0xFF & *p++;
}
for (i = sizeof(header->header.chksum); --i >= 0;)
sum -= 0xFF & header->header.chksum[i];
sum += ' ' * sizeof header->header.chksum;
if (sum != recsum)
return 0;
if (0 == strcmp(header->header.magic, TMAGIC))
return 2;
return 1;
}
static long from_oct(int digs, char *where) {
register long value;
while (apr_isspace(*where)) {
where++;
if (--digs <= 0)
return -1;
}
value = 0;
while (digs > 0 && isodigit(*where)) {
value = (value << 3) | (*where++ - '0');
--digs;
}
if (digs > 0 && *where && !apr_isspace(*where))
return -1;
return value;
}
static int revision_suffix(request_rec *r) {
int suffix_pos, result;
char *sub_filename;
request_rec *sub;
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01555)
MODNAME ": revision_suffix checking %s", r->filename);
#endif
suffix_pos = strlen(r->filename) - 1;
if (!apr_isdigit(r->filename[suffix_pos])) {
return 0;
}
while (suffix_pos >= 0 && apr_isdigit(r->filename[suffix_pos]))
suffix_pos--;
if (suffix_pos < 0 || r->filename[suffix_pos] != '@') {
return 0;
}
result = 0;
sub_filename = apr_pstrndup(r->pool, r->filename, suffix_pos);
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01556)
MODNAME ": subrequest lookup for %s", sub_filename);
#endif
sub = ap_sub_req_lookup_file(sub_filename, r, NULL);
if (sub->content_type) {
ap_set_content_type(r, apr_pstrdup(r->pool, sub->content_type));
#if MIME_MAGIC_DEBUG
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(01557)
MODNAME ": subrequest %s got %s",
sub_filename, r->content_type);
#endif
if (sub->content_encoding)
r->content_encoding =
apr_pstrdup(r->pool, sub->content_encoding);
if (sub->content_languages) {
int n;
r->content_languages = apr_array_copy(r->pool,
sub->content_languages);
for (n = 0; n < r->content_languages->nelts; ++n) {
char **lang = ((char **)r->content_languages->elts) + n;
*lang = apr_pstrdup(r->pool, *lang);
}
}
result = 1;
}
ap_destroy_sub_req(sub);
return result;
}
static int magic_init(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *main_server) {
int result;
magic_server_config_rec *conf;
magic_server_config_rec *main_conf;
server_rec *s;
#if MIME_MAGIC_DEBUG
struct magic *m, *prevm;
#endif
main_conf = ap_get_module_config(main_server->module_config, &mime_magic_module);
for (s = main_server; s; s = s->next) {
conf = ap_get_module_config(s->module_config, &mime_magic_module);
if (conf->magicfile == NULL && s != main_server) {
*conf = *main_conf;
} else if (conf->magicfile) {
result = apprentice(s, p);
if (result == -1)
return OK;
#if MIME_MAGIC_DEBUG
prevm = 0;
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01558)
MODNAME ": magic_init 1 test");
for (m = conf->magic; m; m = m->next) {
if (apr_isprint((((unsigned long) m) >> 24) & 255) &&
apr_isprint((((unsigned long) m) >> 16) & 255) &&
apr_isprint((((unsigned long) m) >> 8) & 255) &&
apr_isprint(((unsigned long) m) & 255)) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(01559)
MODNAME ": magic_init 1: POINTER CLOBBERED! "
"m=\"%c%c%c%c\" line=%d",
(((unsigned long) m) >> 24) & 255,
(((unsigned long) m) >> 16) & 255,
(((unsigned long) m) >> 8) & 255,
((unsigned long) m) & 255,
prevm ? prevm->lineno : -1);
break;
}
prevm = m;
}
#endif
}
}
return OK;
}
static int magic_find_ct(request_rec *r) {
int result;
magic_server_config_rec *conf;
if (r->finfo.filetype == APR_NOFILE || !r->filename) {
return DECLINED;
}
if (r->content_type) {
return DECLINED;
}
conf = ap_get_module_config(r->server->module_config, &mime_magic_module);
if (!conf || !conf->magic) {
return DECLINED;
}
if (!magic_set_config(r)) {
return HTTP_INTERNAL_SERVER_ERROR;
}
if (revision_suffix(r) != 1) {
if ((result = magic_process(r)) != OK) {
return result;
}
}
return magic_rsl_to_request(r);
}
static void register_hooks(apr_pool_t *p) {
static const char * const aszPre[]= { "mod_mime.c", NULL };
ap_hook_type_checker(magic_find_ct, aszPre, NULL, APR_HOOK_MIDDLE);
ap_hook_post_config(magic_init, NULL, NULL, APR_HOOK_FIRST);
}
AP_DECLARE_MODULE(mime_magic) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
create_magic_server_config,
merge_magic_server_config,
mime_magic_cmds,
register_hooks
};
