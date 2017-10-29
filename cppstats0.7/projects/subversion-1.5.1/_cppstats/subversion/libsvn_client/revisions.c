#include <apr_pools.h>
#include "svn_error.h"
#include "svn_ra.h"
#include "svn_path.h"
#include "client.h"
#include "svn_private_config.h"
#include "private/svn_wc_private.h"
svn_error_t *
svn_client__get_revision_number(svn_revnum_t *revnum,
svn_revnum_t *youngest_rev,
svn_ra_session_t *ra_session,
const svn_opt_revision_t *revision,
const char *path,
apr_pool_t *pool) {
switch (revision->kind) {
case svn_opt_revision_unspecified:
*revnum = SVN_INVALID_REVNUM;
break;
case svn_opt_revision_number:
*revnum = revision->value.number;
break;
case svn_opt_revision_head:
if (youngest_rev && SVN_IS_VALID_REVNUM(*youngest_rev)) {
*revnum = *youngest_rev;
} else {
if (! ra_session)
return svn_error_create(SVN_ERR_CLIENT_RA_ACCESS_REQUIRED,
NULL, NULL);
SVN_ERR(svn_ra_get_latest_revnum(ra_session, revnum, pool));
if (youngest_rev)
*youngest_rev = *revnum;
}
break;
case svn_opt_revision_committed:
case svn_opt_revision_working:
case svn_opt_revision_base:
case svn_opt_revision_previous: {
svn_wc_adm_access_t *adm_access;
const svn_wc_entry_t *ent;
if (path == NULL)
return svn_error_create(SVN_ERR_CLIENT_VERSIONED_PATH_REQUIRED,
NULL, NULL);
SVN_ERR(svn_wc_adm_probe_open3(&adm_access, NULL, path, FALSE,
0, NULL, NULL, pool));
SVN_ERR(svn_wc__entry_versioned(&ent, path, adm_access, FALSE, pool));
SVN_ERR(svn_wc_adm_close(adm_access));
if ((revision->kind == svn_opt_revision_base)
|| (revision->kind == svn_opt_revision_working)) {
*revnum = ent->revision;
} else {
if (! SVN_IS_VALID_REVNUM(ent->cmt_rev))
return svn_error_createf(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Path '%s' has no committed "
"revision"), path);
*revnum = ent->cmt_rev;
if (revision->kind == svn_opt_revision_previous)
(*revnum)--;
}
}
break;
case svn_opt_revision_date:
if (! ra_session)
return svn_error_create(SVN_ERR_CLIENT_RA_ACCESS_REQUIRED, NULL, NULL);
SVN_ERR(svn_ra_get_dated_revision(ra_session, revnum,
revision->value.date, pool));
break;
default:
return svn_error_createf(SVN_ERR_CLIENT_BAD_REVISION, NULL,
_("Unrecognized revision type requested for "
"'%s'"),
svn_path_local_style(path, pool));
}
if (youngest_rev
&& SVN_IS_VALID_REVNUM(*youngest_rev)
&& SVN_IS_VALID_REVNUM(*revnum)
&& (*revnum > *youngest_rev))
*revnum = *youngest_rev;
return SVN_NO_ERROR;
}
svn_boolean_t
svn_client__compare_revisions(svn_opt_revision_t *revision1,
svn_opt_revision_t *revision2) {
if ((revision1->kind != revision2->kind)
|| ((revision1->kind == svn_opt_revision_number)
&& (revision1->value.number != revision2->value.number))
|| ((revision1->kind == svn_opt_revision_date)
&& (revision1->value.date != revision2->value.date)))
return FALSE;
return TRUE;
}
svn_boolean_t
svn_client__revision_is_local(const svn_opt_revision_t *revision) {
if ((revision->kind == svn_opt_revision_unspecified)
|| (revision->kind == svn_opt_revision_head)
|| (revision->kind == svn_opt_revision_number)
|| (revision->kind == svn_opt_revision_date))
return FALSE;
else
return TRUE;
}
