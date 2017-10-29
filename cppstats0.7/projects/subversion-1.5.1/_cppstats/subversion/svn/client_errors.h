#if !defined(SVN_CLIENT_ERRORS_H)
#define SVN_CLIENT_ERRORS_H
#if defined(__cplusplus)
extern "C" {
#endif
#if defined(SVN_ERROR_BUILD_ARRAY) || !defined(SVN_CMDLINE_ERROR_ENUM_DEFINED)
#if defined(SVN_ERROR_BUILD_ARRAY)
#define SVN_ERROR_START static const err_defn error_table[] = { { SVN_ERR_CDMLINE__WARNING, "Warning" },
#define SVN_ERRDEF(n, s) { n, s },
#define SVN_ERROR_END { 0, NULL } };
#elif !defined(SVN_CMDLINE_ERROR_ENUM_DEFINED)
#define SVN_ERROR_START typedef enum svn_client_errno_t { SVN_ERR_CDMLINE__WARNING = SVN_ERR_LAST + 1,
#define SVN_ERRDEF(n, s) n,
#define SVN_ERROR_END SVN_ERR_CMDLINE__ERR_LAST } svn_client_errno_t;
#define SVN_CMDLINE_ERROR_ENUM_DEFINED
#endif
SVN_ERROR_START
SVN_ERRDEF(SVN_ERR_CMDLINE__TMPFILE_WRITE,
"Failed writing to temporary file.")
SVN_ERRDEF(SVN_ERR_CMDLINE__TMPFILE_STAT,
"Failed getting info about temporary file.")
SVN_ERRDEF(SVN_ERR_CMDLINE__TMPFILE_OPEN,
"Failed opening temporary file.")
SVN_ERROR_END
#undef SVN_ERROR_START
#undef SVN_ERRDEF
#undef SVN_ERROR_END
#endif
#if defined(__cplusplus)
}
#endif
#endif