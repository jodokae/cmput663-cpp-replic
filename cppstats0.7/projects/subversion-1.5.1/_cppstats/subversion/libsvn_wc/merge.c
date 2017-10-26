#include "svn_wc.h"
#include "svn_diff.h"
#include "svn_path.h"
#include "wc.h"
#include "entries.h"
#include "translate.h"
#include "questions.h"
#include "log.h"
#include "svn_private_config.h"
static const svn_prop_t *
get_prop(const apr_array_header_t *prop_diff,
const char *prop_name) {
if (prop_diff) {
int i;
for (i = 0; i < prop_diff->nelts; i++) {
const svn_prop_t *elt = &APR_ARRAY_IDX(prop_diff, i, svn_prop_t);
if (strcmp(elt->name,prop_name) == 0)
return elt;
}
}
return NULL;
}
static svn_error_t *
detranslate_wc_file(const char **detranslated_file,
const char *merge_target,
svn_wc_adm_access_t *adm_access,
svn_boolean_t force_copy,
const apr_array_header_t *prop_diff,
apr_pool_t *pool) {
svn_boolean_t is_binary;
const svn_prop_t *prop;
svn_subst_eol_style_t style;
const char *eol;
apr_hash_t *keywords;
svn_boolean_t special;
SVN_ERR(svn_wc_has_binary_prop(&is_binary,
merge_target, adm_access, pool));
if (is_binary
&& (((prop = get_prop(prop_diff, SVN_PROP_MIME_TYPE))
&& prop->value && svn_mime_type_is_binary(prop->value->data))
|| prop == NULL)) {
keywords = NULL;
special = FALSE;
eol = NULL;
style = svn_subst_eol_style_none;
} else if ((!is_binary)
&& (prop = get_prop(prop_diff, SVN_PROP_MIME_TYPE))
&& prop->value && svn_mime_type_is_binary(prop->value->data)) {
SVN_ERR(svn_wc__get_keywords(&keywords, merge_target,
adm_access, NULL, pool));
SVN_ERR(svn_wc__get_special(&special, merge_target, adm_access, pool));
} else {
SVN_ERR(svn_wc__get_special(&special, merge_target, adm_access, pool));
if (special) {
keywords = NULL;
eol = NULL;
style = svn_subst_eol_style_none;
} else {
if ((prop = get_prop(prop_diff, SVN_PROP_EOL_STYLE)) && prop->value) {
svn_subst_eol_style_from_value(&style, &eol, prop->value->data);
} else if (!is_binary)
SVN_ERR(svn_wc__get_eol_style(&style, &eol, merge_target,
adm_access, pool));
else {
eol = NULL;
style = svn_subst_eol_style_none;
}
if (!is_binary)
SVN_ERR(svn_wc__get_keywords(&keywords, merge_target,
adm_access, NULL, pool));
else
keywords = NULL;
}
}
if (force_copy || keywords || eol || special) {
const char *detranslated;
SVN_ERR(svn_wc_create_tmp_file2
(NULL, &detranslated,
svn_wc_adm_access_path(adm_access),
svn_io_file_del_none, pool));
SVN_ERR(svn_subst_translate_to_normal_form(merge_target,
detranslated,
style,
eol, eol ? FALSE : TRUE,
keywords,
special,
pool));
*detranslated_file = detranslated;
} else
*detranslated_file = merge_target;
return SVN_NO_ERROR;
}
static svn_error_t *
maybe_update_target_eols(const char **new_target,
const char *old_target,
svn_wc_adm_access_t *adm_access,
const apr_array_header_t *prop_diff,
apr_pool_t *pool) {
const svn_prop_t *prop = get_prop(prop_diff, SVN_PROP_EOL_STYLE);
if (prop && prop->value) {
const char *eol;
const char *tmp_new;
svn_subst_eol_style_from_value(NULL, &eol, prop->value->data);
SVN_ERR(svn_wc_create_tmp_file2(NULL, &tmp_new,
svn_wc_adm_access_path(adm_access),
svn_io_file_del_none,
pool));
SVN_ERR(svn_subst_copy_and_translate3(old_target,
tmp_new,
eol, eol ? FALSE : TRUE,
NULL, FALSE,
FALSE, pool));
*new_target = tmp_new;
} else
*new_target = old_target;
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc__merge_internal(svn_stringbuf_t **log_accum,
enum svn_wc_merge_outcome_t *merge_outcome,
const char *left,
const char *right,
const char *merge_target,
const char *copyfrom_text,
svn_wc_adm_access_t *adm_access,
const char *left_label,
const char *right_label,
const char *target_label,
svn_boolean_t dry_run,
const char *diff3_cmd,
const apr_array_header_t *merge_options,
const apr_array_header_t *prop_diff,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
apr_pool_t *pool) {
const char *tmp_target, *result_target, *working_text;
const char *adm_path = svn_wc_adm_access_path(adm_access);
apr_file_t *result_f;
svn_boolean_t is_binary = FALSE;
const svn_wc_entry_t *entry;
svn_boolean_t contains_conflicts;
const svn_prop_t *mimeprop;
SVN_ERR(svn_wc_entry(&entry, merge_target, adm_access, FALSE, pool));
if (! entry && ! copyfrom_text) {
*merge_outcome = svn_wc_merge_no_merge;
return SVN_NO_ERROR;
}
if ((mimeprop = get_prop(prop_diff, SVN_PROP_MIME_TYPE))
&& mimeprop->value)
is_binary = svn_mime_type_is_binary(mimeprop->value->data);
else if (! copyfrom_text)
SVN_ERR(svn_wc_has_binary_prop(&is_binary, merge_target, adm_access, pool));
working_text = copyfrom_text ? copyfrom_text : merge_target;
SVN_ERR(detranslate_wc_file(&tmp_target, working_text, adm_access,
(! is_binary) && diff3_cmd != NULL,
prop_diff, pool));
SVN_ERR(maybe_update_target_eols(&left, left, adm_access, prop_diff, pool));
if (! is_binary) {
SVN_ERR(svn_wc_create_tmp_file2(&result_f, &result_target,
adm_path, svn_io_file_del_none,
pool));
if (diff3_cmd) {
int exit_code;
SVN_ERR(svn_io_run_diff3_2(&exit_code, ".",
tmp_target, left, right,
target_label, left_label, right_label,
result_f, diff3_cmd,
merge_options, pool));
contains_conflicts = exit_code == 1;
} else {
svn_diff_t *diff;
const char *target_marker;
const char *left_marker;
const char *right_marker;
svn_stream_t *ostream;
svn_diff_file_options_t *options;
ostream = svn_stream_from_aprfile(result_f, pool);
options = svn_diff_file_options_create(pool);
if (merge_options)
SVN_ERR(svn_diff_file_options_parse(options, merge_options, pool));
SVN_ERR(svn_diff_file_diff3_2(&diff,
left, tmp_target, right,
options, pool));
if (target_label)
target_marker = apr_psprintf(pool, "<<<<<<< %s", target_label);
else
target_marker = "<<<<<<< .working";
if (left_label)
left_marker = apr_psprintf(pool, "||||||| %s", left_label);
else
left_marker = "||||||| .old";
if (right_label)
right_marker = apr_psprintf(pool, ">>>>>>> %s", right_label);
else
right_marker = ">>>>>>> .new";
SVN_ERR(svn_diff_file_output_merge(ostream, diff,
left, tmp_target, right,
left_marker,
target_marker,
right_marker,
"=======",
FALSE,
FALSE,
pool));
SVN_ERR(svn_stream_close(ostream));
contains_conflicts = svn_diff_contains_conflicts(diff);
}
SVN_ERR(svn_io_file_close(result_f, pool));
if (contains_conflicts && ! dry_run) {
const char *left_copy, *right_copy, *target_copy;
const char *tmp_left, *tmp_right, *tmp_target_copy;
const char *parentt, *target_base;
svn_wc_adm_access_t *parent_access;
svn_wc_entry_t tmp_entry;
if (conflict_func) {
svn_wc_conflict_result_t *result = NULL;
svn_wc_conflict_description_t cdesc;
cdesc.path = merge_target;
cdesc.node_kind = svn_node_file;
cdesc.kind = svn_wc_conflict_kind_text;
cdesc.is_binary = FALSE;
cdesc.mime_type = (mimeprop && mimeprop->value)
? mimeprop->value->data : NULL;
cdesc.access = adm_access;
cdesc.action = svn_wc_conflict_action_edit;
cdesc.reason = svn_wc_conflict_reason_edited;
cdesc.base_file = left;
cdesc.their_file = right;
cdesc.my_file = tmp_target;
cdesc.merged_file = result_target;
cdesc.property_name = NULL;
SVN_ERR(conflict_func(&result, &cdesc, conflict_baton, pool));
if (result == NULL)
return svn_error_create(SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE,
NULL, _("Conflict callback violated API:"
" returned no results"));
switch (result->choice) {
case svn_wc_conflict_choose_base: {
SVN_ERR(svn_wc__loggy_copy
(log_accum, NULL, adm_access,
svn_wc__copy_translate,
left, merge_target,
FALSE, pool));
*merge_outcome = svn_wc_merge_merged;
contains_conflicts = FALSE;
goto merge_complete;
}
case svn_wc_conflict_choose_theirs_full: {
SVN_ERR(svn_wc__loggy_copy
(log_accum, NULL, adm_access,
svn_wc__copy_translate,
right, merge_target,
FALSE, pool));
*merge_outcome = svn_wc_merge_merged;
contains_conflicts = FALSE;
goto merge_complete;
}
case svn_wc_conflict_choose_mine_full: {
*merge_outcome = svn_wc_merge_merged;
contains_conflicts = FALSE;
goto merge_complete;
}
case svn_wc_conflict_choose_merged: {
SVN_ERR(svn_wc__loggy_copy
(log_accum, NULL, adm_access,
svn_wc__copy_translate,
result->merged_file ?
result->merged_file : result_target,
merge_target,
FALSE, pool));
*merge_outcome = svn_wc_merge_merged;
contains_conflicts = FALSE;
goto merge_complete;
}
case svn_wc_conflict_choose_postpone:
default: {
}
}
}
SVN_ERR(svn_io_open_unique_file2(NULL,
&left_copy,
merge_target,
left_label,
svn_io_file_del_none,
pool));
SVN_ERR(svn_io_open_unique_file2(NULL,
&right_copy,
merge_target,
right_label,
svn_io_file_del_none,
pool));
SVN_ERR(svn_io_open_unique_file2(NULL,
&target_copy,
merge_target,
target_label,
svn_io_file_del_none,
pool));
svn_path_split(target_copy, &parentt, &target_base, pool);
SVN_ERR(svn_wc_adm_retrieve(&parent_access, adm_access, parentt,
pool));
if (! svn_path_is_child(adm_path, left, pool)) {
SVN_ERR(svn_wc_create_tmp_file2
(NULL, &tmp_left,
adm_path, svn_io_file_del_none, pool));
SVN_ERR(svn_io_copy_file(left, tmp_left, TRUE, pool));
} else
tmp_left = left;
if (! svn_path_is_child(adm_path, right, pool)) {
SVN_ERR(svn_wc_create_tmp_file2
(NULL, &tmp_right,
adm_path, svn_io_file_del_none, pool));
SVN_ERR(svn_io_copy_file(right, tmp_right, TRUE, pool));
} else
tmp_right = right;
SVN_ERR(svn_wc__loggy_translated_file(log_accum,
adm_access,
left_copy, tmp_left,
merge_target, pool));
SVN_ERR(svn_wc__loggy_translated_file(log_accum,
adm_access,
right_copy, tmp_right,
merge_target, pool));
SVN_ERR(svn_wc_translated_file2(&tmp_target_copy,
merge_target,
merge_target,
adm_access,
SVN_WC_TRANSLATE_TO_NF
| SVN_WC_TRANSLATE_NO_OUTPUT_CLEANUP,
pool));
SVN_ERR(svn_wc__loggy_translated_file
(log_accum, adm_access,
target_copy, tmp_target_copy, merge_target, pool));
tmp_entry.conflict_old
= svn_path_is_child(adm_path, left_copy, pool);
tmp_entry.conflict_new
= svn_path_is_child(adm_path, right_copy, pool);
tmp_entry.conflict_wrk = target_base;
SVN_ERR(svn_wc__loggy_entry_modify
(log_accum, adm_access,
merge_target, &tmp_entry,
SVN_WC__ENTRY_MODIFY_CONFLICT_OLD
| SVN_WC__ENTRY_MODIFY_CONFLICT_NEW
| SVN_WC__ENTRY_MODIFY_CONFLICT_WRK,
pool));
*merge_outcome = svn_wc_merge_conflict;
} else if (contains_conflicts && dry_run) {
*merge_outcome = svn_wc_merge_conflict;
}
else if (copyfrom_text) {
*merge_outcome = svn_wc_merge_merged;
} else {
svn_boolean_t same, special;
SVN_ERR(svn_wc__get_special(&special, merge_target,
adm_access, pool));
SVN_ERR(svn_io_files_contents_same_p(&same, result_target,
(special ?
tmp_target :
merge_target),
pool));
*merge_outcome = same ? svn_wc_merge_unchanged : svn_wc_merge_merged;
}
if (*merge_outcome != svn_wc_merge_unchanged && ! dry_run)
SVN_ERR(svn_wc__loggy_copy(log_accum, NULL,
adm_access,
svn_wc__copy_translate,
result_target, merge_target,
FALSE, pool));
}
else if (! dry_run) {
const char *left_copy, *right_copy;
const char *parentt, *left_base, *right_base;
svn_wc_entry_t tmp_entry;
if (conflict_func) {
svn_wc_conflict_result_t *result = NULL;
svn_wc_conflict_description_t cdesc;
cdesc.path = merge_target;
cdesc.node_kind = svn_node_file;
cdesc.kind = svn_wc_conflict_kind_text;
cdesc.is_binary = TRUE;
cdesc.mime_type = (mimeprop && mimeprop->value)
? mimeprop->value->data : NULL;
cdesc.access = adm_access;
cdesc.action = svn_wc_conflict_action_edit;
cdesc.reason = svn_wc_conflict_reason_edited;
cdesc.base_file = left;
cdesc.their_file = right;
cdesc.my_file = tmp_target;
cdesc.merged_file = NULL;
cdesc.property_name = NULL;
SVN_ERR(conflict_func(&result, &cdesc, conflict_baton, pool));
if (result == NULL)
return svn_error_create(SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE,
NULL, _("Conflict callback violated API:"
" returned no results"));
switch (result->choice) {
case svn_wc_conflict_choose_base: {
SVN_ERR(svn_wc__loggy_copy
(log_accum, NULL, adm_access,
svn_wc__copy_translate,
left, merge_target,
FALSE, pool));
*merge_outcome = svn_wc_merge_merged;
contains_conflicts = FALSE;
goto merge_complete;
}
case svn_wc_conflict_choose_theirs_full: {
SVN_ERR(svn_wc__loggy_copy
(log_accum, NULL, adm_access,
svn_wc__copy_translate,
right, merge_target,
FALSE, pool));
*merge_outcome = svn_wc_merge_merged;
contains_conflicts = FALSE;
goto merge_complete;
}
case svn_wc_conflict_choose_mine_full: {
*merge_outcome = svn_wc_merge_merged;
contains_conflicts = FALSE;
goto merge_complete;
}
case svn_wc_conflict_choose_merged: {
if (! result->merged_file) {
return svn_error_create
(SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE,
NULL, _("Conflict callback violated API:"
" returned no merged file"));
} else {
SVN_ERR(svn_wc__loggy_copy
(log_accum, NULL, adm_access,
svn_wc__copy_translate,
result->merged_file, merge_target,
FALSE, pool));
*merge_outcome = svn_wc_merge_merged;
contains_conflicts = FALSE;
goto merge_complete;
}
}
case svn_wc_conflict_choose_postpone:
default: {
}
}
}
SVN_ERR(svn_io_open_unique_file2(NULL,
&left_copy,
merge_target,
left_label,
svn_io_file_del_none,
pool));
SVN_ERR(svn_io_open_unique_file2(NULL,
&right_copy,
merge_target,
right_label,
svn_io_file_del_none,
pool));
SVN_ERR(svn_io_copy_file(left,
left_copy, TRUE, pool));
SVN_ERR(svn_io_copy_file(right,
right_copy, TRUE, pool));
if (merge_target != tmp_target) {
const char *mine_copy;
SVN_ERR(svn_io_open_unique_file2(NULL,
&mine_copy,
merge_target,
target_label,
svn_io_file_del_none,
pool));
SVN_ERR(svn_wc__loggy_move(log_accum, NULL,
adm_access,
tmp_target,
mine_copy,
FALSE, pool));
mine_copy = svn_path_is_child(adm_path, mine_copy, pool);
tmp_entry.conflict_wrk = mine_copy;
} else
tmp_entry.conflict_wrk = NULL;
svn_path_split(left_copy, &parentt, &left_base, pool);
svn_path_split(right_copy, &parentt, &right_base, pool);
tmp_entry.conflict_old = left_base;
tmp_entry.conflict_new = right_base;
SVN_ERR(svn_wc__loggy_entry_modify
(log_accum,
adm_access, merge_target,
&tmp_entry,
SVN_WC__ENTRY_MODIFY_CONFLICT_OLD
| SVN_WC__ENTRY_MODIFY_CONFLICT_NEW
| SVN_WC__ENTRY_MODIFY_CONFLICT_WRK,
pool));
*merge_outcome = svn_wc_merge_conflict;
}
else
*merge_outcome = svn_wc_merge_conflict;
merge_complete:
if (! dry_run) {
SVN_ERR(svn_wc__loggy_maybe_set_executable(log_accum,
adm_access, merge_target,
pool));
SVN_ERR(svn_wc__loggy_maybe_set_readonly(log_accum,
adm_access, merge_target,
pool));
}
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_merge3(enum svn_wc_merge_outcome_t *merge_outcome,
const char *left,
const char *right,
const char *merge_target,
svn_wc_adm_access_t *adm_access,
const char *left_label,
const char *right_label,
const char *target_label,
svn_boolean_t dry_run,
const char *diff3_cmd,
const apr_array_header_t *merge_options,
const apr_array_header_t *prop_diff,
svn_wc_conflict_resolver_func_t conflict_func,
void *conflict_baton,
apr_pool_t *pool) {
svn_stringbuf_t *log_accum = svn_stringbuf_create("", pool);
SVN_ERR(svn_wc__merge_internal(&log_accum, merge_outcome,
left, right, merge_target,
NULL,
adm_access,
left_label, right_label, target_label,
dry_run,
diff3_cmd,
merge_options,
prop_diff,
conflict_func, conflict_baton,
pool));
SVN_ERR(svn_wc__write_log(adm_access, 0, log_accum, pool));
SVN_ERR(svn_wc__run_log(adm_access, NULL, pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_wc_merge2(enum svn_wc_merge_outcome_t *merge_outcome,
const char *left,
const char *right,
const char *merge_target,
svn_wc_adm_access_t *adm_access,
const char *left_label,
const char *right_label,
const char *target_label,
svn_boolean_t dry_run,
const char *diff3_cmd,
const apr_array_header_t *merge_options,
apr_pool_t *pool) {
return svn_wc_merge3(merge_outcome,
left, right, merge_target, adm_access,
left_label, right_label, target_label,
dry_run, diff3_cmd, merge_options, NULL,
NULL, NULL, pool);
}
svn_error_t *
svn_wc_merge(const char *left,
const char *right,
const char *merge_target,
svn_wc_adm_access_t *adm_access,
const char *left_label,
const char *right_label,
const char *target_label,
svn_boolean_t dry_run,
enum svn_wc_merge_outcome_t *merge_outcome,
const char *diff3_cmd,
apr_pool_t *pool) {
return svn_wc_merge3(merge_outcome,
left, right, merge_target, adm_access,
left_label, right_label, target_label,
dry_run, diff3_cmd, NULL, NULL, NULL,
NULL, pool);
}
svn_wc_conflict_result_t *
svn_wc_create_conflict_result(svn_wc_conflict_choice_t choice,
const char *merged_file,
apr_pool_t *pool) {
svn_wc_conflict_result_t *result = apr_pcalloc(pool, sizeof(*result));
result->choice = choice;
result->merged_file = merged_file;
return result;
}
