#include <stdlib.h>
#if !defined(WIN32)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <apr_errno.h>
#include "svn_nls.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_path.h"
#include "svn_private_config.h"
#if defined(WIN32)
#include <arch/win32/apr_arch_utf8.h>
#endif
svn_error_t *
svn_nls_init(void) {
svn_error_t *err = SVN_NO_ERROR;
#if defined(ENABLE_NLS)
#if defined(WIN32)
{
WCHAR ucs2_path[MAX_PATH];
char* utf8_path;
const char* internal_path;
apr_pool_t* pool;
apr_status_t apr_err;
apr_size_t inwords, outbytes, outlength;
apr_pool_create(&pool, 0);
inwords = GetModuleFileNameW(0, ucs2_path,
sizeof(ucs2_path) / sizeof(ucs2_path[0]));
if (! inwords) {
CHAR ansi_path[MAX_PATH];
if (GetModuleFileNameA(0, ansi_path, sizeof(ansi_path))) {
inwords =
MultiByteToWideChar(CP_ACP, 0, ansi_path, -1, ucs2_path,
sizeof(ucs2_path) / sizeof(ucs2_path[0]));
if (! inwords) {
err =
svn_error_createf(APR_EINVAL, NULL,
_("Can't convert string to UCS-2: '%s'"),
ansi_path);
}
} else {
err = svn_error_create(APR_EINVAL, NULL,
_("Can't get module file name"));
}
}
if (! err) {
outbytes = outlength = 3 * (inwords + 1);
utf8_path = apr_palloc(pool, outlength);
apr_err = apr_conv_ucs2_to_utf8(ucs2_path, &inwords,
utf8_path, &outbytes);
if (!apr_err && (inwords > 0 || outbytes == 0))
apr_err = APR_INCOMPLETE;
if (apr_err) {
err = svn_error_createf(apr_err, NULL,
_("Can't convert module path "
"to UTF-8 from UCS-2: '%s'"),
ucs2_path);
} else {
utf8_path[outlength - outbytes] = '\0';
internal_path = svn_path_internal_style(utf8_path, pool);
internal_path = svn_path_dirname(internal_path, pool);
internal_path = svn_path_join(internal_path,
SVN_LOCALE_RELATIVE_PATH,
pool);
bindtextdomain(PACKAGE_NAME, internal_path);
}
}
svn_pool_destroy(pool);
}
#else
bindtextdomain(PACKAGE_NAME, SVN_LOCALE_DIR);
#if defined(HAVE_BIND_TEXTDOMAIN_CODESET)
bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
#endif
#endif
#endif
return err;
}