#include "util_cookies.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#define LOG_PREFIX "ap_cookie: "
#undef APLOG_MODULE_INDEX
#define APLOG_MODULE_INDEX AP_CORE_MODULE_INDEX
AP_DECLARE(apr_status_t) ap_cookie_write(request_rec * r, const char *name, const char *val,
const char *attrs, long maxage, ...) {
const char *buffer;
const char *rfc2109;
apr_table_t *t;
va_list vp;
buffer = "";
if (maxage) {
buffer = apr_pstrcat(r->pool, "Max-Age=", apr_ltoa(r->pool, maxage), ";", NULL);
}
rfc2109 = apr_pstrcat(r->pool, name, "=", val, ";", buffer,
attrs && *attrs ? attrs : DEFAULT_ATTRS, NULL);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00007) LOG_PREFIX
"user '%s' set cookie: '%s'", r->user, rfc2109);
va_start(vp, maxage);
while ((t = va_arg(vp, apr_table_t *))) {
apr_table_addn(t, SET_COOKIE, rfc2109);
}
va_end(vp);
return APR_SUCCESS;
}
AP_DECLARE(apr_status_t) ap_cookie_write2(request_rec * r, const char *name2, const char *val,
const char *attrs2, long maxage, ...) {
const char *buffer;
const char *rfc2965;
apr_table_t *t;
va_list vp;
buffer = "";
if (maxage) {
buffer = apr_pstrcat(r->pool, "Max-Age=", apr_ltoa(r->pool, maxage), ";", NULL);
}
rfc2965 = apr_pstrcat(r->pool, name2, "=", val, ";", buffer,
attrs2 && *attrs2 ? attrs2 : DEFAULT_ATTRS, NULL);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00008) LOG_PREFIX
"user '%s' set cookie2: '%s'", r->user, rfc2965);
va_start(vp, maxage);
while ((t = va_arg(vp, apr_table_t *))) {
apr_table_addn(t, SET_COOKIE2, rfc2965);
}
va_end(vp);
return APR_SUCCESS;
}
AP_DECLARE(apr_status_t) ap_cookie_remove(request_rec * r, const char *name, const char *attrs, ...) {
apr_table_t *t;
va_list vp;
const char *rfc2109 = apr_pstrcat(r->pool, name, "=;Max-Age=0;",
attrs ? attrs : CLEAR_ATTRS, NULL);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00009) LOG_PREFIX
"user '%s' removed cookie: '%s'", r->user, rfc2109);
va_start(vp, attrs);
while ((t = va_arg(vp, apr_table_t *))) {
apr_table_addn(t, SET_COOKIE, rfc2109);
}
va_end(vp);
return APR_SUCCESS;
}
AP_DECLARE(apr_status_t) ap_cookie_remove2(request_rec * r, const char *name2, const char *attrs2, ...) {
apr_table_t *t;
va_list vp;
const char *rfc2965 = apr_pstrcat(r->pool, name2, "=;Max-Age=0;",
attrs2 ? attrs2 : CLEAR_ATTRS, NULL);
ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, APLOGNO(00010) LOG_PREFIX
"user '%s' removed cookie2: '%s'", r->user, rfc2965);
va_start(vp, attrs2);
while ((t = va_arg(vp, apr_table_t *))) {
apr_table_addn(t, SET_COOKIE2, rfc2965);
}
va_end(vp);
return APR_SUCCESS;
}
static int extract_cookie_line(void *varg, const char *key, const char *val) {
ap_cookie_do *v = varg;
char *last1, *last2;
char *cookie = apr_pstrdup(v->r->pool, val);
const char *name = apr_pstrcat(v->r->pool, v->name ? v->name : "", "=", NULL);
apr_size_t len = strlen(name);
const char *new_cookie = "";
const char *comma = ",";
char *next1;
const char *semi = ";";
char *next2;
const char *sep = "";
int cookies = 0;
int eat = 0;
next1 = apr_strtok(cookie, comma, &last1);
while (next1) {
next2 = apr_strtok(next1, semi, &last2);
while (next2) {
char *trim = next2;
while (apr_isspace(*trim)) {
trim++;
}
if (!strncmp(trim, name, len)) {
if (v->encoded) {
if (strcmp(v->encoded, trim + len)) {
v->duplicated = 1;
}
}
v->encoded = apr_pstrdup(v->r->pool, trim + len);
eat = 1;
} else {
if (*trim != '$') {
cookies++;
eat = 0;
}
if (!eat) {
new_cookie = apr_pstrcat(v->r->pool, new_cookie, sep, next2, NULL);
}
}
next2 = apr_strtok(NULL, semi, &last2);
sep = semi;
}
next1 = apr_strtok(NULL, comma, &last1);
sep = comma;
}
if (cookies) {
apr_table_addn(v->new_cookies, key, new_cookie);
}
return 1;
}
AP_DECLARE(apr_status_t) ap_cookie_read(request_rec * r, const char *name, const char **val,
int remove) {
ap_cookie_do v;
v.r = r;
v.encoded = NULL;
v.new_cookies = apr_table_make(r->pool, 10);
v.duplicated = 0;
v.name = name;
apr_table_do(extract_cookie_line, &v, r->headers_in,
"Cookie", "Cookie2", NULL);
if (v.duplicated) {
ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00011) LOG_PREFIX
"client submitted cookie '%s' more than once: %s", v.name, r->uri);
return APR_EGENERAL;
}
if (remove) {
apr_table_unset(r->headers_in, "Cookie");
apr_table_unset(r->headers_in, "Cookie2");
r->headers_in = apr_table_overlay(r->pool, r->headers_in, v.new_cookies);
}
*val = v.encoded;
return APR_SUCCESS;
}
AP_DECLARE(apr_status_t) ap_cookie_check_string(const char *string) {
if (!string || !*string || ap_strchr_c(string, '=') || ap_strchr_c(string, '&') ||
ap_strchr_c(string, ';')) {
return APR_EGENERAL;
}
return APR_SUCCESS;
}
