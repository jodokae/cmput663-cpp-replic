#if !defined(SVN_DAV_H)
#define SVN_DAV_H
#if defined(__cplusplus)
extern "C" {
#endif
#define SVN_SVNDIFF_MIME_TYPE "application/vnd.svn-svndiff"
#define SVN_DAV_DELTA_BASE_HEADER "X-SVN-VR-Base"
#define SVN_DAV_OPTIONS_HEADER "X-SVN-Options"
#define SVN_DAV_OPTION_NO_MERGE_RESPONSE "no-merge-response"
#define SVN_DAV_OPTION_LOCK_BREAK "lock-break"
#define SVN_DAV_OPTION_LOCK_STEAL "lock-steal"
#define SVN_DAV_OPTION_RELEASE_LOCKS "release-locks"
#define SVN_DAV_OPTION_KEEP_LOCKS "keep-locks"
#define SVN_DAV_VERSION_NAME_HEADER "X-SVN-Version-Name"
#define SVN_DAV_CREATIONDATE_HEADER "X-SVN-Creation-Date"
#define SVN_DAV_LOCK_OWNER_HEADER "X-SVN-Lock-Owner"
#define SVN_DAV_BASE_FULLTEXT_MD5_HEADER "X-SVN-Base-Fulltext-MD5"
#define SVN_DAV_RESULT_FULLTEXT_MD5_HEADER "X-SVN-Result-Fulltext-MD5"
#define SVN_DAV_ERROR_NAMESPACE "svn:"
#define SVN_DAV_ERROR_TAG "error"
#define SVN_DAV_PROP_NS_SVN "http://subversion.tigris.org/xmlns/svn/"
#define SVN_DAV_PROP_NS_CUSTOM "http://subversion.tigris.org/xmlns/custom/"
#define SVN_DAV_PROP_NS_DAV "http://subversion.tigris.org/xmlns/dav/"
#define SVN_DAV_NS_DAV_SVN_DEPTH SVN_DAV_PROP_NS_DAV "svn/depth"
#define SVN_DAV_NS_DAV_SVN_MERGEINFO SVN_DAV_PROP_NS_DAV "svn/mergeinfo"
#define SVN_DAV_NS_DAV_SVN_LOG_REVPROPS SVN_DAV_PROP_NS_DAV "svn/log-revprops"
#define SVN_DAV_NS_DAV_SVN_PARTIAL_REPLAYSVN_DAV_PROP_NS_DAV "svn/partial-replay"
#if defined(__cplusplus)
}
#endif
#endif
