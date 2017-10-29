#if !defined(SVN_RA_SVN_H)
#define SVN_RA_SVN_H
#include <apr.h>
#include <apr_pools.h>
#include <apr_network_io.h>
#include "svn_config.h"
#include "svn_delta.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_RA_SVN_PORT 3690
#define SVN_RA_SVN_CAP_EDIT_PIPELINE "edit-pipeline"
#define SVN_RA_SVN_CAP_SVNDIFF1 "svndiff1"
#define SVN_RA_SVN_CAP_ABSENT_ENTRIES "absent-entries"
#define SVN_RA_SVN_CAP_COMMIT_REVPROPS "commit-revprops"
#define SVN_RA_SVN_CAP_MERGEINFO "mergeinfo"
#define SVN_RA_SVN_CAP_DEPTH "depth"
#define SVN_RA_SVN_CAP_LOG_REVPROPS "log-revprops"
#define SVN_RA_SVN_CAP_PARTIAL_REPLAY "partial-replay"
#define SVN_RA_SVN_DIRENT_KIND "kind"
#define SVN_RA_SVN_DIRENT_SIZE "size"
#define SVN_RA_SVN_DIRENT_HAS_PROPS "has-props"
#define SVN_RA_SVN_DIRENT_CREATED_REV "created-rev"
#define SVN_RA_SVN_DIRENT_TIME "time"
#define SVN_RA_SVN_DIRENT_LAST_AUTHOR "last-author"
#define SVN_RA_SVN_UNSPECIFIED_NUMBER ~((apr_uint64_t) 0)
#define SVN_CMD_ERR(expr) do { svn_error_t *svn_err__temp = (expr); if (svn_err__temp) return svn_error_create(SVN_ERR_RA_SVN_CMD_ERR, svn_err__temp, NULL); } while (0)
typedef struct svn_ra_svn_conn_st svn_ra_svn_conn_t;
typedef svn_error_t *(*svn_ra_svn_command_handler)(svn_ra_svn_conn_t *conn,
apr_pool_t *pool,
apr_array_header_t *params,
void *baton);
typedef struct svn_ra_svn_cmd_entry_t {
const char *cmdname;
svn_ra_svn_command_handler handler;
svn_boolean_t terminate;
} svn_ra_svn_cmd_entry_t;
typedef struct svn_ra_svn_item_t {
enum {
SVN_RA_SVN_NUMBER,
SVN_RA_SVN_STRING,
SVN_RA_SVN_WORD,
SVN_RA_SVN_LIST
} kind;
union {
apr_uint64_t number;
svn_string_t *string;
const char *word;
apr_array_header_t *list;
} u;
} svn_ra_svn_item_t;
typedef svn_error_t *(*svn_ra_svn_edit_callback)(void *baton);
svn_ra_svn_conn_t *svn_ra_svn_create_conn(apr_socket_t *sock,
apr_file_t *in_file,
apr_file_t *out_file,
apr_pool_t *pool);
svn_error_t *svn_ra_svn_set_capabilities(svn_ra_svn_conn_t *conn,
apr_array_header_t *list);
svn_boolean_t svn_ra_svn_has_capability(svn_ra_svn_conn_t *conn,
const char *capability);
svn_error_t *svn_ra_svn_write_number(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
apr_uint64_t number);
svn_error_t *svn_ra_svn_write_string(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
const svn_string_t *str);
svn_error_t *svn_ra_svn_write_cstring(svn_ra_svn_conn_t *conn,
apr_pool_t *pool, const char *s);
svn_error_t *svn_ra_svn_write_word(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
const char *word);
svn_error_t *svn_ra_svn_write_proplist(svn_ra_svn_conn_t *conn,
apr_pool_t *pool,
apr_hash_t *props);
svn_error_t *svn_ra_svn_start_list(svn_ra_svn_conn_t *conn, apr_pool_t *pool);
svn_error_t *svn_ra_svn_end_list(svn_ra_svn_conn_t *conn, apr_pool_t *pool);
svn_error_t *svn_ra_svn_flush(svn_ra_svn_conn_t *conn, apr_pool_t *pool);
svn_error_t *svn_ra_svn_write_tuple(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
const char *fmt, ...);
svn_error_t *svn_ra_svn_read_item(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
svn_ra_svn_item_t **item);
svn_error_t *svn_ra_svn_skip_leading_garbage(svn_ra_svn_conn_t *conn,
apr_pool_t *pool);
svn_error_t *svn_ra_svn_parse_tuple(apr_array_header_t *list,
apr_pool_t *pool,
const char *fmt, ...);
svn_error_t *svn_ra_svn_read_tuple(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
const char *fmt, ...);
svn_error_t *svn_ra_svn_parse_proplist(apr_array_header_t *list,
apr_pool_t *pool,
apr_hash_t **props);
svn_error_t *svn_ra_svn_read_cmd_response(svn_ra_svn_conn_t *conn,
apr_pool_t *pool,
const char *fmt, ...);
svn_error_t *svn_ra_svn_handle_commands(svn_ra_svn_conn_t *conn,
apr_pool_t *pool,
const svn_ra_svn_cmd_entry_t *commands,
void *baton);
svn_error_t *svn_ra_svn_write_cmd(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
const char *cmdname, const char *fmt, ...);
svn_error_t *svn_ra_svn_write_cmd_response(svn_ra_svn_conn_t *conn,
apr_pool_t *pool,
const char *fmt, ...);
svn_error_t *svn_ra_svn_write_cmd_failure(svn_ra_svn_conn_t *conn,
apr_pool_t *pool, svn_error_t *err);
void svn_ra_svn_get_editor(const svn_delta_editor_t **editor,
void **edit_baton, svn_ra_svn_conn_t *conn,
apr_pool_t *pool, svn_ra_svn_edit_callback callback,
void *callback_baton);
svn_error_t *svn_ra_svn_drive_editor2(svn_ra_svn_conn_t *conn,
apr_pool_t *pool,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_boolean_t *aborted,
svn_boolean_t for_replay);
svn_error_t *svn_ra_svn_drive_editor(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
const svn_delta_editor_t *editor,
void *edit_baton,
svn_boolean_t *aborted);
svn_error_t *svn_ra_svn_cram_server(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
svn_config_t *pwdb, const char **user,
svn_boolean_t *success);
const svn_version_t *svn_ra_svn_version(void);
#if defined(__cplusplus)
}
#endif
#endif