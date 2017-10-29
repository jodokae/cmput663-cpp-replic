#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include <apr_want.h>
#include <apr_general.h>
#include <apr_lib.h>
#include "svn_error.h"
#include "svn_pools.h"
#include "config_impl.h"
#include "svn_private_config.h"
typedef struct cfg_section_t cfg_section_t;
struct cfg_section_t {
const char *name;
const char *hash_key;
apr_hash_t *options;
};
typedef struct cfg_option_t cfg_option_t;
struct cfg_option_t {
const char *name;
const char *hash_key;
const char *value;
const char *x_value;
svn_boolean_t expanded;
};
svn_error_t *
svn_config_read(svn_config_t **cfgp, const char *file,
svn_boolean_t must_exist, apr_pool_t *pool) {
svn_config_t *cfg = apr_palloc(pool, sizeof(*cfg));
svn_error_t *err;
cfg->sections = apr_hash_make(pool);
cfg->pool = pool;
cfg->x_pool = svn_pool_create(pool);
cfg->x_values = FALSE;
cfg->tmp_key = svn_stringbuf_create("", pool);
cfg->tmp_value = svn_stringbuf_create("", pool);
#if defined(WIN32)
if (0 == strncmp(file, SVN_REGISTRY_PREFIX, SVN_REGISTRY_PREFIX_LEN))
err = svn_config__parse_registry(cfg, file + SVN_REGISTRY_PREFIX_LEN,
must_exist, pool);
else
#endif
err = svn_config__parse_file(cfg, file, must_exist, pool);
if (err != SVN_NO_ERROR)
return err;
else
*cfgp = cfg;
return SVN_NO_ERROR;
}
static svn_error_t *
read_all(svn_config_t **cfgp,
const char *sys_registry_path,
const char *usr_registry_path,
const char *sys_file_path,
const char *usr_file_path,
apr_pool_t *pool) {
svn_boolean_t red_config = FALSE;
#if defined(WIN32)
if (sys_registry_path) {
SVN_ERR(svn_config_read(cfgp, sys_registry_path, FALSE, pool));
red_config = TRUE;
}
#endif
if (sys_file_path) {
if (red_config)
SVN_ERR(svn_config_merge(*cfgp, sys_file_path, FALSE));
else {
SVN_ERR(svn_config_read(cfgp, sys_file_path, FALSE, pool));
red_config = TRUE;
}
}
#if defined(WIN32)
if (usr_registry_path) {
if (red_config)
SVN_ERR(svn_config_merge(*cfgp, usr_registry_path, FALSE));
else {
SVN_ERR(svn_config_read(cfgp, usr_registry_path, FALSE, pool));
red_config = TRUE;
}
}
#endif
if (usr_file_path) {
if (red_config)
SVN_ERR(svn_config_merge(*cfgp, usr_file_path, FALSE));
else {
SVN_ERR(svn_config_read(cfgp, usr_file_path, FALSE, pool));
red_config = TRUE;
}
}
if (! red_config)
*cfgp = NULL;
return SVN_NO_ERROR;
}
static svn_error_t *
get_category_config(svn_config_t **cfg,
const char *config_dir,
const char *category,
apr_pool_t *pool) {
const char *usr_reg_path = NULL, *sys_reg_path = NULL;
const char *usr_cfg_path, *sys_cfg_path;
*cfg = NULL;
if (! config_dir) {
#if defined(WIN32)
sys_reg_path = apr_pstrcat(pool, SVN_REGISTRY_SYS_CONFIG_PATH,
category, NULL);
usr_reg_path = apr_pstrcat(pool, SVN_REGISTRY_USR_CONFIG_PATH,
category, NULL);
#endif
SVN_ERR(svn_config__sys_config_path(&sys_cfg_path, category, pool));
} else
sys_cfg_path = NULL;
SVN_ERR(svn_config__user_config_path(config_dir, &usr_cfg_path, category,
pool));
SVN_ERR(read_all(cfg,
sys_reg_path, usr_reg_path,
sys_cfg_path, usr_cfg_path,
pool));
return SVN_NO_ERROR;
}
svn_error_t *
svn_config_get_config(apr_hash_t **cfg_hash,
const char *config_dir,
apr_pool_t *pool) {
svn_config_t *cfg;
*cfg_hash = apr_hash_make(pool);
#define CATLEN (sizeof(SVN_CONFIG_CATEGORY_SERVERS) - 1)
SVN_ERR(get_category_config(&cfg, config_dir, SVN_CONFIG_CATEGORY_SERVERS,
pool));
if (cfg)
apr_hash_set(*cfg_hash, SVN_CONFIG_CATEGORY_SERVERS, CATLEN, cfg);
#undef CATLEN
#define CATLEN (sizeof(SVN_CONFIG_CATEGORY_CONFIG) - 1)
SVN_ERR(get_category_config(&cfg, config_dir, SVN_CONFIG_CATEGORY_CONFIG,
pool));
if (cfg)
apr_hash_set(*cfg_hash, SVN_CONFIG_CATEGORY_CONFIG, CATLEN, cfg);
#undef CATLEN
return SVN_NO_ERROR;
}
static void
for_each_option(svn_config_t *cfg, void *baton, apr_pool_t *pool,
svn_boolean_t callback(void *same_baton,
cfg_section_t *section,
cfg_option_t *option)) {
apr_hash_index_t *sec_ndx;
for (sec_ndx = apr_hash_first(pool, cfg->sections);
sec_ndx != NULL;
sec_ndx = apr_hash_next(sec_ndx)) {
void *sec_ptr;
cfg_section_t *sec;
apr_hash_index_t *opt_ndx;
apr_hash_this(sec_ndx, NULL, NULL, &sec_ptr);
sec = sec_ptr;
for (opt_ndx = apr_hash_first(pool, sec->options);
opt_ndx != NULL;
opt_ndx = apr_hash_next(opt_ndx)) {
void *opt_ptr;
cfg_option_t *opt;
apr_hash_this(opt_ndx, NULL, NULL, &opt_ptr);
opt = opt_ptr;
if (callback(baton, sec, opt))
return;
}
}
}
static svn_boolean_t
merge_callback(void *baton, cfg_section_t *section, cfg_option_t *option) {
svn_config_set(baton, section->name, option->name, option->value);
return FALSE;
}
svn_error_t *
svn_config_merge(svn_config_t *cfg, const char *file,
svn_boolean_t must_exist) {
svn_config_t *merge_cfg;
SVN_ERR(svn_config_read(&merge_cfg, file, must_exist, cfg->pool));
for_each_option(merge_cfg, cfg, merge_cfg->pool, merge_callback);
return SVN_NO_ERROR;
}
static svn_boolean_t
rmex_callback(void *baton, cfg_section_t *section, cfg_option_t *option) {
if (option->expanded && option->x_value != NULL) {
option->x_value = NULL;
option->expanded = FALSE;
}
return FALSE;
}
static void
remove_expansions(svn_config_t *cfg) {
if (!cfg->x_values)
return;
for_each_option(cfg, NULL, cfg->x_pool, rmex_callback);
svn_pool_clear(cfg->x_pool);
cfg->x_values = FALSE;
}
static APR_INLINE char *
make_hash_key(char *key) {
register char *p;
for (p = key; *p != 0; ++p)
*p = apr_tolower(*p);
return key;
}
static cfg_option_t *
find_option(svn_config_t *cfg, const char *section, const char *option,
cfg_section_t **sectionp) {
void *sec_ptr;
svn_stringbuf_set(cfg->tmp_key, section);
make_hash_key(cfg->tmp_key->data);
sec_ptr = apr_hash_get(cfg->sections, cfg->tmp_key->data,
cfg->tmp_key->len);
if (sectionp != NULL)
*sectionp = sec_ptr;
if (sec_ptr != NULL && option != NULL) {
cfg_section_t *sec = sec_ptr;
cfg_option_t *opt;
svn_stringbuf_set(cfg->tmp_key, option);
make_hash_key(cfg->tmp_key->data);
opt = apr_hash_get(sec->options, cfg->tmp_key->data,
cfg->tmp_key->len);
if (opt == NULL
&& apr_strnatcasecmp(section, SVN_CONFIG__DEFAULT_SECTION) != 0)
opt = find_option(cfg, SVN_CONFIG__DEFAULT_SECTION, option, &sec);
return opt;
}
return NULL;
}
static void
expand_option_value(svn_config_t *cfg, cfg_section_t *section,
const char *opt_value, const char **opt_x_valuep,
apr_pool_t *x_pool);
static void
make_string_from_option(const char **valuep, svn_config_t *cfg,
cfg_section_t *section, cfg_option_t *opt,
apr_pool_t* x_pool) {
if (!opt->expanded) {
apr_pool_t *tmp_pool = (x_pool ? x_pool : svn_pool_create(cfg->x_pool));
expand_option_value(cfg, section, opt->value, &opt->x_value, tmp_pool);
opt->expanded = TRUE;
if (!x_pool) {
if (opt->x_value)
opt->x_value = apr_pstrmemdup(cfg->x_pool, opt->x_value,
strlen(opt->x_value));
svn_pool_destroy(tmp_pool);
}
}
if (opt->x_value)
*valuep = opt->x_value;
else
*valuep = opt->value;
}
#define FMT_START "%("
#define FMT_START_LEN (sizeof(FMT_START) - 1)
#define FMT_END ")s"
#define FMT_END_LEN (sizeof(FMT_END) - 1)
static void
expand_option_value(svn_config_t *cfg, cfg_section_t *section,
const char *opt_value, const char **opt_x_valuep,
apr_pool_t *x_pool) {
svn_stringbuf_t *buf = NULL;
const char *parse_from = opt_value;
const char *copy_from = parse_from;
const char *name_start, *name_end;
while (parse_from != NULL
&& *parse_from != '\0'
&& (name_start = strstr(parse_from, FMT_START)) != NULL) {
name_start += FMT_START_LEN;
if (*name_start == '\0')
break;
name_end = strstr(name_start, FMT_END);
if (name_end != NULL) {
cfg_option_t *x_opt;
apr_size_t len = name_end - name_start;
char *name = apr_pstrmemdup(x_pool, name_start, len);
x_opt = find_option(cfg, section->name, name, NULL);
if (x_opt != NULL) {
const char *cstring;
make_string_from_option(&cstring, cfg, section, x_opt, x_pool);
len = name_start - FMT_START_LEN - copy_from;
if (buf == NULL) {
buf = svn_stringbuf_ncreate(copy_from, len, x_pool);
cfg->x_values = TRUE;
} else
svn_stringbuf_appendbytes(buf, copy_from, len);
svn_stringbuf_appendcstr(buf, cstring);
parse_from = name_end + FMT_END_LEN;
copy_from = parse_from;
} else
parse_from = name_end + FMT_END_LEN;
} else
parse_from = NULL;
}
if (buf != NULL) {
svn_stringbuf_appendcstr(buf, copy_from);
*opt_x_valuep = buf->data;
} else
*opt_x_valuep = NULL;
}
void
svn_config_get(svn_config_t *cfg, const char **valuep,
const char *section, const char *option,
const char *default_value) {
if (cfg) {
cfg_section_t *sec;
cfg_option_t *opt = find_option(cfg, section, option, &sec);
if (opt != NULL) {
make_string_from_option(valuep, cfg, sec, opt, NULL);
} else {
apr_pool_t *tmp_pool = svn_pool_create(cfg->x_pool);
const char *x_default;
expand_option_value(cfg, sec, default_value, &x_default, tmp_pool);
if (x_default) {
svn_stringbuf_set(cfg->tmp_value, x_default);
*valuep = cfg->tmp_value->data;
} else
*valuep = default_value;
svn_pool_destroy(tmp_pool);
}
} else {
*valuep = default_value;
}
}
void
svn_config_set(svn_config_t *cfg,
const char *section, const char *option,
const char *value) {
cfg_section_t *sec;
cfg_option_t *opt;
remove_expansions(cfg);
opt = find_option(cfg, section, option, &sec);
if (opt != NULL) {
opt->value = apr_pstrdup(cfg->pool, value);
opt->expanded = FALSE;
return;
}
opt = apr_palloc(cfg->pool, sizeof(*opt));
opt->name = apr_pstrdup(cfg->pool, option);
opt->hash_key = make_hash_key(apr_pstrdup(cfg->pool, option));
opt->value = apr_pstrdup(cfg->pool, value);
opt->x_value = NULL;
opt->expanded = FALSE;
if (sec == NULL) {
sec = apr_palloc(cfg->pool, sizeof(*sec));
sec->name = apr_pstrdup(cfg->pool, section);
sec->hash_key = make_hash_key(apr_pstrdup(cfg->pool, section));
sec->options = apr_hash_make(cfg->pool);
apr_hash_set(cfg->sections, sec->hash_key, APR_HASH_KEY_STRING, sec);
}
apr_hash_set(sec->options, opt->hash_key, APR_HASH_KEY_STRING, opt);
}
svn_error_t *
svn_config_get_bool(svn_config_t *cfg, svn_boolean_t *valuep,
const char *section, const char *option,
svn_boolean_t default_value) {
const char *tmp_value;
svn_config_get(cfg, &tmp_value, section, option, NULL);
if (tmp_value == NULL)
*valuep = default_value;
else if (0 == svn_cstring_casecmp(tmp_value, SVN_CONFIG_TRUE)
|| 0 == svn_cstring_casecmp(tmp_value, "yes")
|| 0 == svn_cstring_casecmp(tmp_value, "on")
|| 0 == strcmp(tmp_value, "1"))
*valuep = TRUE;
else if (0 == svn_cstring_casecmp(tmp_value, SVN_CONFIG_FALSE)
|| 0 == svn_cstring_casecmp(tmp_value, "no")
|| 0 == svn_cstring_casecmp(tmp_value, "off")
|| 0 == strcmp(tmp_value, "0"))
*valuep = FALSE;
else
return svn_error_createf(SVN_ERR_RA_DAV_INVALID_CONFIG_VALUE, NULL,
_("Config error: invalid boolean value '%s'"),
tmp_value);
return SVN_NO_ERROR;
}
void
svn_config_set_bool(svn_config_t *cfg,
const char *section, const char *option,
svn_boolean_t value) {
svn_config_set(cfg, section, option,
(value ? SVN_CONFIG_TRUE : SVN_CONFIG_FALSE));
}
int
svn_config__enumerate_sections(svn_config_t *cfg,
svn_config__section_enumerator_t callback,
void *baton) {
return svn_config_enumerate_sections(cfg,
(svn_config_section_enumerator_t) callback, baton);
}
int
svn_config_enumerate_sections(svn_config_t *cfg,
svn_config_section_enumerator_t callback,
void *baton) {
apr_hash_index_t *sec_ndx;
int count = 0;
apr_pool_t *subpool = svn_pool_create(cfg->x_pool);
for (sec_ndx = apr_hash_first(subpool, cfg->sections);
sec_ndx != NULL;
sec_ndx = apr_hash_next(sec_ndx)) {
void *sec_ptr;
cfg_section_t *sec;
apr_hash_this(sec_ndx, NULL, NULL, &sec_ptr);
sec = sec_ptr;
++count;
if (!callback(sec->name, baton))
break;
}
svn_pool_destroy(subpool);
return count;
}
int
svn_config_enumerate_sections2(svn_config_t *cfg,
svn_config_section_enumerator2_t callback,
void *baton, apr_pool_t *pool) {
apr_hash_index_t *sec_ndx;
apr_pool_t *iteration_pool;
int count = 0;
iteration_pool = svn_pool_create(pool);
for (sec_ndx = apr_hash_first(pool, cfg->sections);
sec_ndx != NULL;
sec_ndx = apr_hash_next(sec_ndx)) {
void *sec_ptr;
cfg_section_t *sec;
apr_hash_this(sec_ndx, NULL, NULL, &sec_ptr);
sec = sec_ptr;
++count;
svn_pool_clear(iteration_pool);
if (!callback(sec->name, baton, iteration_pool))
break;
}
svn_pool_destroy(iteration_pool);
return count;
}
int
svn_config_enumerate(svn_config_t *cfg, const char *section,
svn_config_enumerator_t callback, void *baton) {
cfg_section_t *sec;
apr_hash_index_t *opt_ndx;
int count;
apr_pool_t *subpool;
find_option(cfg, section, NULL, &sec);
if (sec == NULL)
return 0;
subpool = svn_pool_create(cfg->x_pool);
count = 0;
for (opt_ndx = apr_hash_first(subpool, sec->options);
opt_ndx != NULL;
opt_ndx = apr_hash_next(opt_ndx)) {
void *opt_ptr;
cfg_option_t *opt;
const char *temp_value;
apr_hash_this(opt_ndx, NULL, NULL, &opt_ptr);
opt = opt_ptr;
++count;
make_string_from_option(&temp_value, cfg, sec, opt, NULL);
if (!callback(opt->name, temp_value, baton))
break;
}
svn_pool_destroy(subpool);
return count;
}
int
svn_config_enumerate2(svn_config_t *cfg, const char *section,
svn_config_enumerator2_t callback, void *baton,
apr_pool_t *pool) {
cfg_section_t *sec;
apr_hash_index_t *opt_ndx;
apr_pool_t *iteration_pool;
int count;
find_option(cfg, section, NULL, &sec);
if (sec == NULL)
return 0;
iteration_pool = svn_pool_create(pool);
count = 0;
for (opt_ndx = apr_hash_first(pool, sec->options);
opt_ndx != NULL;
opt_ndx = apr_hash_next(opt_ndx)) {
void *opt_ptr;
cfg_option_t *opt;
const char *temp_value;
apr_hash_this(opt_ndx, NULL, NULL, &opt_ptr);
opt = opt_ptr;
++count;
make_string_from_option(&temp_value, cfg, sec, opt, NULL);
svn_pool_clear(iteration_pool);
if (!callback(opt->name, temp_value, baton, iteration_pool))
break;
}
svn_pool_destroy(iteration_pool);
return count;
}
struct search_groups_baton {
const char *key;
const char *match;
apr_pool_t *pool;
};
static svn_boolean_t search_groups(const char *name,
const char *value,
void *baton,
apr_pool_t *pool) {
struct search_groups_baton *b = baton;
apr_array_header_t *list;
list = svn_cstring_split(value, ",", TRUE, pool);
if (svn_cstring_match_glob_list(b->key, list)) {
b->match = apr_pstrdup(b->pool, name);
return FALSE;
} else
return TRUE;
}
const char *svn_config_find_group(svn_config_t *cfg, const char *key,
const char *master_section,
apr_pool_t *pool) {
struct search_groups_baton gb;
gb.key = key;
gb.match = NULL;
gb.pool = pool;
svn_config_enumerate2(cfg, master_section, search_groups, &gb, pool);
return gb.match;
}
const char*
svn_config_get_server_setting(svn_config_t *cfg,
const char* server_group,
const char* option_name,
const char* default_value) {
const char *retval;
svn_config_get(cfg, &retval, SVN_CONFIG_SECTION_GLOBAL,
option_name, default_value);
if (server_group) {
svn_config_get(cfg, &retval, server_group, option_name, retval);
}
return retval;
}
svn_error_t*
svn_config_get_server_setting_int(svn_config_t *cfg,
const char *server_group,
const char *option_name,
apr_int64_t default_value,
apr_int64_t *result_value,
apr_pool_t *pool) {
const char* tmp_value;
char *end_pos;
tmp_value = svn_config_get_server_setting(cfg, server_group,
option_name, NULL);
if (tmp_value == NULL)
*result_value = default_value;
else {
*result_value = apr_strtoi64(tmp_value, &end_pos, 0);
if (*end_pos != 0) {
return svn_error_createf
(SVN_ERR_RA_DAV_INVALID_CONFIG_VALUE, NULL,
_("Config error: invalid integer value '%s'"),
tmp_value);
}
}
return SVN_NO_ERROR;
}
svn_boolean_t
svn_config_has_section(svn_config_t *cfg, const char *section) {
return apr_hash_get(cfg->sections, section, APR_HASH_KEY_STRING) != NULL;
}