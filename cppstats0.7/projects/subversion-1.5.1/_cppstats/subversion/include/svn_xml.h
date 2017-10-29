#if !defined(SVN_XML_H)
#define SVN_XML_H
#include <apr.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include "svn_error.h"
#include "svn_string.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_XML_NAMESPACE "svn:"
enum svn_xml_open_tag_style {
svn_xml_normal = 1,
svn_xml_protect_pcdata,
svn_xml_self_closing
};
svn_boolean_t svn_xml_is_xml_safe(const char *data,
apr_size_t len);
void svn_xml_escape_cdata_stringbuf(svn_stringbuf_t **outstr,
const svn_stringbuf_t *string,
apr_pool_t *pool);
void svn_xml_escape_cdata_string(svn_stringbuf_t **outstr,
const svn_string_t *string,
apr_pool_t *pool);
void svn_xml_escape_cdata_cstring(svn_stringbuf_t **outstr,
const char *string,
apr_pool_t *pool);
void svn_xml_escape_attr_stringbuf(svn_stringbuf_t **outstr,
const svn_stringbuf_t *string,
apr_pool_t *pool);
void svn_xml_escape_attr_string(svn_stringbuf_t **outstr,
const svn_string_t *string,
apr_pool_t *pool);
void svn_xml_escape_attr_cstring(svn_stringbuf_t **outstr,
const char *string,
apr_pool_t *pool);
const char *svn_xml_fuzzy_escape(const char *string,
apr_pool_t *pool);
typedef struct svn_xml_parser_t svn_xml_parser_t;
typedef void (*svn_xml_start_elem)(void *baton,
const char *name,
const char **atts);
typedef void (*svn_xml_end_elem)(void *baton, const char *name);
typedef void (*svn_xml_char_data)(void *baton,
const char *data,
apr_size_t len);
svn_xml_parser_t *svn_xml_make_parser(void *baton,
svn_xml_start_elem start_handler,
svn_xml_end_elem end_handler,
svn_xml_char_data data_handler,
apr_pool_t *pool);
void svn_xml_free_parser(svn_xml_parser_t *svn_parser);
svn_error_t *svn_xml_parse(svn_xml_parser_t *svn_parser,
const char *buf,
apr_size_t len,
svn_boolean_t is_final);
void svn_xml_signal_bailout(svn_error_t *error,
svn_xml_parser_t *svn_parser);
const char *svn_xml_get_attr_value(const char *name, const char **atts);
apr_hash_t *svn_xml_ap_to_hash(va_list ap, apr_pool_t *pool);
apr_hash_t *svn_xml_make_att_hash(const char **atts, apr_pool_t *pool);
void svn_xml_hash_atts_preserving(const char **atts,
apr_hash_t *ht,
apr_pool_t *pool);
void svn_xml_hash_atts_overlaying(const char **atts,
apr_hash_t *ht,
apr_pool_t *pool);
void svn_xml_make_header(svn_stringbuf_t **str, apr_pool_t *pool);
void svn_xml_make_open_tag(svn_stringbuf_t **str,
apr_pool_t *pool,
enum svn_xml_open_tag_style style,
const char *tagname,
...);
void svn_xml_make_open_tag_v(svn_stringbuf_t **str,
apr_pool_t *pool,
enum svn_xml_open_tag_style style,
const char *tagname,
va_list ap);
void svn_xml_make_open_tag_hash(svn_stringbuf_t **str,
apr_pool_t *pool,
enum svn_xml_open_tag_style style,
const char *tagname,
apr_hash_t *attributes);
void svn_xml_make_close_tag(svn_stringbuf_t **str,
apr_pool_t *pool,
const char *tagname);
#if defined(__cplusplus)
}
#endif
#endif