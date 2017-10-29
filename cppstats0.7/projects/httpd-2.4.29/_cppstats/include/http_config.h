#if !defined(APACHE_HTTP_CONFIG_H)
#define APACHE_HTTP_CONFIG_H
#include "util_cfgtree.h"
#include "ap_config.h"
#include "apr_tables.h"
#if defined(__cplusplus)
extern "C" {
#endif
enum cmd_how {
RAW_ARGS,
TAKE1,
TAKE2,
ITERATE,
ITERATE2,
FLAG,
NO_ARGS,
TAKE12,
TAKE3,
TAKE23,
TAKE123,
TAKE13,
TAKE_ARGV
};
typedef struct cmd_parms_struct cmd_parms;
#if defined(AP_HAVE_DESIGNATED_INITIALIZER) || defined(DOXYGEN)
typedef union {
const char *(*no_args) (cmd_parms *parms, void *mconfig);
const char *(*raw_args) (cmd_parms *parms, void *mconfig,
const char *args);
const char *(*take_argv) (cmd_parms *parms, void *mconfig,
int argc, char *const argv[]);
const char *(*take1) (cmd_parms *parms, void *mconfig, const char *w);
const char *(*take2) (cmd_parms *parms, void *mconfig, const char *w,
const char *w2);
const char *(*take3) (cmd_parms *parms, void *mconfig, const char *w,
const char *w2, const char *w3);
const char *(*flag) (cmd_parms *parms, void *mconfig, int on);
} cmd_func;
#define AP_NO_ARGS func.no_args
#define AP_RAW_ARGS func.raw_args
#define AP_TAKE_ARGV func.take_argv
#define AP_TAKE1 func.take1
#define AP_TAKE2 func.take2
#define AP_TAKE3 func.take3
#define AP_FLAG func.flag
#define AP_INIT_NO_ARGS(directive, func, mconfig, where, help) { directive, { .no_args=func }, mconfig, where, RAW_ARGS, help }
#define AP_INIT_RAW_ARGS(directive, func, mconfig, where, help) { directive, { .raw_args=func }, mconfig, where, RAW_ARGS, help }
#define AP_INIT_TAKE_ARGV(directive, func, mconfig, where, help) { directive, { .take_argv=func }, mconfig, where, TAKE_ARGV, help }
#define AP_INIT_TAKE1(directive, func, mconfig, where, help) { directive, { .take1=func }, mconfig, where, TAKE1, help }
#define AP_INIT_ITERATE(directive, func, mconfig, where, help) { directive, { .take1=func }, mconfig, where, ITERATE, help }
#define AP_INIT_TAKE2(directive, func, mconfig, where, help) { directive, { .take2=func }, mconfig, where, TAKE2, help }
#define AP_INIT_TAKE12(directive, func, mconfig, where, help) { directive, { .take2=func }, mconfig, where, TAKE12, help }
#define AP_INIT_ITERATE2(directive, func, mconfig, where, help) { directive, { .take2=func }, mconfig, where, ITERATE2, help }
#define AP_INIT_TAKE13(directive, func, mconfig, where, help) { directive, { .take3=func }, mconfig, where, TAKE13, help }
#define AP_INIT_TAKE23(directive, func, mconfig, where, help) { directive, { .take3=func }, mconfig, where, TAKE23, help }
#define AP_INIT_TAKE123(directive, func, mconfig, where, help) { directive, { .take3=func }, mconfig, where, TAKE123, help }
#define AP_INIT_TAKE3(directive, func, mconfig, where, help) { directive, { .take3=func }, mconfig, where, TAKE3, help }
#define AP_INIT_FLAG(directive, func, mconfig, where, help) { directive, { .flag=func }, mconfig, where, FLAG, help }
#else
typedef const char *(*cmd_func) ();
#define AP_NO_ARGS func
#define AP_RAW_ARGS func
#define AP_TAKE_ARGV func
#define AP_TAKE1 func
#define AP_TAKE2 func
#define AP_TAKE3 func
#define AP_FLAG func
#define AP_INIT_NO_ARGS(directive, func, mconfig, where, help) { directive, func, mconfig, where, RAW_ARGS, help }
#define AP_INIT_RAW_ARGS(directive, func, mconfig, where, help) { directive, func, mconfig, where, RAW_ARGS, help }
#define AP_INIT_TAKE_ARGV(directive, func, mconfig, where, help) { directive, func, mconfig, where, TAKE_ARGV, help }
#define AP_INIT_TAKE1(directive, func, mconfig, where, help) { directive, func, mconfig, where, TAKE1, help }
#define AP_INIT_ITERATE(directive, func, mconfig, where, help) { directive, func, mconfig, where, ITERATE, help }
#define AP_INIT_TAKE2(directive, func, mconfig, where, help) { directive, func, mconfig, where, TAKE2, help }
#define AP_INIT_TAKE12(directive, func, mconfig, where, help) { directive, func, mconfig, where, TAKE12, help }
#define AP_INIT_ITERATE2(directive, func, mconfig, where, help) { directive, func, mconfig, where, ITERATE2, help }
#define AP_INIT_TAKE13(directive, func, mconfig, where, help) { directive, func, mconfig, where, TAKE13, help }
#define AP_INIT_TAKE23(directive, func, mconfig, where, help) { directive, func, mconfig, where, TAKE23, help }
#define AP_INIT_TAKE123(directive, func, mconfig, where, help) { directive, func, mconfig, where, TAKE123, help }
#define AP_INIT_TAKE3(directive, func, mconfig, where, help) { directive, func, mconfig, where, TAKE3, help }
#define AP_INIT_FLAG(directive, func, mconfig, where, help) { directive, func, mconfig, where, FLAG, help }
#endif
typedef struct command_struct command_rec;
struct command_struct {
const char *name;
cmd_func func;
void *cmd_data;
int req_override;
enum cmd_how args_how;
const char *errmsg;
};
#define OR_NONE 0
#define OR_LIMIT 1
#define OR_OPTIONS 2
#define OR_FILEINFO 4
#define OR_AUTHCFG 8
#define OR_INDEXES 16
#define OR_UNSET 32
#define ACCESS_CONF 64
#define RSRC_CONF 128
#define EXEC_ON_READ 256
#define NONFATAL_OVERRIDE 512
#define NONFATAL_UNKNOWN 1024
#define NONFATAL_ALL (NONFATAL_OVERRIDE|NONFATAL_UNKNOWN)
#define OR_ALL (OR_LIMIT|OR_OPTIONS|OR_FILEINFO|OR_AUTHCFG|OR_INDEXES)
#define DECLINE_CMD "\a\b"
typedef struct ap_configfile_t ap_configfile_t;
struct ap_configfile_t {
apr_status_t (*getch) (char *ch, void *param);
apr_status_t (*getstr) (void *buf, apr_size_t bufsiz, void *param);
apr_status_t (*close) (void *param);
void *param;
const char *name;
unsigned line_number;
};
struct cmd_parms_struct {
void *info;
int override;
int override_opts;
apr_table_t *override_list;
apr_int64_t limited;
apr_array_header_t *limited_xmethods;
ap_method_list_t *xlimited;
ap_configfile_t *config_file;
ap_directive_t *directive;
apr_pool_t *pool;
apr_pool_t *temp_pool;
server_rec *server;
char *path;
const command_rec *cmd;
struct ap_conf_vector_t *context;
const ap_directive_t *err_directive;
};
typedef struct module_struct module;
struct module_struct {
int version;
int minor_version;
int module_index;
const char *name;
void *dynamic_load_handle;
struct module_struct *next;
unsigned long magic;
void (*rewrite_args) (process_rec *process);
void *(*create_dir_config) (apr_pool_t *p, char *dir);
void *(*merge_dir_config) (apr_pool_t *p, void *base_conf, void *new_conf);
void *(*create_server_config) (apr_pool_t *p, server_rec *s);
void *(*merge_server_config) (apr_pool_t *p, void *base_conf,
void *new_conf);
const command_rec *cmds;
void (*register_hooks) (apr_pool_t *p);
};
#if defined(AP_MAYBE_UNUSED)
#elif defined(__GNUC__)
#define AP_MAYBE_UNUSED(x) x __attribute__((unused))
#elif defined(__LCLINT__)
#define AP_MAYBE_UNUSED(x) x
#else
#define AP_MAYBE_UNUSED(x) x
#endif
#define APLOG_USE_MODULE(foo) extern module AP_MODULE_DECLARE_DATA foo##_module; AP_MAYBE_UNUSED(static int * const aplog_module_index) = &(foo##_module.module_index)
#define AP_DECLARE_MODULE(foo) APLOG_USE_MODULE(foo); module AP_MODULE_DECLARE_DATA foo##_module
#define STANDARD_MODULE_STUFF this_module_needs_to_be_ported_to_apache_2_0
#define STANDARD20_MODULE_STUFF MODULE_MAGIC_NUMBER_MAJOR, MODULE_MAGIC_NUMBER_MINOR, -1, __FILE__, NULL, NULL, MODULE_MAGIC_COOKIE, NULL
#define MPM20_MODULE_STUFF MODULE_MAGIC_NUMBER_MAJOR, MODULE_MAGIC_NUMBER_MINOR, -1, __FILE__, NULL, NULL, MODULE_MAGIC_COOKIE
typedef struct ap_conf_vector_t ap_conf_vector_t;
AP_DECLARE(void *) ap_get_module_config(const ap_conf_vector_t *cv,
const module *m);
AP_DECLARE(void) ap_set_module_config(ap_conf_vector_t *cv, const module *m,
void *val);
#if !defined(AP_DEBUG)
#define ap_get_module_config(v,m) (((void **)(v))[(m)->module_index])
#define ap_set_module_config(v,m,val) ((((void **)(v))[(m)->module_index]) = (val))
#endif
AP_DECLARE(int) ap_get_server_module_loglevel(const server_rec *s, int index);
AP_DECLARE(int) ap_get_conn_module_loglevel(const conn_rec *c, int index);
AP_DECLARE(int) ap_get_conn_server_module_loglevel(const conn_rec *c,
const server_rec *s,
int index);
AP_DECLARE(int) ap_get_request_module_loglevel(const request_rec *r, int index);
AP_DECLARE(void) ap_set_module_loglevel(apr_pool_t *p, struct ap_logconf *l,
int index, int level);
#if !defined(AP_DEBUG)
#define ap_get_conn_logconf(c) ((c)->log ? (c)->log : &(c)->base_server->log)
#define ap_get_conn_server_logconf(c,s) ( ( (c)->log != &(c)->base_server->log && (c)->log != NULL ) ? (c)->log : &(s)->log )
#define ap_get_request_logconf(r) ((r)->log ? (r)->log : (r)->connection->log ? (r)->connection->log : &(r)->server->log)
#define ap_get_module_loglevel(l,i) (((i) < 0 || (l)->module_levels == NULL || (l)->module_levels[i] < 0) ? (l)->level : (l)->module_levels[i])
#define ap_get_server_module_loglevel(s,i) (ap_get_module_loglevel(&(s)->log,i))
#define ap_get_conn_module_loglevel(c,i) (ap_get_module_loglevel(ap_get_conn_logconf(c),i))
#define ap_get_conn_server_module_loglevel(c,s,i) (ap_get_module_loglevel(ap_get_conn_server_logconf(c,s),i))
#define ap_get_request_module_loglevel(r,i) (ap_get_module_loglevel(ap_get_request_logconf(r),i))
#endif
AP_DECLARE(void) ap_reset_module_loglevels(struct ap_logconf *l, int val);
AP_DECLARE_NONSTD(const char *) ap_set_string_slot(cmd_parms *cmd,
void *struct_ptr,
const char *arg);
AP_DECLARE_NONSTD(const char *) ap_set_int_slot(cmd_parms *cmd,
void *struct_ptr,
const char *arg);
AP_DECLARE(const char *) ap_parse_log_level(const char *str, int *val);
AP_DECLARE(int) ap_method_is_limited(cmd_parms *cmd, const char *method);
AP_DECLARE_NONSTD(const char *) ap_set_string_slot_lower(cmd_parms *cmd,
void *struct_ptr,
const char *arg);
AP_DECLARE_NONSTD(const char *) ap_set_flag_slot(cmd_parms *cmd,
void *struct_ptr,
int arg);
AP_DECLARE_NONSTD(const char *) ap_set_flag_slot_char(cmd_parms *cmd,
void *struct_ptr,
int arg);
AP_DECLARE_NONSTD(const char *) ap_set_file_slot(cmd_parms *cmd,
void *struct_ptr,
const char *arg);
AP_DECLARE_NONSTD(const char *) ap_set_deprecated(cmd_parms *cmd,
void *struct_ptr,
const char *arg);
AP_DECLARE(char *) ap_server_root_relative(apr_pool_t *p, const char *fname);
AP_DECLARE(char *) ap_runtime_dir_relative(apr_pool_t *p, const char *fname);
AP_DECLARE(const char *) ap_add_module(module *m, apr_pool_t *p,
const char *s);
AP_DECLARE(void) ap_remove_module(module *m);
AP_DECLARE(const char *) ap_add_loaded_module(module *mod, apr_pool_t *p,
const char *s);
AP_DECLARE(void) ap_remove_loaded_module(module *mod);
AP_DECLARE(const char *) ap_find_module_name(module *m);
AP_DECLARE(const char *) ap_find_module_short_name(int module_index);
AP_DECLARE(module *) ap_find_linked_module(const char *name);
AP_DECLARE(apr_status_t) ap_pcfg_openfile(ap_configfile_t **ret_cfg,
apr_pool_t *p, const char *name);
AP_DECLARE(ap_configfile_t *) ap_pcfg_open_custom(apr_pool_t *p,
const char *descr,
void *param,
apr_status_t (*getc_func) (char *ch, void *param),
apr_status_t (*gets_func) (void *buf, apr_size_t bufsiz, void *param),
apr_status_t (*close_func) (void *param));
AP_DECLARE(apr_status_t) ap_cfg_getline(char *buf, apr_size_t bufsize, ap_configfile_t *cfp);
AP_DECLARE(apr_status_t) ap_cfg_getc(char *ch, ap_configfile_t *cfp);
AP_DECLARE(int) ap_cfg_closefile(ap_configfile_t *cfp);
AP_DECLARE(const char *) ap_pcfg_strerror(apr_pool_t *p, ap_configfile_t *cfp,
apr_status_t rc);
AP_DECLARE(const char *) ap_soak_end_container(cmd_parms *cmd, char *directive);
AP_DECLARE(const char *) ap_build_cont_config(apr_pool_t *p,
apr_pool_t *temp_pool,
cmd_parms *parms,
ap_directive_t **current,
ap_directive_t **curr_parent,
char *orig_directive);
AP_DECLARE(const char *) ap_build_config(cmd_parms *parms,
apr_pool_t *conf_pool,
apr_pool_t *temp_pool,
ap_directive_t **conftree);
AP_DECLARE(const char *) ap_walk_config(ap_directive_t *conftree,
cmd_parms *parms,
ap_conf_vector_t *section_vector);
AP_DECLARE(const char *) ap_check_cmd_context(cmd_parms *cmd,
unsigned forbidden);
#define NOT_IN_VIRTUALHOST 0x01
#define NOT_IN_LIMIT 0x02
#define NOT_IN_DIRECTORY 0x04
#define NOT_IN_LOCATION 0x08
#define NOT_IN_FILES 0x10
#define NOT_IN_HTACCESS 0x20
#define NOT_IN_DIR_LOC_FILE (NOT_IN_DIRECTORY|NOT_IN_LOCATION|NOT_IN_FILES)
#define GLOBAL_ONLY (NOT_IN_VIRTUALHOST|NOT_IN_LIMIT|NOT_IN_DIR_LOC_FILE)
typedef struct {
const char *name;
module *modp;
} ap_module_symbol_t;
AP_DECLARE_DATA extern module *ap_top_module;
AP_DECLARE_DATA extern module *ap_prelinked_modules[];
AP_DECLARE_DATA extern ap_module_symbol_t ap_prelinked_module_symbols[];
AP_DECLARE_DATA extern module *ap_preloaded_modules[];
AP_DECLARE_DATA extern module **ap_loaded_modules;
AP_DECLARE(void) ap_single_module_configure(apr_pool_t *p, server_rec *s,
module *m);
AP_DECLARE(const char *) ap_setup_prelinked_modules(process_rec *process);
AP_DECLARE(void) ap_show_directives(void);
AP_DECLARE(void) ap_show_modules(void);
AP_DECLARE(const char *) ap_show_mpm(void);
AP_DECLARE(server_rec *) ap_read_config(process_rec *process,
apr_pool_t *temp_pool,
const char *config_name,
ap_directive_t **conftree);
AP_DECLARE(void) ap_run_rewrite_args(process_rec *process);
AP_DECLARE(void) ap_register_hooks(module *m, apr_pool_t *p);
AP_DECLARE(void) ap_fixup_virtual_hosts(apr_pool_t *p,
server_rec *main_server);
AP_DECLARE(void) ap_reserve_module_slots(int count);
AP_DECLARE(void) ap_reserve_module_slots_directive(const char *directive);
AP_DECLARE(ap_conf_vector_t*) ap_create_request_config(apr_pool_t *p);
AP_CORE_DECLARE(ap_conf_vector_t *) ap_create_per_dir_config(apr_pool_t *p);
AP_CORE_DECLARE(ap_conf_vector_t*) ap_merge_per_dir_configs(apr_pool_t *p,
ap_conf_vector_t *base,
ap_conf_vector_t *new_conf);
AP_DECLARE(struct ap_logconf *) ap_new_log_config(apr_pool_t *p,
const struct ap_logconf *old);
AP_DECLARE(void) ap_merge_log_config(const struct ap_logconf *old_conf,
struct ap_logconf *new_conf);
AP_CORE_DECLARE(ap_conf_vector_t*) ap_create_conn_config(apr_pool_t *p);
AP_CORE_DECLARE(int) ap_parse_htaccess(ap_conf_vector_t **result,
request_rec *r,
int override,
int override_opts,
apr_table_t *override_list,
const char *path,
const char *access_name);
AP_CORE_DECLARE(const char *) ap_init_virtual_host(apr_pool_t *p,
const char *hostname,
server_rec *main_server,
server_rec **ps);
AP_DECLARE(const char *) ap_process_resource_config(server_rec *s,
const char *fname,
ap_directive_t **conftree,
apr_pool_t *p,
apr_pool_t *ptemp);
AP_DECLARE(const char *) ap_process_fnmatch_configs(server_rec *s,
const char *fname,
ap_directive_t **conftree,
apr_pool_t *p,
apr_pool_t *ptemp,
int optional);
AP_DECLARE(int) ap_process_config_tree(server_rec *s,
ap_directive_t *conftree,
apr_pool_t *p,
apr_pool_t *ptemp);
AP_DECLARE(void *) ap_retained_data_create(const char *key, apr_size_t size);
AP_DECLARE(void *) ap_retained_data_get(const char *key);
AP_CORE_DECLARE(int) ap_invoke_handler(request_rec *r);
AP_CORE_DECLARE(const command_rec *) ap_find_command(const char *name,
const command_rec *cmds);
AP_CORE_DECLARE(const command_rec *) ap_find_command_in_modules(const char *cmd_name,
module **mod);
AP_CORE_DECLARE(void *) ap_set_config_vectors(server_rec *server,
ap_conf_vector_t *section_vector,
const char *section,
module *mod, apr_pool_t *pconf);
AP_DECLARE_HOOK(int,header_parser,(request_rec *r))
AP_DECLARE_HOOK(int,pre_config,(apr_pool_t *pconf,apr_pool_t *plog,
apr_pool_t *ptemp))
AP_DECLARE_HOOK(int,check_config,(apr_pool_t *pconf, apr_pool_t *plog,
apr_pool_t *ptemp, server_rec *s))
AP_DECLARE_HOOK(void,test_config,(apr_pool_t *pconf, server_rec *s))
AP_DECLARE_HOOK(int,post_config,(apr_pool_t *pconf,apr_pool_t *plog,
apr_pool_t *ptemp,server_rec *s))
AP_DECLARE_HOOK(int,open_logs,(apr_pool_t *pconf,apr_pool_t *plog,
apr_pool_t *ptemp,server_rec *s))
AP_DECLARE_HOOK(void,child_init,(apr_pool_t *pchild, server_rec *s))
AP_DECLARE_HOOK(int,handler,(request_rec *r))
AP_DECLARE_HOOK(int,quick_handler,(request_rec *r, int lookup_uri))
AP_DECLARE_HOOK(void,optional_fn_retrieve,(void))
AP_DECLARE_HOOK(apr_status_t,open_htaccess,
(request_rec *r, const char *dir_name, const char *access_name,
ap_configfile_t **conffile, const char **full_name))
apr_status_t ap_open_htaccess(request_rec *r, const char *dir_name,
const char *access_name, ap_configfile_t **conffile,
const char **full_name);
AP_DECLARE_NONSTD(apr_status_t) ap_pool_cleanup_set_null(void *data);
#if defined(__cplusplus)
}
#endif
#endif