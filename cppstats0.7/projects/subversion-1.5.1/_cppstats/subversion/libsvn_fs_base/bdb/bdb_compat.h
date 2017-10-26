#if !defined(SVN_LIBSVN_FS_BDB_COMPAT_H)
#define SVN_LIBSVN_FS_BDB_COMPAT_H
#define APU_WANT_DB
#include <apu_want.h>
#if defined(__cplusplus)
extern "C" {
#endif
#if defined(DB_AUTO_COMMIT)
#define SVN_BDB_AUTO_COMMIT (DB_AUTO_COMMIT)
#else
#define SVN_BDB_AUTO_COMMIT (0)
#endif
#if defined(DB_INCOMPLETE)
#define SVN_BDB_HAS_DB_INCOMPLETE 1
#else
#define SVN_BDB_HAS_DB_INCOMPLETE 0
#endif
#if defined(DB_BUFFER_SMALL)
#define SVN_BDB_DB_BUFFER_SMALL DB_BUFFER_SMALL
#else
#define SVN_BDB_DB_BUFFER_SMALL ENOMEM
#endif
#if defined(DB_REGISTER)
#define SVN_BDB_AUTO_RECOVER (DB_REGISTER | DB_RECOVER)
#else
#define SVN_BDB_AUTO_RECOVER (0)
#endif
#define SVN_BDB_VERSION_AT_LEAST(major,minor) (DB_VERSION_MAJOR > (major) || (DB_VERSION_MAJOR == (major) && DB_VERSION_MINOR >= (minor)))
#if SVN_BDB_VERSION_AT_LEAST(4,1)
#define SVN_BDB_OPEN_PARAMS(env,txn) (env), (txn)
#else
#define SVN_BDB_OPEN_PARAMS(env,txn) (env)
#endif
#if SVN_BDB_VERSION_AT_LEAST(4,3)
#define SVN_BDB_ERROR_GATHERER_IGNORE(varname) ((void)(varname))
#else
#define bdb_error_gatherer(param1, param2, param3) bdb_error_gatherer(param2, char *msg)
#define SVN_BDB_ERROR_GATHERER_IGNORE(varname) ((void)0)
#endif
#if defined(WIN32) && SVN_BDB_VERSION_AT_LEAST(4,3)
#define SVN_BDB_PATH_UTF8 (1)
#else
#define SVN_BDB_PATH_UTF8 (0)
#endif
#if SVN_BDB_VERSION_AT_LEAST(4,6)
#define svn_bdb_dbc_close(c) ((c)->close(c))
#define svn_bdb_dbc_count(c,r,f) ((c)->count(c,r,f))
#define svn_bdb_dbc_del(c,f) ((c)->del(c,f))
#define svn_bdb_dbc_dup(c,p,f) ((c)->dup(c,p,f))
#define svn_bdb_dbc_get(c,k,d,f) ((c)->get(c,k,d,f))
#define svn_bdb_dbc_pget(c,k,p,d,f) ((c)->pget(c,k,p,d,f))
#define svn_bdb_dbc_put(c,k,d,f) ((c)->put(c,k,d,f))
#else
#define svn_bdb_dbc_close(c) ((c)->c_close(c))
#define svn_bdb_dbc_count(c,r,f) ((c)->c_count(c,r,f))
#define svn_bdb_dbc_del(c,f) ((c)->c_del(c,f))
#define svn_bdb_dbc_dup(c,p,f) ((c)->c_dup(c,p,f))
#define svn_bdb_dbc_get(c,k,d,f) ((c)->c_get(c,k,d,f))
#define svn_bdb_dbc_pget(c,k,p,d,f) ((c)->c_pget(c,k,p,d,f))
#define svn_bdb_dbc_put(c,k,d,f) ((c)->c_put(c,k,d,f))
#endif
int svn_fs_bdb__check_version(void);
#if defined(__cplusplus)
}
#endif
#endif
