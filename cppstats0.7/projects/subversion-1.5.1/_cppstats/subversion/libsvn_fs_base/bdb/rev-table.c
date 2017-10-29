#include "bdb_compat.h"
#include "svn_fs.h"
#include "../fs.h"
#include "../err.h"
#include "../util/skel.h"
#include "../util/fs_skels.h"
#include "../../libsvn_fs/fs-loader.h"
#include "bdb-err.h"
#include "dbt.h"
#include "rev-table.h"
#include "svn_private_config.h"
#include "private/svn_fs_util.h"
int svn_fs_bdb__open_revisions_table(DB **revisions_p,
DB_ENV *env,
svn_boolean_t create) {
const u_int32_t open_flags = (create ? (DB_CREATE | DB_EXCL) : 0);
DB *revisions;
BDB_ERR(svn_fs_bdb__check_version());
BDB_ERR(db_create(&revisions, env, 0));
BDB_ERR((revisions->open)(SVN_BDB_OPEN_PARAMS(revisions, NULL),
"revisions", 0, DB_RECNO,
open_flags, 0666));
*revisions_p = revisions;
return 0;
}
svn_error_t *
svn_fs_bdb__get_rev(revision_t **revision_p,
svn_fs_t *fs,
svn_revnum_t rev,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
int db_err;
DBT key, value;
skel_t *skel;
revision_t *revision;
db_recno_t recno = rev + 1;
svn_fs_base__trail_debug(trail, "revisions", "get");
db_err = bfd->revisions->get(bfd->revisions, trail->db_txn,
svn_fs_base__set_dbt(&key, &recno,
sizeof(recno)),
svn_fs_base__result_dbt(&value),
0);
svn_fs_base__track_dbt(&value, pool);
if (db_err == DB_NOTFOUND)
return svn_fs_base__err_dangling_rev(fs, rev);
SVN_ERR(BDB_WRAP(fs, _("reading filesystem revision"), db_err));
skel = svn_fs_base__parse_skel(value.data, value.size, pool);
if (! skel)
return svn_fs_base__err_corrupt_fs_revision(fs, rev);
SVN_ERR(svn_fs_base__parse_revision_skel(&revision, skel, pool));
*revision_p = revision;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__put_rev(svn_revnum_t *rev,
svn_fs_t *fs,
const revision_t *revision,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
int db_err;
db_recno_t recno = 0;
skel_t *skel;
DBT key, value;
SVN_ERR(svn_fs_base__unparse_revision_skel(&skel, revision, pool));
if (SVN_IS_VALID_REVNUM(*rev)) {
DBT query, result;
recno = *rev + 1;
svn_fs_base__trail_debug(trail, "revisions", "put");
db_err = bfd->revisions->put
(bfd->revisions, trail->db_txn,
svn_fs_base__set_dbt(&query, &recno, sizeof(recno)),
svn_fs_base__skel_to_dbt(&result, skel, pool), 0);
return BDB_WRAP(fs, "updating filesystem revision", db_err);
}
svn_fs_base__trail_debug(trail, "revisions", "put");
db_err = bfd->revisions->put(bfd->revisions, trail->db_txn,
svn_fs_base__recno_dbt(&key, &recno),
svn_fs_base__skel_to_dbt(&value, skel, pool),
DB_APPEND);
SVN_ERR(BDB_WRAP(fs, "storing filesystem revision", db_err));
*rev = recno - 1;
return SVN_NO_ERROR;
}
svn_error_t *
svn_fs_bdb__youngest_rev(svn_revnum_t *youngest_p,
svn_fs_t *fs,
trail_t *trail,
apr_pool_t *pool) {
base_fs_data_t *bfd = fs->fsap_data;
int db_err;
DBC *cursor = 0;
DBT key, value;
db_recno_t recno;
SVN_ERR(svn_fs__check_fs(fs, TRUE));
svn_fs_base__trail_debug(trail, "revisions", "cursor");
SVN_ERR(BDB_WRAP(fs, "getting youngest revision (creating cursor)",
bfd->revisions->cursor(bfd->revisions, trail->db_txn,
&cursor, 0)));
db_err = svn_bdb_dbc_get(cursor,
svn_fs_base__recno_dbt(&key, &recno),
svn_fs_base__nodata_dbt(&value),
DB_LAST);
if (db_err) {
svn_bdb_dbc_close(cursor);
if (db_err == DB_NOTFOUND)
return
svn_error_createf
(SVN_ERR_FS_CORRUPT, 0,
"Corrupt DB: revision 0 missing from 'revisions' table, in "
"filesystem '%s'", fs->path);
SVN_ERR(BDB_WRAP(fs, "getting youngest revision (finding last entry)",
db_err));
}
SVN_ERR(BDB_WRAP(fs, "getting youngest revision (closing cursor)",
svn_bdb_dbc_close(cursor)));
*youngest_p = recno - 1;
return SVN_NO_ERROR;
}
