#include <apr_lib.h>
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include "svn_error.h"
#include "svn_private_config.h"
svn_error_t *
svn_mime_type_validate(const char *mime_type, apr_pool_t *pool) {
const apr_size_t len = strcspn(mime_type, "; ");
const char *const slash_pos = strchr(mime_type, '/');
if (len == 0)
return svn_error_createf
(SVN_ERR_BAD_MIME_TYPE, NULL,
_("MIME type '%s' has empty media type"), mime_type);
if (slash_pos == NULL || slash_pos >= &mime_type[len])
return svn_error_createf
(SVN_ERR_BAD_MIME_TYPE, NULL,
_("MIME type '%s' does not contain '/'"), mime_type);
if (! apr_isalnum(mime_type[len - 1]))
return svn_error_createf
(SVN_ERR_BAD_MIME_TYPE, NULL,
_("MIME type '%s' ends with non-alphanumeric character"), mime_type);
return SVN_NO_ERROR;
}
svn_boolean_t
svn_mime_type_is_binary(const char *mime_type) {
const apr_size_t len = strcspn(mime_type, "; ");
return ((strncmp(mime_type, "text/", 5) != 0)
&& (len != 15 || strncmp(mime_type, "image/x-xbitmap", len) != 0)
&& (len != 15 || strncmp(mime_type, "image/x-xpixmap", len) != 0)
);
}
