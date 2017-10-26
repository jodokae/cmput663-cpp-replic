#include "bdb_compat.h"
int
svn_fs_bdb__check_version(void) {
int major, minor;
db_version(&major, &minor, NULL);
if (major != DB_VERSION_MAJOR || minor != DB_VERSION_MINOR)
return DB_OLD_VERSION;
return 0;
}
