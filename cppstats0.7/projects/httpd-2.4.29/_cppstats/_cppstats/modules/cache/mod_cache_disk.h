#if !defined(MOD_CACHE_DISK_H)
#define MOD_CACHE_DISK_H
#include "apr_file_io.h"
#include "cache_disk_common.h"
typedef struct {
apr_pool_t *pool;
const char *file;
apr_file_t *fd;
char *tempfile;
apr_file_t *tempfd;
} disk_cache_file_t;
typedef struct disk_cache_object {
const char *root;
apr_size_t root_len;
const char *prefix;
disk_cache_file_t data;
disk_cache_file_t hdrs;
disk_cache_file_t vary;
const char *hashfile;
const char *name;
const char *key;
apr_off_t file_size;
disk_cache_info_t disk_info;
apr_table_t *headers_in;
apr_table_t *headers_out;
apr_off_t offset;
apr_time_t timeout;
unsigned int done:1;
} disk_cache_object_t;
#define CACHEFILE_LEN 20
#define DEFAULT_DIRLEVELS 2
#define DEFAULT_DIRLENGTH 2
#define DEFAULT_MIN_FILE_SIZE 1
#define DEFAULT_MAX_FILE_SIZE 1000000
#define DEFAULT_READSIZE 0
#define DEFAULT_READTIME 0
typedef struct {
const char* cache_root;
apr_size_t cache_root_len;
int dirlevels;
int dirlength;
} disk_cache_conf;
typedef struct {
apr_off_t minfs;
apr_off_t maxfs;
apr_off_t readsize;
apr_time_t readtime;
unsigned int minfs_set:1;
unsigned int maxfs_set:1;
unsigned int readsize_set:1;
unsigned int readtime_set:1;
} disk_cache_dir_conf;
#endif
