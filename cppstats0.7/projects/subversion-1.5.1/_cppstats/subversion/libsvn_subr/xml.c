#include <string.h>
#include <assert.h>
#include "svn_private_config.h"
#include "svn_pools.h"
#include "svn_xml.h"
#include "svn_error.h"
#include "svn_ctype.h"
#include "utf_impl.h"
#if defined(SVN_HAVE_OLD_EXPAT)
#include <xmlparse.h>
#else
#include <expat.h>
#endif
#if defined(XML_UNICODE)
#error Expat is unusable -- it has been compiled for wide characters
#endif
struct svn_xml_parser_t {
XML_Parser parser;
svn_xml_start_elem start_handler;
svn_xml_end_elem end_handler;
svn_xml_char_data data_handler;
void *baton;
svn_error_t *error;
apr_pool_t *pool;
};
svn_boolean_t
svn_xml_is_xml_safe(const char *data, apr_size_t len) {
const char *end = data + len;
const char *p;
if (! svn_utf__is_valid(data, len))
return FALSE;
for (p = data; p < end; p++) {
unsigned char c = *p;
if (svn_ctype_iscntrl(c)) {
if ((c != SVN_CTYPE_ASCII_TAB)
&& (c != SVN_CTYPE_ASCII_LINEFEED)
&& (c != SVN_CTYPE_ASCII_CARRIAGERETURN)
&& (c != SVN_CTYPE_ASCII_DELETE))
return FALSE;
}
}
return TRUE;
}
static void
xml_escape_cdata(svn_stringbuf_t **outstr,
const char *data,
apr_size_t len,
apr_pool_t *pool) {
const char *end = data + len;
const char *p = data, *q;
if (*outstr == NULL)
*outstr = svn_stringbuf_create("", pool);
while (1) {
q = p;
while (q < end && *q != '&' && *q != '<' && *q != '>' && *q != '\r')
q++;
svn_stringbuf_appendbytes(*outstr, p, q - p);
if (q == end)
break;
if (*q == '&')
svn_stringbuf_appendcstr(*outstr, "&amp;");
else if (*q == '<')
svn_stringbuf_appendcstr(*outstr, "&lt;");
else if (*q == '>')
svn_stringbuf_appendcstr(*outstr, "&gt;");
else if (*q == '\r')
svn_stringbuf_appendcstr(*outstr, "&#13;");
p = q + 1;
}
}
static void
xml_escape_attr(svn_stringbuf_t **outstr,
const char *data,
apr_size_t len,
apr_pool_t *pool) {
const char *end = data + len;
const char *p = data, *q;
if (*outstr == NULL)
*outstr = svn_stringbuf_create("", pool);
while (1) {
q = p;
while (q < end && *q != '&' && *q != '<' && *q != '>'
&& *q != '"' && *q != '\'' && *q != '\r'
&& *q != '\n' && *q != '\t')
q++;
svn_stringbuf_appendbytes(*outstr, p, q - p);
if (q == end)
break;
if (*q == '&')
svn_stringbuf_appendcstr(*outstr, "&amp;");
else if (*q == '<')
svn_stringbuf_appendcstr(*outstr, "&lt;");
else if (*q == '>')
svn_stringbuf_appendcstr(*outstr, "&gt;");
else if (*q == '"')
svn_stringbuf_appendcstr(*outstr, "&quot;");
else if (*q == '\'')
svn_stringbuf_appendcstr(*outstr, "&apos;");
else if (*q == '\r')
svn_stringbuf_appendcstr(*outstr, "&#13;");
else if (*q == '\n')
svn_stringbuf_appendcstr(*outstr, "&#10;");
else if (*q == '\t')
svn_stringbuf_appendcstr(*outstr, "&#9;");
p = q + 1;
}
}
void
svn_xml_escape_cdata_stringbuf(svn_stringbuf_t **outstr,
const svn_stringbuf_t *string,
apr_pool_t *pool) {
xml_escape_cdata(outstr, string->data, string->len, pool);
}
void
svn_xml_escape_cdata_string(svn_stringbuf_t **outstr,
const svn_string_t *string,
apr_pool_t *pool) {
xml_escape_cdata(outstr, string->data, string->len, pool);
}
void
svn_xml_escape_cdata_cstring(svn_stringbuf_t **outstr,
const char *string,
apr_pool_t *pool) {
xml_escape_cdata(outstr, string, (apr_size_t) strlen(string), pool);
}
void
svn_xml_escape_attr_stringbuf(svn_stringbuf_t **outstr,
const svn_stringbuf_t *string,
apr_pool_t *pool) {
xml_escape_attr(outstr, string->data, string->len, pool);
}
void
svn_xml_escape_attr_string(svn_stringbuf_t **outstr,
const svn_string_t *string,
apr_pool_t *pool) {
xml_escape_attr(outstr, string->data, string->len, pool);
}
void
svn_xml_escape_attr_cstring(svn_stringbuf_t **outstr,
const char *string,
apr_pool_t *pool) {
xml_escape_attr(outstr, string, (apr_size_t) strlen(string), pool);
}
const char *
svn_xml_fuzzy_escape(const char *string, apr_pool_t *pool) {
const char *end = string + strlen(string);
const char *p = string, *q;
svn_stringbuf_t *outstr;
char escaped_char[6];
for (q = p; q < end; q++) {
if (svn_ctype_iscntrl(*q)
&& ! ((*q == '\n') || (*q == '\r') || (*q == '\t')))
break;
}
if (q == end)
return string;
outstr = svn_stringbuf_create("", pool);
while (1) {
q = p;
while ((q < end)
&& ((! svn_ctype_iscntrl(*q))
|| (*q == '\n') || (*q == '\r') || (*q == '\t')))
q++;
svn_stringbuf_appendbytes(outstr, p, q - p);
if (q == end)
break;
sprintf(escaped_char, "?\\%03u", (unsigned char) *q);
svn_stringbuf_appendcstr(outstr, escaped_char);
p = q + 1;
}
return outstr->data;
}
static void expat_start_handler(void *userData,
const XML_Char *name,
const XML_Char **atts) {
svn_xml_parser_t *svn_parser = userData;
(*svn_parser->start_handler)(svn_parser->baton, name, atts);
}
static void expat_end_handler(void *userData, const XML_Char *name) {
svn_xml_parser_t *svn_parser = userData;
(*svn_parser->end_handler)(svn_parser->baton, name);
}
static void expat_data_handler(void *userData, const XML_Char *s, int len) {
svn_xml_parser_t *svn_parser = userData;
(*svn_parser->data_handler)(svn_parser->baton, s, (apr_size_t)len);
}
svn_xml_parser_t *
svn_xml_make_parser(void *baton,
svn_xml_start_elem start_handler,
svn_xml_end_elem end_handler,
svn_xml_char_data data_handler,
apr_pool_t *pool) {
svn_xml_parser_t *svn_parser;
apr_pool_t *subpool;
XML_Parser parser = XML_ParserCreate(NULL);
XML_SetElementHandler(parser,
start_handler ? expat_start_handler : NULL,
end_handler ? expat_end_handler : NULL);
XML_SetCharacterDataHandler(parser,
data_handler ? expat_data_handler : NULL);
subpool = svn_pool_create(pool);
svn_parser = apr_pcalloc(subpool, sizeof(*svn_parser));
svn_parser->parser = parser;
svn_parser->start_handler = start_handler;
svn_parser->end_handler = end_handler;
svn_parser->data_handler = data_handler;
svn_parser->baton = baton;
svn_parser->pool = subpool;
XML_SetUserData(parser, svn_parser);
return svn_parser;
}
void
svn_xml_free_parser(svn_xml_parser_t *svn_parser) {
XML_ParserFree(svn_parser->parser);
svn_pool_destroy(svn_parser->pool);
}
svn_error_t *
svn_xml_parse(svn_xml_parser_t *svn_parser,
const char *buf,
apr_size_t len,
svn_boolean_t is_final) {
svn_error_t *err;
int success;
success = XML_Parse(svn_parser->parser, buf, len, is_final);
if (! success) {
long line = XML_GetCurrentLineNumber(svn_parser->parser);
err = svn_error_createf
(SVN_ERR_XML_MALFORMED, NULL,
_("Malformed XML: %s at line %ld"),
XML_ErrorString(XML_GetErrorCode(svn_parser->parser)), line);
svn_xml_free_parser(svn_parser);
return err;
}
if (svn_parser->error) {
err = svn_parser->error;
svn_xml_free_parser(svn_parser);
return err;
}
return SVN_NO_ERROR;
}
void svn_xml_signal_bailout(svn_error_t *error,
svn_xml_parser_t *svn_parser) {
XML_SetElementHandler(svn_parser->parser, NULL, NULL);
XML_SetCharacterDataHandler(svn_parser->parser, NULL);
svn_parser->error = error;
}
const char *
svn_xml_get_attr_value(const char *name, const char **atts) {
while (atts && (*atts)) {
if (strcmp(atts[0], name) == 0)
return atts[1];
else
atts += 2;
}
return NULL;
}
void
svn_xml_make_header(svn_stringbuf_t **str, apr_pool_t *pool) {
if (*str == NULL)
*str = svn_stringbuf_create("", pool);
svn_stringbuf_appendcstr(*str,
"<?xml version=\"1.0\"?>\n");
}
static void
amalgamate(const char **atts,
apr_hash_t *ht,
svn_boolean_t preserve,
apr_pool_t *pool) {
const char *key;
if (atts)
for (key = *atts; key; key = *(++atts)) {
const char *val = *(++atts);
size_t keylen;
assert(key != NULL);
keylen = strlen(key);
if (preserve && ((apr_hash_get(ht, key, keylen)) != NULL))
continue;
else
apr_hash_set(ht, apr_pstrndup(pool, key, keylen), keylen,
val ? apr_pstrdup(pool, val) : NULL);
}
}
apr_hash_t *
svn_xml_ap_to_hash(va_list ap, apr_pool_t *pool) {
apr_hash_t *ht = apr_hash_make(pool);
const char *key;
while ((key = va_arg(ap, char *)) != NULL) {
const char *val = va_arg(ap, const char *);
apr_hash_set(ht, key, APR_HASH_KEY_STRING, val);
}
return ht;
}
apr_hash_t *
svn_xml_make_att_hash(const char **atts, apr_pool_t *pool) {
apr_hash_t *ht = apr_hash_make(pool);
amalgamate(atts, ht, 0, pool);
return ht;
}
void
svn_xml_hash_atts_overlaying(const char **atts,
apr_hash_t *ht,
apr_pool_t *pool) {
amalgamate(atts, ht, 0, pool);
}
void
svn_xml_hash_atts_preserving(const char **atts,
apr_hash_t *ht,
apr_pool_t *pool) {
amalgamate(atts, ht, 1, pool);
}
void
svn_xml_make_open_tag_hash(svn_stringbuf_t **str,
apr_pool_t *pool,
enum svn_xml_open_tag_style style,
const char *tagname,
apr_hash_t *attributes) {
apr_hash_index_t *hi;
if (*str == NULL)
*str = svn_stringbuf_create("", pool);
svn_stringbuf_appendcstr(*str, "<");
svn_stringbuf_appendcstr(*str, tagname);
for (hi = apr_hash_first(pool, attributes); hi; hi = apr_hash_next(hi)) {
const void *key;
void *val;
apr_hash_this(hi, &key, NULL, &val);
assert(val != NULL);
svn_stringbuf_appendcstr(*str, "\n ");
svn_stringbuf_appendcstr(*str, key);
svn_stringbuf_appendcstr(*str, "=\"");
svn_xml_escape_attr_cstring(str, val, pool);
svn_stringbuf_appendcstr(*str, "\"");
}
if (style == svn_xml_self_closing)
svn_stringbuf_appendcstr(*str, "/");
svn_stringbuf_appendcstr(*str, ">");
if (style != svn_xml_protect_pcdata)
svn_stringbuf_appendcstr(*str, "\n");
}
void
svn_xml_make_open_tag_v(svn_stringbuf_t **str,
apr_pool_t *pool,
enum svn_xml_open_tag_style style,
const char *tagname,
va_list ap) {
apr_pool_t *subpool = svn_pool_create(pool);
apr_hash_t *ht = svn_xml_ap_to_hash(ap, subpool);
svn_xml_make_open_tag_hash(str, pool, style, tagname, ht);
svn_pool_destroy(subpool);
}
void
svn_xml_make_open_tag(svn_stringbuf_t **str,
apr_pool_t *pool,
enum svn_xml_open_tag_style style,
const char *tagname,
...) {
va_list ap;
va_start(ap, tagname);
svn_xml_make_open_tag_v(str, pool, style, tagname, ap);
va_end(ap);
}
void svn_xml_make_close_tag(svn_stringbuf_t **str,
apr_pool_t *pool,
const char *tagname) {
if (*str == NULL)
*str = svn_stringbuf_create("", pool);
svn_stringbuf_appendcstr(*str, "</");
svn_stringbuf_appendcstr(*str, tagname);
svn_stringbuf_appendcstr(*str, ">\n");
}