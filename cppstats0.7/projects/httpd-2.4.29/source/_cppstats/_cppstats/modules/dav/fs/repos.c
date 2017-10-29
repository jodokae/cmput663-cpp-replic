#include "apr.h"
#include "apr_file_io.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "httpd.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"
#include "mod_dav.h"
#include "repos.h"
#define DEBUG_GET_HANDLER 0
#define DAV_FS_COPY_BLOCKSIZE 16384
struct dav_resource_private {
apr_pool_t *pool;
const char *pathname;
apr_finfo_t finfo;
request_rec *r;
};
typedef struct {
const dav_walk_params *params;
dav_walk_resource wres;
dav_resource res1;
dav_resource_private info1;
dav_buffer path1;
dav_buffer uri_buf;
dav_resource res2;
dav_resource_private info2;
dav_buffer path2;
dav_buffer locknull_buf;
} dav_fs_walker_context;
typedef struct {
int is_move;
dav_buffer work_buf;
const dav_resource *res_dst;
const dav_resource *root;
apr_pool_t *pool;
} dav_fs_copymove_walk_ctx;
#define DAV_WALKTYPE_HIDDEN 0x4000
#define DAV_WALKTYPE_POSTFIX 0x8000
#define DAV_CALLTYPE_POSTFIX 1000
extern const dav_hooks_locks dav_hooks_locks_fs;
static const dav_hooks_repository dav_hooks_repository_fs;
static const dav_hooks_liveprop dav_hooks_liveprop_fs;
static const char * const dav_fs_namespace_uris[] = {
"DAV:",
"http://apache.org/dav/props/",
NULL
};
enum {
DAV_FS_URI_DAV,
DAV_FS_URI_MYPROPS
};
#if !defined(WIN32)
#define DAV_FS_HAS_EXECUTABLE
#define DAV_FINFO_MASK (APR_FINFO_LINK | APR_FINFO_TYPE | APR_FINFO_INODE | APR_FINFO_SIZE | APR_FINFO_CTIME | APR_FINFO_MTIME | APR_FINFO_PROT)
#else
#define DAV_FINFO_MASK (APR_FINFO_LINK | APR_FINFO_TYPE | APR_FINFO_INODE | APR_FINFO_SIZE | APR_FINFO_CTIME | APR_FINFO_MTIME)
#endif
#define DAV_PROPID_FS_executable 1
#define DAV_FS_TMP_PREFIX ".davfs.tmp"
static const dav_liveprop_spec dav_fs_props[] = {
{
DAV_FS_URI_DAV,
"creationdate",
DAV_PROPID_creationdate,
0
},
{
DAV_FS_URI_DAV,
"getcontentlength",
DAV_PROPID_getcontentlength,
0
},
{
DAV_FS_URI_DAV,
"getetag",
DAV_PROPID_getetag,
0
},
{
DAV_FS_URI_DAV,
"getlastmodified",
DAV_PROPID_getlastmodified,
0
},
{
DAV_FS_URI_MYPROPS,
"executable",
DAV_PROPID_FS_executable,
0
},
{ 0 }
};
static const dav_liveprop_group dav_fs_liveprop_group = {
dav_fs_props,
dav_fs_namespace_uris,
&dav_hooks_liveprop_fs
};
struct dav_stream {
apr_pool_t *p;
apr_file_t *f;
const char *pathname;
char *temppath;
int unlink_on_error;
};
#define MAP_IO2HTTP(e) (APR_STATUS_IS_ENOSPC(e) ? HTTP_INSUFFICIENT_STORAGE : APR_STATUS_IS_ENOENT(e) ? HTTP_CONFLICT : HTTP_INTERNAL_SERVER_ERROR)
static dav_error * dav_fs_walk(const dav_walk_params *params, int depth,
dav_response **response);
static dav_error * dav_fs_internal_walk(const dav_walk_params *params,
int depth, int is_move,
const dav_resource *root_dst,
dav_response **response);
static request_rec *dav_fs_get_request_rec(const dav_resource *resource) {
return resource->info->r;
}
apr_pool_t *dav_fs_pool(const dav_resource *resource) {
return resource->info->pool;
}
const char *dav_fs_pathname(const dav_resource *resource) {
return resource->info->pathname;
}
dav_error * dav_fs_dir_file_name(
const dav_resource *resource,
const char **dirpath_p,
const char **fname_p) {
dav_resource_private *ctx = resource->info;
if (resource->collection) {
*dirpath_p = ctx->pathname;
if (fname_p != NULL)
*fname_p = NULL;
} else {
const char *testpath, *rootpath;
char *dirpath = ap_make_dirstr_parent(ctx->pool, ctx->pathname);
apr_size_t dirlen = strlen(dirpath);
apr_status_t rv = APR_SUCCESS;
testpath = dirpath;
if (dirlen > 0) {
rv = apr_filepath_root(&rootpath, &testpath, 0, ctx->pool);
}
if ((rv == APR_SUCCESS && testpath && *testpath)
|| rv == APR_ERELATIVE) {
if (dirpath[dirlen - 1] == '/') {
dirpath[dirlen - 1] = '\0';
}
}
if (rv == APR_SUCCESS || rv == APR_ERELATIVE) {
*dirpath_p = dirpath;
if (fname_p != NULL)
*fname_p = ctx->pathname + dirlen;
} else {
return dav_new_error(ctx->pool, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
"An incomplete/bad path was found in "
"dav_fs_dir_file_name.");
}
}
return NULL;
}
static void dav_format_time(int style, apr_time_t sec, char *buf, apr_size_t buflen) {
apr_time_exp_t tms;
(void) apr_time_exp_gmt(&tms, sec);
if (style == DAV_STYLE_ISO8601) {
apr_snprintf(buf, buflen, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2dZ",
tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
tms.tm_hour, tms.tm_min, tms.tm_sec);
return;
}
apr_snprintf(buf, buflen, "%s, %.2d %s %d %.2d:%.2d:%.2d GMT",
apr_day_snames[tms.tm_wday],
tms.tm_mday, apr_month_snames[tms.tm_mon],
tms.tm_year + 1900,
tms.tm_hour, tms.tm_min, tms.tm_sec);
}
static dav_error * dav_fs_copymove_file(
int is_move,
apr_pool_t * p,
const char *src,
const char *dst,
const apr_finfo_t *src_finfo,
const apr_finfo_t *dst_finfo,
dav_buffer *pbuf) {
dav_buffer work_buf = { 0 };
apr_file_t *inf = NULL;
apr_file_t *outf = NULL;
apr_status_t status;
apr_fileperms_t perms;
if (pbuf == NULL)
pbuf = &work_buf;
if (src_finfo && src_finfo->valid & APR_FINFO_PROT
&& src_finfo->protection & APR_UEXECUTE) {
perms = src_finfo->protection;
if (dst_finfo != NULL) {
if ((status = apr_file_perms_set(dst, perms)) != APR_SUCCESS) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, status,
"Could not set permissions on destination");
}
}
} else {
perms = APR_OS_DEFAULT;
}
dav_set_bufsize(p, pbuf, DAV_FS_COPY_BLOCKSIZE);
if ((status = apr_file_open(&inf, src, APR_READ | APR_BINARY,
APR_OS_DEFAULT, p)) != APR_SUCCESS) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, status,
"Could not open file for reading");
}
status = apr_file_open(&outf, dst, APR_WRITE | APR_CREATE | APR_TRUNCATE
| APR_BINARY, perms, p);
if (status != APR_SUCCESS) {
apr_file_close(inf);
return dav_new_error(p, MAP_IO2HTTP(status), 0, status,
"Could not open file for writing");
}
while (1) {
apr_size_t len = DAV_FS_COPY_BLOCKSIZE;
status = apr_file_read(inf, pbuf->buf, &len);
if (status != APR_SUCCESS && status != APR_EOF) {
apr_status_t lcl_status;
apr_file_close(inf);
apr_file_close(outf);
if ((lcl_status = apr_file_remove(dst, p)) != APR_SUCCESS) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
lcl_status,
"Could not delete output after read "
"failure. Server is now in an "
"inconsistent state.");
}
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, status,
"Could not read input file");
}
if (status == APR_EOF)
break;
status = apr_file_write_full(outf, pbuf->buf, len, NULL);
if (status != APR_SUCCESS) {
apr_status_t lcl_status;
apr_file_close(inf);
apr_file_close(outf);
if ((lcl_status = apr_file_remove(dst, p)) != APR_SUCCESS) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0,
lcl_status,
"Could not delete output after write "
"failure. Server is now in an "
"inconsistent state.");
}
return dav_new_error(p, MAP_IO2HTTP(status), 0, status,
"Could not write output file");
}
}
apr_file_close(inf);
apr_file_close(outf);
if (is_move && (status = apr_file_remove(src, p)) != APR_SUCCESS) {
dav_error *err;
apr_status_t lcl_status;
if (APR_STATUS_IS_ENOENT(status)) {
err = dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, status,
apr_psprintf(p, "Could not remove source "
"file %s after move to %s. The "
"server may be in an "
"inconsistent state.", src, dst));
return err;
} else if ((lcl_status = apr_file_remove(dst, p)) != APR_SUCCESS) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, lcl_status,
"Could not remove source or destination "
"file. Server is now in an inconsistent "
"state.");
}
err = dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, status,
"Could not remove source file after move. "
"Destination was removed to ensure consistency.");
return err;
}
return NULL;
}
static dav_error * dav_fs_copymove_state(
int is_move,
apr_pool_t * p,
const char *src_dir, const char *src_file,
const char *dst_dir, const char *dst_file,
dav_buffer *pbuf) {
apr_finfo_t src_finfo;
apr_finfo_t dst_state_finfo;
apr_status_t rv;
const char *src;
const char *dst;
src = apr_pstrcat(p, src_dir, "/" DAV_FS_STATE_DIR "/", src_file, NULL);
rv = apr_stat(&src_finfo, src, APR_FINFO_NORM, p);
if (rv != APR_SUCCESS && rv != APR_INCOMPLETE) {
return NULL;
}
dst = apr_pstrcat(p, dst_dir, "/" DAV_FS_STATE_DIR, NULL);
rv = apr_dir_make(dst, APR_OS_DEFAULT, p);
if (rv != APR_SUCCESS) {
if (!APR_STATUS_IS_EEXIST(rv)) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
"Could not create internal state directory");
}
}
rv = apr_stat(&dst_state_finfo, dst, APR_FINFO_NORM, p);
if (rv != APR_SUCCESS && rv != APR_INCOMPLETE) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
"State directory disappeared");
}
if (dst_state_finfo.filetype != APR_DIR) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
"State directory is actually a file");
}
dst = apr_pstrcat(p, dst, "/", dst_file, NULL);
if (is_move) {
rv = apr_file_rename(src, dst, p);
if (APR_STATUS_IS_EXDEV(rv)) {
return dav_fs_copymove_file(is_move, p, src, dst, NULL, NULL, pbuf);
}
if (rv != APR_SUCCESS) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
"Could not move state file.");
}
} else {
return dav_fs_copymove_file(is_move, p, src, dst, NULL, NULL, pbuf);
}
return NULL;
}
static dav_error *dav_fs_copymoveset(int is_move, apr_pool_t *p,
const dav_resource *src,
const dav_resource *dst,
dav_buffer *pbuf) {
const char *src_dir;
const char *src_file;
const char *src_state1;
const char *src_state2;
const char *dst_dir;
const char *dst_file;
const char *dst_state1;
const char *dst_state2;
dav_error *err;
(void) dav_fs_dir_file_name(src, &src_dir, &src_file);
(void) dav_fs_dir_file_name(dst, &dst_dir, &dst_file);
dav_dbm_get_statefiles(p, src_file, &src_state1, &src_state2);
dav_dbm_get_statefiles(p, dst_file, &dst_state1, &dst_state2);
#if DAV_DEBUG
if ((src_state2 != NULL && dst_state2 == NULL) ||
(src_state2 == NULL && dst_state2 != NULL)) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
"DESIGN ERROR: dav_dbm_get_statefiles() "
"returned inconsistent results.");
}
#endif
err = dav_fs_copymove_state(is_move, p,
src_dir, src_state1,
dst_dir, dst_state1,
pbuf);
if (err == NULL && src_state2 != NULL) {
err = dav_fs_copymove_state(is_move, p,
src_dir, src_state2,
dst_dir, dst_state2,
pbuf);
if (err != NULL) {
err->status = HTTP_INTERNAL_SERVER_ERROR;
err->desc =
"Could not fully copy/move the properties. "
"The server is now in an inconsistent state.";
}
}
return err;
}
static dav_error *dav_fs_deleteset(apr_pool_t *p, const dav_resource *resource) {
const char *dirpath;
const char *fname;
const char *state1;
const char *state2;
const char *pathname;
apr_status_t status;
(void) dav_fs_dir_file_name(resource, &dirpath, &fname);
dav_dbm_get_statefiles(p, fname, &state1, &state2);
pathname = apr_pstrcat(p,
dirpath,
"/" DAV_FS_STATE_DIR "/",
state1,
NULL);
if ((status = apr_file_remove(pathname, p)) != APR_SUCCESS
&& !APR_STATUS_IS_ENOENT(status)) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, status,
"Could not remove properties.");
}
if (state2 != NULL) {
pathname = apr_pstrcat(p,
dirpath,
"/" DAV_FS_STATE_DIR "/",
state2,
NULL);
if ((status = apr_file_remove(pathname, p)) != APR_SUCCESS
&& !APR_STATUS_IS_ENOENT(status)) {
return dav_new_error(p, HTTP_INTERNAL_SERVER_ERROR, 0, status,
"Could not fully remove properties. "
"The server is now in an inconsistent "
"state.");
}
}
return NULL;
}
static dav_error * dav_fs_get_resource(
request_rec *r,
const char *root_dir,
const char *label,
int use_checked_in,
dav_resource **result_resource) {
dav_resource_private *ctx;
dav_resource *resource;
char *s;
char *filename;
apr_size_t len;
ctx = apr_pcalloc(r->pool, sizeof(*ctx));
ctx->finfo = r->finfo;
ctx->r = r;
ctx->pool = r->pool;
#if 0
filename = r->case_preserved_filename;
#else
filename = r->filename;
#endif
s = apr_pstrcat(r->pool, filename, r->path_info, NULL);
len = strlen(s);
if (len > 1 && s[len - 1] == '/') {
s[len - 1] = '\0';
}
ctx->pathname = s;
resource = apr_pcalloc(r->pool, sizeof(*resource));
resource->type = DAV_RESOURCE_TYPE_REGULAR;
resource->info = ctx;
resource->hooks = &dav_hooks_repository_fs;
resource->pool = r->pool;
len = strlen(r->uri);
if (len > 1 && r->uri[len - 1] == '/') {
s = apr_pstrmemdup(r->pool, r->uri, len-1);
resource->uri = s;
} else {
resource->uri = r->uri;
}
if (r->finfo.filetype != APR_NOFILE) {
resource->exists = 1;
resource->collection = r->finfo.filetype == APR_DIR;
if (r->path_info != NULL && *r->path_info != '\0') {
if (resource->collection) {
if (*r->path_info != '/' || r->path_info[1] != '\0') {
resource->exists = 0;
resource->collection = 0;
}
} else {
return dav_new_error(r->pool, HTTP_BAD_REQUEST, 0, 0,
"The URL contains extraneous path "
"components. The resource could not "
"be identified.");
}
if (!resource->exists) {
ctx->finfo.filetype = APR_NOFILE;
}
}
}
*result_resource = resource;
return NULL;
}
static dav_error * dav_fs_get_parent_resource(const dav_resource *resource,
dav_resource **result_parent) {
dav_resource_private *ctx = resource->info;
dav_resource_private *parent_ctx;
dav_resource *parent_resource;
apr_status_t rv;
char *dirpath;
const char *testroot;
const char *testpath;
if (strcmp(resource->uri, "/") == 0) {
*result_parent = NULL;
return NULL;
}
testpath = ctx->pathname;
rv = apr_filepath_root(&testroot, &testpath, 0, ctx->pool);
if ((rv != APR_SUCCESS && rv != APR_ERELATIVE)
|| !testpath || !*testpath) {
*result_parent = NULL;
return NULL;
}
parent_ctx = apr_pcalloc(ctx->pool, sizeof(*parent_ctx));
parent_ctx->pool = ctx->pool;
dirpath = ap_make_dirstr_parent(ctx->pool, ctx->pathname);
if (strlen(dirpath) > 1 && dirpath[strlen(dirpath) - 1] == '/')
dirpath[strlen(dirpath) - 1] = '\0';
parent_ctx->pathname = dirpath;
parent_resource = apr_pcalloc(ctx->pool, sizeof(*parent_resource));
parent_resource->info = parent_ctx;
parent_resource->collection = 1;
parent_resource->hooks = &dav_hooks_repository_fs;
parent_resource->pool = resource->pool;
if (resource->uri != NULL) {
char *uri = ap_make_dirstr_parent(ctx->pool, resource->uri);
if (strlen(uri) > 1 && uri[strlen(uri) - 1] == '/')
uri[strlen(uri) - 1] = '\0';
parent_resource->uri = uri;
}
rv = apr_stat(&parent_ctx->finfo, parent_ctx->pathname,
APR_FINFO_NORM, ctx->pool);
if (rv == APR_SUCCESS || rv == APR_INCOMPLETE) {
parent_resource->exists = 1;
}
*result_parent = parent_resource;
return NULL;
}
static int dav_fs_is_same_resource(
const dav_resource *res1,
const dav_resource *res2) {
dav_resource_private *ctx1 = res1->info;
dav_resource_private *ctx2 = res2->info;
if (res1->hooks != res2->hooks)
return 0;
if ((ctx1->finfo.filetype != APR_NOFILE) && (ctx2->finfo.filetype != APR_NOFILE)
&& (ctx1->finfo.valid & ctx2->finfo.valid & APR_FINFO_INODE)) {
return ctx1->finfo.inode == ctx2->finfo.inode;
} else {
return strcmp(ctx1->pathname, ctx2->pathname) == 0;
}
}
static int dav_fs_is_parent_resource(
const dav_resource *res1,
const dav_resource *res2) {
dav_resource_private *ctx1 = res1->info;
dav_resource_private *ctx2 = res2->info;
apr_size_t len1 = strlen(ctx1->pathname);
apr_size_t len2;
if (res1->hooks != res2->hooks)
return 0;
len2 = strlen(ctx2->pathname);
return (len2 > len1
&& memcmp(ctx1->pathname, ctx2->pathname, len1) == 0
&& ctx2->pathname[len1] == '/');
}
static apr_status_t tmpfile_cleanup(void *data) {
dav_stream *ds = data;
if (ds->temppath) {
apr_file_remove(ds->temppath, ds->p);
}
return APR_SUCCESS;
}
static apr_status_t dav_fs_mktemp(apr_file_t **fp, char *templ, apr_pool_t *p) {
apr_status_t rv;
int num = ((getpid() << 7) + (apr_uintptr_t)templ % (1 << 16) ) %
( 1 << 23 ) ;
char *numstr = templ + strlen(templ) - 6;
ap_assert(numstr >= templ);
do {
num = (num + 1) % ( 1 << 23 );
apr_snprintf(numstr, 7, "%06x", num);
rv = apr_file_open(fp, templ,
APR_WRITE | APR_CREATE | APR_BINARY | APR_EXCL,
APR_OS_DEFAULT, p);
} while (APR_STATUS_IS_EEXIST(rv));
return rv;
}
static dav_error * dav_fs_open_stream(const dav_resource *resource,
dav_stream_mode mode,
dav_stream **stream) {
apr_pool_t *p = resource->info->pool;
dav_stream *ds = apr_pcalloc(p, sizeof(*ds));
apr_int32_t flags;
apr_status_t rv;
switch (mode) {
default:
flags = APR_READ | APR_BINARY;
break;
case DAV_MODE_WRITE_TRUNC:
flags = APR_WRITE | APR_CREATE | APR_TRUNCATE | APR_BINARY;
break;
case DAV_MODE_WRITE_SEEKABLE:
flags = APR_WRITE | APR_CREATE | APR_BINARY;
break;
}
ds->p = p;
ds->pathname = resource->info->pathname;
ds->temppath = NULL;
ds->unlink_on_error = 0;
if (mode == DAV_MODE_WRITE_TRUNC) {
ds->temppath = apr_pstrcat(p, ap_make_dirstr_parent(p, ds->pathname),
DAV_FS_TMP_PREFIX "XXXXXX", NULL);
rv = dav_fs_mktemp(&ds->f, ds->temppath, ds->p);
apr_pool_cleanup_register(p, ds, tmpfile_cleanup,
apr_pool_cleanup_null);
} else if (mode == DAV_MODE_WRITE_SEEKABLE) {
rv = apr_file_open(&ds->f, ds->pathname, flags | APR_FOPEN_EXCL,
APR_OS_DEFAULT, ds->p);
if (rv == APR_SUCCESS) {
ds->unlink_on_error = 1;
} else if (APR_STATUS_IS_EEXIST(rv)) {
rv = apr_file_open(&ds->f, ds->pathname, flags, APR_OS_DEFAULT,
ds->p);
}
} else {
rv = apr_file_open(&ds->f, ds->pathname, flags, APR_OS_DEFAULT, ds->p);
}
if (rv != APR_SUCCESS) {
return dav_new_error(p, MAP_IO2HTTP(rv), 0, rv,
"An error occurred while opening a resource.");
}
*stream = ds;
return NULL;
}
static dav_error * dav_fs_close_stream(dav_stream *stream, int commit) {
apr_status_t rv;
apr_file_close(stream->f);
if (!commit) {
if (stream->temppath) {
apr_pool_cleanup_run(stream->p, stream, tmpfile_cleanup);
} else if (stream->unlink_on_error) {
if ((rv = apr_file_remove(stream->pathname, stream->p))
!= APR_SUCCESS) {
return dav_new_error(stream->p, HTTP_INTERNAL_SERVER_ERROR, 0,
rv,
"There was a problem removing (rolling "
"back) the resource "
"when it was being closed.");
}
}
} else if (stream->temppath) {
rv = apr_file_rename(stream->temppath, stream->pathname, stream->p);
if (rv) {
return dav_new_error(stream->p, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
"There was a problem writing the file "
"atomically after writes.");
}
apr_pool_cleanup_kill(stream->p, stream, tmpfile_cleanup);
}
return NULL;
}
static dav_error * dav_fs_write_stream(dav_stream *stream,
const void *buf, apr_size_t bufsize) {
apr_status_t status;
status = apr_file_write_full(stream->f, buf, bufsize, NULL);
if (APR_STATUS_IS_ENOSPC(status)) {
return dav_new_error(stream->p, HTTP_INSUFFICIENT_STORAGE, 0, status,
"There is not enough storage to write to "
"this resource.");
} else if (status != APR_SUCCESS) {
return dav_new_error(stream->p, HTTP_INTERNAL_SERVER_ERROR, 0, status,
"An error occurred while writing to a "
"resource.");
}
return NULL;
}
static dav_error * dav_fs_seek_stream(dav_stream *stream, apr_off_t abs_pos) {
apr_status_t status;
if ((status = apr_file_seek(stream->f, APR_SET, &abs_pos))
!= APR_SUCCESS) {
return dav_new_error(stream->p, HTTP_INTERNAL_SERVER_ERROR, 0, status,
"Could not seek to specified position in the "
"resource.");
}
return NULL;
}
#if DEBUG_GET_HANDLER
static dav_error * dav_fs_set_headers(request_rec *r,
const dav_resource *resource) {
if (!resource->exists)
return NULL;
ap_update_mtime(r, resource->info->finfo.mtime);
ap_set_last_modified(r);
ap_set_etag(r);
ap_set_accept_ranges(r);
ap_set_content_length(r, resource->info->finfo.size);
return NULL;
}
static dav_error * dav_fs_deliver(const dav_resource *resource,
ap_filter_t *output) {
apr_pool_t *pool = resource->pool;
apr_bucket_brigade *bb;
apr_file_t *fd;
apr_status_t status;
apr_bucket *bkt;
if (resource->type != DAV_RESOURCE_TYPE_REGULAR
&& resource->type != DAV_RESOURCE_TYPE_VERSION
&& resource->type != DAV_RESOURCE_TYPE_WORKING) {
return dav_new_error(pool, HTTP_CONFLICT, 0, 0,
"Cannot GET this type of resource.");
}
if (resource->collection) {
return dav_new_error(pool, HTTP_CONFLICT, 0, 0,
"There is no default response to GET for a "
"collection.");
}
if ((status = apr_file_open(&fd, resource->info->pathname,
APR_READ | APR_BINARY, 0,
pool)) != APR_SUCCESS) {
return dav_new_error(pool, HTTP_FORBIDDEN, 0, status,
"File permissions deny server access.");
}
bb = apr_brigade_create(pool, output->c->bucket_alloc);
apr_brigade_insert_file(bb, fd, 0, resource->info->finfo.size, pool);
bkt = apr_bucket_eos_create(output->c->bucket_alloc);
APR_BRIGADE_INSERT_TAIL(bb, bkt);
if ((status = ap_pass_brigade(output, bb)) != APR_SUCCESS) {
return dav_new_error(pool, AP_FILTER_ERROR, 0, status,
"Could not write contents to filter.");
}
return NULL;
}
#endif
static dav_error * dav_fs_create_collection(dav_resource *resource) {
dav_resource_private *ctx = resource->info;
apr_status_t status;
status = apr_dir_make(ctx->pathname, APR_OS_DEFAULT, ctx->pool);
if (APR_STATUS_IS_ENOSPC(status)) {
return dav_new_error(ctx->pool, HTTP_INSUFFICIENT_STORAGE, 0, status,
"There is not enough storage to create "
"this collection.");
} else if (APR_STATUS_IS_ENOENT(status)) {
return dav_new_error(ctx->pool, HTTP_CONFLICT, 0, status,
"Cannot create collection; intermediate "
"collection does not exist.");
} else if (status != APR_SUCCESS) {
return dav_new_error(ctx->pool, HTTP_FORBIDDEN, 0, status,
"Unable to create collection.");
}
resource->exists = 1;
resource->collection = 1;
return NULL;
}
static dav_error * dav_fs_copymove_walker(dav_walk_resource *wres,
int calltype) {
apr_status_t status;
dav_fs_copymove_walk_ctx *ctx = wres->walk_ctx;
dav_resource_private *srcinfo = wres->resource->info;
dav_resource_private *dstinfo = ctx->res_dst->info;
dav_error *err = NULL;
if (wres->resource->collection) {
if (calltype == DAV_CALLTYPE_POSTFIX) {
(void) apr_dir_remove(srcinfo->pathname, ctx->pool);
} else {
if ((status = apr_dir_make(dstinfo->pathname, APR_OS_DEFAULT,
ctx->pool)) != APR_SUCCESS) {
err = dav_new_error(ctx->pool, HTTP_FORBIDDEN, 0, status, NULL);
}
}
} else {
err = dav_fs_copymove_file(ctx->is_move, ctx->pool,
srcinfo->pathname, dstinfo->pathname,
&srcinfo->finfo,
ctx->res_dst->exists ? &dstinfo->finfo : NULL,
&ctx->work_buf);
}
if (err != NULL
&& !ap_is_HTTP_SERVER_ERROR(err->status)
&& (ctx->is_move
|| !dav_fs_is_same_resource(wres->resource, ctx->root))) {
dav_add_response(wres, err->status, NULL);
return NULL;
}
return err;
}
static dav_error *dav_fs_copymove_resource(
int is_move,
const dav_resource *src,
const dav_resource *dst,
int depth,
dav_response **response) {
dav_error *err = NULL;
dav_buffer work_buf = { 0 };
*response = NULL;
if (src->collection) {
dav_walk_params params = { 0 };
dav_response *multi_status;
params.walk_type = DAV_WALKTYPE_NORMAL | DAV_WALKTYPE_HIDDEN;
params.func = dav_fs_copymove_walker;
params.pool = src->info->pool;
params.root = src;
if (is_move)
params.walk_type |= DAV_WALKTYPE_POSTFIX;
if ((err = dav_fs_internal_walk(&params, depth, is_move, dst,
&multi_status)) != NULL) {
return err;
}
if ((*response = multi_status) != NULL) {
return dav_new_error(src->info->pool, HTTP_MULTI_STATUS, 0, 0,
"Error(s) occurred on some resources during "
"the COPY/MOVE process.");
}
return NULL;
}
if ((err = dav_fs_copymove_file(is_move, src->info->pool,
src->info->pathname, dst->info->pathname,
&src->info->finfo,
dst->exists ? &dst->info->finfo : NULL,
&work_buf)) != NULL) {
return err;
}
return dav_fs_copymoveset(is_move, src->info->pool, src, dst, &work_buf);
}
static dav_error * dav_fs_copy_resource(
const dav_resource *src,
dav_resource *dst,
int depth,
dav_response **response) {
dav_error *err;
#if DAV_DEBUG
if (src->hooks != dst->hooks) {
return dav_new_error(src->info->pool, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
"DESIGN ERROR: a mix of repositories "
"was passed to copy_resource.");
}
#endif
if ((err = dav_fs_copymove_resource(0, src, dst, depth,
response)) == NULL) {
dst->exists = 1;
dst->collection = src->collection;
}
return err;
}
static dav_error * dav_fs_move_resource(
dav_resource *src,
dav_resource *dst,
dav_response **response) {
dav_resource_private *srcinfo = src->info;
dav_resource_private *dstinfo = dst->info;
dav_error *err;
apr_status_t rv;
#if DAV_DEBUG
if (src->hooks != dst->hooks) {
return dav_new_error(src->info->pool, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
"DESIGN ERROR: a mix of repositories "
"was passed to move_resource.");
}
#endif
rv = apr_file_rename(srcinfo->pathname, dstinfo->pathname, srcinfo->pool);
if (APR_STATUS_IS_EXDEV(rv)) {
if ((err = dav_fs_copymove_resource(1, src, dst, DAV_INFINITY,
response)) == NULL) {
dst->exists = 1;
dst->collection = src->collection;
src->exists = 0;
src->collection = 0;
}
return err;
}
*response = NULL;
if (rv != APR_SUCCESS) {
return dav_new_error(srcinfo->pool, HTTP_INTERNAL_SERVER_ERROR, 0, rv,
"Could not rename resource.");
}
dst->exists = 1;
dst->collection = src->collection;
src->exists = 0;
src->collection = 0;
if ((err = dav_fs_copymoveset(1, src->info->pool,
src, dst, NULL)) == NULL) {
return NULL;
}
if (apr_file_rename(dstinfo->pathname, srcinfo->pathname,
srcinfo->pool) != APR_SUCCESS) {
return dav_push_error(srcinfo->pool,
HTTP_INTERNAL_SERVER_ERROR, 0,
"The resource was moved, but a failure "
"occurred during the move of its "
"properties. The resource could not be "
"restored to its original location. The "
"server is now in an inconsistent state.",
err);
}
src->exists = 1;
src->collection = dst->collection;
dst->exists = 0;
dst->collection = 0;
return dav_push_error(srcinfo->pool,
HTTP_INTERNAL_SERVER_ERROR, 0,
"The resource was moved, but a failure "
"occurred during the move of its properties. "
"The resource was moved back to its original "
"location, but its properties may have been "
"partially moved. The server may be in an "
"inconsistent state.",
err);
}
static dav_error * dav_fs_delete_walker(dav_walk_resource *wres, int calltype) {
dav_resource_private *info = wres->resource->info;
if (wres->resource->exists &&
(!wres->resource->collection || calltype == DAV_CALLTYPE_POSTFIX)) {
apr_status_t result;
result = wres->resource->collection
? apr_dir_remove(info->pathname, wres->pool)
: apr_file_remove(info->pathname, wres->pool);
if (result != APR_SUCCESS) {
dav_add_response(wres, HTTP_FORBIDDEN, NULL);
}
}
return NULL;
}
static dav_error * dav_fs_remove_resource(dav_resource *resource,
dav_response **response) {
apr_status_t status;
dav_resource_private *info = resource->info;
*response = NULL;
if (resource->collection) {
dav_walk_params params = { 0 };
dav_error *err = NULL;
dav_response *multi_status;
params.walk_type = (DAV_WALKTYPE_NORMAL
| DAV_WALKTYPE_HIDDEN
| DAV_WALKTYPE_POSTFIX);
params.func = dav_fs_delete_walker;
params.pool = info->pool;
params.root = resource;
if ((err = dav_fs_walk(&params, DAV_INFINITY,
&multi_status)) != NULL) {
return err;
}
if ((*response = multi_status) != NULL) {
return dav_new_error(info->pool, HTTP_MULTI_STATUS, 0, 0,
"Error(s) occurred on some resources during "
"the deletion process.");
}
resource->exists = 0;
resource->collection = 0;
return NULL;
}
if ((status = apr_file_remove(info->pathname, info->pool)) != APR_SUCCESS) {
return dav_new_error(info->pool, HTTP_FORBIDDEN, 0, status, NULL);
}
resource->exists = 0;
resource->collection = 0;
return dav_fs_deleteset(info->pool, resource);
}
static dav_error * dav_fs_walker(dav_fs_walker_context *fsctx, int depth) {
const dav_walk_params *params = fsctx->params;
apr_pool_t *pool = params->pool;
apr_status_t status;
dav_error *err = NULL;
int isdir = fsctx->res1.collection;
apr_finfo_t dirent;
apr_dir_t *dirp;
err = (*params->func)(&fsctx->wres,
isdir
? DAV_CALLTYPE_COLLECTION
: DAV_CALLTYPE_MEMBER);
if (err != NULL) {
return err;
}
if (depth == 0 || !isdir) {
return NULL;
}
dav_check_bufsize(pool, &fsctx->path1, DAV_BUFFER_PAD);
fsctx->path1.buf[fsctx->path1.cur_len++] = '/';
fsctx->path1.buf[fsctx->path1.cur_len] = '\0';
if (fsctx->path2.buf != NULL) {
dav_check_bufsize(pool, &fsctx->path2, DAV_BUFFER_PAD);
fsctx->path2.buf[fsctx->path2.cur_len++] = '/';
fsctx->path2.buf[fsctx->path2.cur_len] = '\0';
}
fsctx->res1.exists = 1;
fsctx->res1.collection = 0;
fsctx->res2.collection = 0;
if ((status = apr_dir_open(&dirp, fsctx->path1.buf, pool)) != APR_SUCCESS) {
return dav_new_error(pool, HTTP_NOT_FOUND, 0, status, NULL);
}
while ((apr_dir_read(&dirent, APR_FINFO_DIRENT, dirp)) == APR_SUCCESS) {
apr_size_t len;
len = strlen(dirent.name);
if (dirent.name[0] == '.'
&& (len == 1 || (dirent.name[1] == '.' && len == 2))) {
continue;
}
if (params->walk_type & DAV_WALKTYPE_AUTH) {
if (!strcmp(dirent.name, DAV_FS_STATE_DIR) ||
!strncmp(dirent.name, DAV_FS_TMP_PREFIX,
strlen(DAV_FS_TMP_PREFIX))) {
continue;
}
}
if (!(params->walk_type & DAV_WALKTYPE_HIDDEN)
&& (!strcmp(dirent.name, DAV_FS_STATE_DIR) ||
!strncmp(dirent.name, DAV_FS_TMP_PREFIX,
strlen(DAV_FS_TMP_PREFIX)))) {
continue;
}
dav_buffer_place_mem(pool, &fsctx->path1, dirent.name, len + 1, 0);
status = apr_stat(&fsctx->info1.finfo, fsctx->path1.buf,
DAV_FINFO_MASK, pool);
if (status != APR_SUCCESS && status != APR_INCOMPLETE) {
err = dav_new_error(pool, HTTP_NOT_FOUND, 0, status, NULL);
break;
}
dav_buffer_place_mem(pool, &fsctx->uri_buf, dirent.name, len + 1, 1);
if (fsctx->path2.buf != NULL) {
dav_buffer_place_mem(pool, &fsctx->path2, dirent.name, len + 1, 0);
}
fsctx->info1.pathname = fsctx->path1.buf;
fsctx->info2.pathname = fsctx->path2.buf;
fsctx->res1.uri = fsctx->uri_buf.buf;
if (fsctx->info1.finfo.filetype == APR_REG) {
if ((err = (*params->func)(&fsctx->wres,
DAV_CALLTYPE_MEMBER)) != NULL) {
break;
}
} else if (fsctx->info1.finfo.filetype == APR_DIR) {
apr_size_t save_path_len = fsctx->path1.cur_len;
apr_size_t save_uri_len = fsctx->uri_buf.cur_len;
apr_size_t save_path2_len = fsctx->path2.cur_len;
fsctx->path1.cur_len += len;
fsctx->path2.cur_len += len;
fsctx->uri_buf.cur_len += len + 1;
fsctx->uri_buf.buf[fsctx->uri_buf.cur_len - 1] = '/';
fsctx->uri_buf.buf[fsctx->uri_buf.cur_len] = '\0';
fsctx->res1.collection = 1;
fsctx->res2.collection = 1;
if ((err = dav_fs_walker(fsctx, depth - 1)) != NULL) {
break;
}
fsctx->path1.cur_len = save_path_len;
fsctx->path2.cur_len = save_path2_len;
fsctx->uri_buf.cur_len = save_uri_len;
fsctx->res1.collection = 0;
fsctx->res2.collection = 0;
}
}
apr_dir_close(dirp);
if (err != NULL)
return err;
if (params->walk_type & DAV_WALKTYPE_LOCKNULL) {
apr_size_t offset = 0;
fsctx->path1.buf[fsctx->path1.cur_len - 1] = '\0';
fsctx->res1.collection = 1;
if ((err = dav_fs_get_locknull_members(&fsctx->res1,
&fsctx->locknull_buf)) != NULL) {
return err;
}
fsctx->path1.buf[fsctx->path1.cur_len - 1] = '/';
fsctx->res1.exists = 0;
fsctx->res1.collection = 0;
memset(&fsctx->info1.finfo, 0, sizeof(fsctx->info1.finfo));
while (offset < fsctx->locknull_buf.cur_len) {
apr_size_t len = strlen(fsctx->locknull_buf.buf + offset);
dav_lock *locks = NULL;
dav_buffer_place_mem(pool, &fsctx->path1,
fsctx->locknull_buf.buf + offset, len + 1, 0);
dav_buffer_place_mem(pool, &fsctx->uri_buf,
fsctx->locknull_buf.buf + offset, len + 1, 0);
if (fsctx->path2.buf != NULL) {
dav_buffer_place_mem(pool, &fsctx->path2,
fsctx->locknull_buf.buf + offset,
len + 1, 0);
}
fsctx->info1.pathname = fsctx->path1.buf;
fsctx->info2.pathname = fsctx->path2.buf;
fsctx->res1.uri = fsctx->uri_buf.buf;
if ((err = dav_lock_query(params->lockdb, &fsctx->res1,
&locks)) != NULL) {
return err;
}
if (locks != NULL &&
(err = (*params->func)(&fsctx->wres,
DAV_CALLTYPE_LOCKNULL)) != NULL) {
return err;
}
offset += len + 1;
}
fsctx->res1.exists = 1;
}
if (params->walk_type & DAV_WALKTYPE_POSTFIX) {
fsctx->path1.buf[--fsctx->path1.cur_len] = '\0';
fsctx->uri_buf.buf[--fsctx->uri_buf.cur_len] = '\0';
if (fsctx->path2.buf != NULL) {
fsctx->path2.buf[--fsctx->path2.cur_len] = '\0';
}
fsctx->res1.collection = 1;
return (*params->func)(&fsctx->wres, DAV_CALLTYPE_POSTFIX);
}
return NULL;
}
static dav_error * dav_fs_internal_walk(const dav_walk_params *params,
int depth, int is_move,
const dav_resource *root_dst,
dav_response **response) {
dav_fs_walker_context fsctx = { 0 };
dav_error *err;
dav_fs_copymove_walk_ctx cm_ctx = { 0 };
#if DAV_DEBUG
if ((params->walk_type & DAV_WALKTYPE_LOCKNULL) != 0
&& params->lockdb == NULL) {
return dav_new_error(params->pool, HTTP_INTERNAL_SERVER_ERROR, 0, 0,
"DESIGN ERROR: walker called to walk locknull "
"resources, but a lockdb was not provided.");
}
#endif
fsctx.params = params;
fsctx.wres.walk_ctx = params->walk_ctx;
fsctx.wres.pool = params->pool;
fsctx.res1 = *params->root;
fsctx.res1.pool = params->pool;
fsctx.res1.info = &fsctx.info1;
fsctx.info1 = *params->root->info;
dav_buffer_init(params->pool, &fsctx.path1, fsctx.info1.pathname);
fsctx.info1.pathname = fsctx.path1.buf;
if (root_dst != NULL) {
fsctx.wres.walk_ctx = &cm_ctx;
cm_ctx.is_move = is_move;
cm_ctx.res_dst = &fsctx.res2;
cm_ctx.root = params->root;
cm_ctx.pool = params->pool;
fsctx.res2 = *root_dst;
fsctx.res2.exists = 0;
fsctx.res2.collection = 0;
fsctx.res2.uri = NULL;
fsctx.res2.pool = params->pool;
fsctx.res2.info = &fsctx.info2;
fsctx.info2 = *root_dst->info;
memset(&fsctx.info2.finfo, 0, sizeof(fsctx.info2.finfo));
dav_buffer_init(params->pool, &fsctx.path2, fsctx.info2.pathname);
fsctx.info2.pathname = fsctx.path2.buf;
}
dav_buffer_init(params->pool, &fsctx.uri_buf, params->root->uri);
if (fsctx.res1.collection
&& fsctx.uri_buf.buf[fsctx.uri_buf.cur_len - 1] != '/') {
fsctx.uri_buf.buf[fsctx.uri_buf.cur_len++] = '/';
fsctx.uri_buf.buf[fsctx.uri_buf.cur_len] = '\0';
}
fsctx.res1.uri = fsctx.uri_buf.buf;
fsctx.wres.resource = &fsctx.res1;
err = dav_fs_walker(&fsctx, depth);
*response = fsctx.wres.response;
return err;
}
static dav_error * dav_fs_walk(const dav_walk_params *params, int depth,
dav_response **response) {
return dav_fs_internal_walk(params, depth, 0, NULL, response);
}
static const char *dav_fs_getetag(const dav_resource *resource) {
dav_resource_private *ctx = resource->info;
if (!resource->exists)
return apr_pstrdup(ctx->pool, "");
if (ctx->finfo.filetype != APR_NOFILE) {
return apr_psprintf(ctx->pool, "\"%" APR_UINT64_T_HEX_FMT "-%"
APR_UINT64_T_HEX_FMT "\"",
(apr_uint64_t) ctx->finfo.size,
(apr_uint64_t) ctx->finfo.mtime);
}
return apr_psprintf(ctx->pool, "\"%" APR_UINT64_T_HEX_FMT "\"",
(apr_uint64_t) ctx->finfo.mtime);
}
static const dav_hooks_repository dav_hooks_repository_fs = {
DEBUG_GET_HANDLER,
dav_fs_get_resource,
dav_fs_get_parent_resource,
dav_fs_is_same_resource,
dav_fs_is_parent_resource,
dav_fs_open_stream,
dav_fs_close_stream,
dav_fs_write_stream,
dav_fs_seek_stream,
#if DEBUG_GET_HANDLER
dav_fs_set_headers,
dav_fs_deliver,
#else
NULL,
NULL,
#endif
dav_fs_create_collection,
dav_fs_copy_resource,
dav_fs_move_resource,
dav_fs_remove_resource,
dav_fs_walk,
dav_fs_getetag,
NULL,
dav_fs_get_request_rec,
dav_fs_pathname
};
static dav_prop_insert dav_fs_insert_prop(const dav_resource *resource,
int propid, dav_prop_insert what,
apr_text_header *phdr) {
const char *value;
const char *s;
apr_pool_t *p = resource->info->pool;
const dav_liveprop_spec *info;
int global_ns;
char buf[DAV_TIMEBUF_SIZE];
if (!resource->exists)
return DAV_PROP_INSERT_NOTDEF;
switch (propid) {
case DAV_PROPID_creationdate:
dav_format_time(DAV_STYLE_ISO8601,
resource->info->finfo.ctime,
buf, sizeof(buf));
value = buf;
break;
case DAV_PROPID_getcontentlength:
if (resource->collection)
return DAV_PROP_INSERT_NOTDEF;
apr_snprintf(buf, sizeof(buf), "%" APR_OFF_T_FMT, resource->info->finfo.size);
value = buf;
break;
case DAV_PROPID_getetag:
value = dav_fs_getetag(resource);
break;
case DAV_PROPID_getlastmodified:
dav_format_time(DAV_STYLE_RFC822,
resource->info->finfo.mtime,
buf, sizeof(buf));
value = buf;
break;
case DAV_PROPID_FS_executable:
if (resource->collection)
return DAV_PROP_INSERT_NOTDEF;
if (!(resource->info->finfo.valid & APR_FINFO_UPROT))
return DAV_PROP_INSERT_NOTDEF;
if (resource->info->finfo.protection & APR_UEXECUTE)
value = "T";
else
value = "F";
break;
default:
return DAV_PROP_INSERT_NOTDEF;
}
global_ns = dav_get_liveprop_info(propid, &dav_fs_liveprop_group, &info);
if (what == DAV_PROP_INSERT_VALUE) {
s = apr_psprintf(p, "<lp%d:%s>%s</lp%d:%s>" DEBUG_CR,
global_ns, info->name, value, global_ns, info->name);
} else if (what == DAV_PROP_INSERT_NAME) {
s = apr_psprintf(p, "<lp%d:%s/>" DEBUG_CR, global_ns, info->name);
} else {
s = apr_psprintf(p,
"<D:supported-live-property D:name=\"%s\" "
"D:namespace=\"%s\"/>" DEBUG_CR,
info->name, dav_fs_namespace_uris[info->ns]);
}
apr_text_append(p, phdr, s);
return what;
}
static int dav_fs_is_writable(const dav_resource *resource, int propid) {
const dav_liveprop_spec *info;
#if defined(DAV_FS_HAS_EXECUTABLE)
if (propid == DAV_PROPID_FS_executable && !resource->collection)
return 1;
#endif
(void) dav_get_liveprop_info(propid, &dav_fs_liveprop_group, &info);
return info->is_writable;
}
static dav_error *dav_fs_patch_validate(const dav_resource *resource,
const apr_xml_elem *elem,
int operation,
void **context,
int *defer_to_dead) {
const apr_text *cdata;
const apr_text *f_cdata;
char value;
dav_elem_private *priv = elem->priv;
if (priv->propid != DAV_PROPID_FS_executable) {
*defer_to_dead = 1;
return NULL;
}
if (operation == DAV_PROP_OP_DELETE) {
return dav_new_error(resource->info->pool, HTTP_CONFLICT, 0, 0,
"The 'executable' property cannot be removed.");
}
cdata = elem->first_cdata.first;
f_cdata = elem->first_child == NULL
? NULL
: elem->first_child->following_cdata.first;
if (cdata == NULL) {
if (f_cdata == NULL) {
return dav_new_error(resource->info->pool, HTTP_CONFLICT, 0, 0,
"The 'executable' property expects a single "
"character, valued 'T' or 'F'. There was no "
"value submitted.");
}
cdata = f_cdata;
} else if (f_cdata != NULL)
goto too_long;
if (cdata->next != NULL || strlen(cdata->text) != 1)
goto too_long;
value = cdata->text[0];
if (value != 'T' && value != 'F') {
return dav_new_error(resource->info->pool, HTTP_CONFLICT, 0, 0,
"The 'executable' property expects a single "
"character, valued 'T' or 'F'. The value "
"submitted is invalid.");
}
*context = (void *)((long)(value == 'T'));
return NULL;
too_long:
return dav_new_error(resource->info->pool, HTTP_CONFLICT, 0, 0,
"The 'executable' property expects a single "
"character, valued 'T' or 'F'. The value submitted "
"has too many characters.");
}
static dav_error *dav_fs_patch_exec(const dav_resource *resource,
const apr_xml_elem *elem,
int operation,
void *context,
dav_liveprop_rollback **rollback_ctx) {
long value = context != NULL;
apr_fileperms_t perms = resource->info->finfo.protection;
apr_status_t status;
long old_value = (perms & APR_UEXECUTE) != 0;
if (value == old_value)
return NULL;
perms &= ~APR_UEXECUTE;
if (value)
perms |= APR_UEXECUTE;
if ((status = apr_file_perms_set(resource->info->pathname, perms))
!= APR_SUCCESS) {
return dav_new_error(resource->info->pool,
HTTP_INTERNAL_SERVER_ERROR, 0, status,
"Could not set the executable flag of the "
"target resource.");
}
resource->info->finfo.protection = perms;
*rollback_ctx = (dav_liveprop_rollback *)old_value;
return NULL;
}
static void dav_fs_patch_commit(const dav_resource *resource,
int operation,
void *context,
dav_liveprop_rollback *rollback_ctx) {
}
static dav_error *dav_fs_patch_rollback(const dav_resource *resource,
int operation,
void *context,
dav_liveprop_rollback *rollback_ctx) {
apr_fileperms_t perms = resource->info->finfo.protection & ~APR_UEXECUTE;
apr_status_t status;
int value = rollback_ctx != NULL;
if (value)
perms |= APR_UEXECUTE;
if ((status = apr_file_perms_set(resource->info->pathname, perms))
!= APR_SUCCESS) {
return dav_new_error(resource->info->pool,
HTTP_INTERNAL_SERVER_ERROR, 0, status,
"After a failure occurred, the resource's "
"executable flag could not be restored.");
}
resource->info->finfo.protection = perms;
return NULL;
}
static const dav_hooks_liveprop dav_hooks_liveprop_fs = {
dav_fs_insert_prop,
dav_fs_is_writable,
dav_fs_namespace_uris,
dav_fs_patch_validate,
dav_fs_patch_exec,
dav_fs_patch_commit,
dav_fs_patch_rollback
};
static const dav_provider dav_fs_provider = {
&dav_hooks_repository_fs,
&dav_hooks_db_dbm,
&dav_hooks_locks_fs,
NULL,
NULL,
NULL,
NULL
};
void dav_fs_gather_propsets(apr_array_header_t *uris) {
#if defined(DAV_FS_HAS_EXECUTABLE)
*(const char **)apr_array_push(uris) =
"<http://apache.org/dav/propset/fs/1>";
#endif
}
int dav_fs_find_liveprop(const dav_resource *resource,
const char *ns_uri, const char *name,
const dav_hooks_liveprop **hooks) {
if (resource->hooks != &dav_hooks_repository_fs)
return 0;
return dav_do_find_liveprop(ns_uri, name, &dav_fs_liveprop_group, hooks);
}
void dav_fs_insert_all_liveprops(request_rec *r, const dav_resource *resource,
dav_prop_insert what, apr_text_header *phdr) {
if (resource->hooks != &dav_hooks_repository_fs)
return;
if (!resource->exists) {
return;
}
(void) dav_fs_insert_prop(resource, DAV_PROPID_creationdate,
what, phdr);
(void) dav_fs_insert_prop(resource, DAV_PROPID_getcontentlength,
what, phdr);
(void) dav_fs_insert_prop(resource, DAV_PROPID_getlastmodified,
what, phdr);
(void) dav_fs_insert_prop(resource, DAV_PROPID_getetag,
what, phdr);
#if defined(DAV_FS_HAS_EXECUTABLE)
(void) dav_fs_insert_prop(resource, DAV_PROPID_FS_executable,
what, phdr);
#endif
}
void dav_fs_register(apr_pool_t *p) {
dav_register_liveprop_group(p, &dav_fs_liveprop_group);
dav_register_provider(p, "filesystem", &dav_fs_provider);
}