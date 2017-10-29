#if !defined(CACHE_DIST_COMMON_H)
#define CACHE_DIST_COMMON_H
#define VARY_FORMAT_VERSION 5
#define DISK_FORMAT_VERSION 6
#define CACHE_HEADER_SUFFIX ".header"
#define CACHE_DATA_SUFFIX ".data"
#define CACHE_VDIR_SUFFIX ".vary"
#define AP_TEMPFILE_PREFIX "/"
#define AP_TEMPFILE_BASE "aptmp"
#define AP_TEMPFILE_SUFFIX "XXXXXX"
#define AP_TEMPFILE_BASELEN strlen(AP_TEMPFILE_BASE)
#define AP_TEMPFILE_NAMELEN strlen(AP_TEMPFILE_BASE AP_TEMPFILE_SUFFIX)
#define AP_TEMPFILE AP_TEMPFILE_PREFIX AP_TEMPFILE_BASE AP_TEMPFILE_SUFFIX
typedef struct {
apr_uint32_t format;
int status;
apr_size_t name_len;
apr_size_t entity_version;
apr_time_t date;
apr_time_t expire;
apr_time_t request_time;
apr_time_t response_time;
apr_ino_t inode;
apr_dev_t device;
unsigned int has_body:1;
unsigned int header_only:1;
cache_control_t control;
} disk_cache_info_t;
#endif