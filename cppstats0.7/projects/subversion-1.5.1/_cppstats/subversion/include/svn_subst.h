#if !defined(SVN_SUBST_H)
#define SVN_SUBST_H
#include "svn_types.h"
#include "svn_string.h"
#include "svn_io.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef enum svn_subst_eol_style {
svn_subst_eol_style_unknown,
svn_subst_eol_style_none,
svn_subst_eol_style_native,
svn_subst_eol_style_fixed
} svn_subst_eol_style_t;
void
svn_subst_eol_style_from_value(svn_subst_eol_style_t *style,
const char **eol,
const char *value);
svn_boolean_t
svn_subst_translation_required(svn_subst_eol_style_t style,
const char *eol,
apr_hash_t *keywords,
svn_boolean_t special,
svn_boolean_t force_eol_check);
typedef struct svn_subst_keywords_t {
const svn_string_t *revision;
const svn_string_t *date;
const svn_string_t *author;
const svn_string_t *url;
const svn_string_t *id;
} svn_subst_keywords_t;
svn_error_t *
svn_subst_build_keywords2(apr_hash_t **kw,
const char *keywords_string,
const char *rev,
const char *url,
apr_time_t date,
const char *author,
apr_pool_t *pool);
svn_error_t *
svn_subst_build_keywords(svn_subst_keywords_t *kw,
const char *keywords_string,
const char *rev,
const char *url,
apr_time_t date,
const char *author,
apr_pool_t *pool);
svn_boolean_t
svn_subst_keywords_differ2(apr_hash_t *a,
apr_hash_t *b,
svn_boolean_t compare_values,
apr_pool_t *pool);
svn_boolean_t
svn_subst_keywords_differ(const svn_subst_keywords_t *a,
const svn_subst_keywords_t *b,
svn_boolean_t compare_values);
svn_error_t *
svn_subst_translate_stream3(svn_stream_t *src,
svn_stream_t *dst,
const char *eol_str,
svn_boolean_t repair,
apr_hash_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool);
svn_stream_t *
svn_subst_stream_translated(svn_stream_t *stream,
const char *eol_str,
svn_boolean_t repair,
apr_hash_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool);
svn_error_t *
svn_subst_stream_translated_to_normal_form(svn_stream_t **stream,
svn_stream_t *source,
svn_subst_eol_style_t eol_style,
const char *eol_str,
svn_boolean_t always_repair_eols,
apr_hash_t *keywords,
apr_pool_t *pool);
svn_error_t *
svn_subst_stream_from_specialfile(svn_stream_t **stream,
const char *path,
apr_pool_t *pool);
svn_error_t *
svn_subst_translate_stream2(svn_stream_t *src,
svn_stream_t *dst,
const char *eol_str,
svn_boolean_t repair,
const svn_subst_keywords_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool);
svn_error_t *
svn_subst_translate_stream(svn_stream_t *src,
svn_stream_t *dst,
const char *eol_str,
svn_boolean_t repair,
const svn_subst_keywords_t *keywords,
svn_boolean_t expand);
svn_error_t *
svn_subst_copy_and_translate3(const char *src,
const char *dst,
const char *eol_str,
svn_boolean_t repair,
apr_hash_t *keywords,
svn_boolean_t expand,
svn_boolean_t special,
apr_pool_t *pool);
svn_error_t *
svn_subst_copy_and_translate2(const char *src,
const char *dst,
const char *eol_str,
svn_boolean_t repair,
const svn_subst_keywords_t *keywords,
svn_boolean_t expand,
svn_boolean_t special,
apr_pool_t *pool);
svn_error_t *
svn_subst_copy_and_translate(const char *src,
const char *dst,
const char *eol_str,
svn_boolean_t repair,
const svn_subst_keywords_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool);
svn_error_t *
svn_subst_translate_cstring2(const char *src,
const char **dst,
const char *eol_str,
svn_boolean_t repair,
apr_hash_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool);
svn_error_t *
svn_subst_translate_cstring(const char *src,
const char **dst,
const char *eol_str,
svn_boolean_t repair,
const svn_subst_keywords_t *keywords,
svn_boolean_t expand,
apr_pool_t *pool);
svn_error_t *
svn_subst_translate_to_normal_form(const char *src,
const char *dst,
svn_subst_eol_style_t eol_style,
const char *eol_str,
svn_boolean_t always_repair_eols,
apr_hash_t *keywords,
svn_boolean_t special,
apr_pool_t *pool);
svn_error_t *
svn_subst_stream_detranslated(svn_stream_t **stream_p,
const char *src,
svn_subst_eol_style_t eol_style,
const char *eol_str,
svn_boolean_t always_repair_eols,
apr_hash_t *keywords,
svn_boolean_t special,
apr_pool_t *pool);
svn_error_t *svn_subst_translate_string(svn_string_t **new_value,
const svn_string_t *value,
const char *encoding,
apr_pool_t *pool);
svn_error_t *svn_subst_detranslate_string(svn_string_t **new_value,
const svn_string_t *value,
svn_boolean_t for_output,
apr_pool_t *pool);
#if defined(__cplusplus)
}
#endif
#endif