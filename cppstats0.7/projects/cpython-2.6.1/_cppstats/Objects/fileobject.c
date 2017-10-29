#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "structmember.h"
#if defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif
#if defined(MS_WINDOWS)
#define fileno _fileno
#define HAVE_FTRUNCATE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if defined(PYOS_OS2) && defined(PYCC_GCC)
#include <io.h>
#endif
#define BUF(v) PyString_AS_STRING((PyStringObject *)v)
#if !defined(DONT_HAVE_ERRNO_H)
#include <errno.h>
#endif
#if defined(HAVE_GETC_UNLOCKED)
#define GETC(f) getc_unlocked(f)
#define FLOCKFILE(f) flockfile(f)
#define FUNLOCKFILE(f) funlockfile(f)
#else
#define GETC(f) getc(f)
#define FLOCKFILE(f)
#define FUNLOCKFILE(f)
#endif
#define NEWLINE_UNKNOWN 0
#define NEWLINE_CR 1
#define NEWLINE_LF 2
#define NEWLINE_CRLF 4
#define FILE_BEGIN_ALLOW_THREADS(fobj) { fobj->unlocked_count++; Py_BEGIN_ALLOW_THREADS
#define FILE_END_ALLOW_THREADS(fobj) Py_END_ALLOW_THREADS fobj->unlocked_count--; assert(fobj->unlocked_count >= 0); }
#define FILE_ABORT_ALLOW_THREADS(fobj) Py_BLOCK_THREADS fobj->unlocked_count--; assert(fobj->unlocked_count >= 0);
#if defined(__cplusplus)
extern "C" {
#endif
FILE *
PyFile_AsFile(PyObject *f) {
if (f == NULL || !PyFile_Check(f))
return NULL;
else
return ((PyFileObject *)f)->f_fp;
}
void PyFile_IncUseCount(PyFileObject *fobj) {
fobj->unlocked_count++;
}
void PyFile_DecUseCount(PyFileObject *fobj) {
fobj->unlocked_count--;
assert(fobj->unlocked_count >= 0);
}
PyObject *
PyFile_Name(PyObject *f) {
if (f == NULL || !PyFile_Check(f))
return NULL;
else
return ((PyFileObject *)f)->f_name;
}
static int
file_PyObject_Print(PyObject *op, PyFileObject *f, int flags) {
int result;
PyFile_IncUseCount(f);
result = PyObject_Print(op, f->f_fp, flags);
PyFile_DecUseCount(f);
return result;
}
static PyFileObject*
dircheck(PyFileObject* f) {
#if defined(HAVE_FSTAT) && defined(S_IFDIR) && defined(EISDIR)
struct stat buf;
if (f->f_fp == NULL)
return f;
if (fstat(fileno(f->f_fp), &buf) == 0 &&
S_ISDIR(buf.st_mode)) {
char *msg = strerror(EISDIR);
PyObject *exc = PyObject_CallFunction(PyExc_IOError, "(is)",
EISDIR, msg);
PyErr_SetObject(PyExc_IOError, exc);
Py_XDECREF(exc);
return NULL;
}
#endif
return f;
}
static PyObject *
fill_file_fields(PyFileObject *f, FILE *fp, PyObject *name, char *mode,
int (*close)(FILE *)) {
assert(name != NULL);
assert(f != NULL);
assert(PyFile_Check(f));
assert(f->f_fp == NULL);
Py_DECREF(f->f_name);
Py_DECREF(f->f_mode);
Py_DECREF(f->f_encoding);
Py_DECREF(f->f_errors);
Py_INCREF(name);
f->f_name = name;
f->f_mode = PyString_FromString(mode);
f->f_close = close;
f->f_softspace = 0;
f->f_binary = strchr(mode,'b') != NULL;
f->f_buf = NULL;
f->f_univ_newline = (strchr(mode, 'U') != NULL);
f->f_newlinetypes = NEWLINE_UNKNOWN;
f->f_skipnextlf = 0;
Py_INCREF(Py_None);
f->f_encoding = Py_None;
Py_INCREF(Py_None);
f->f_errors = Py_None;
if (f->f_mode == NULL)
return NULL;
f->f_fp = fp;
f = dircheck(f);
return (PyObject *) f;
}
int
_PyFile_SanitizeMode(char *mode) {
char *upos;
size_t len = strlen(mode);
if (!len) {
PyErr_SetString(PyExc_ValueError, "empty mode string");
return -1;
}
upos = strchr(mode, 'U');
if (upos) {
memmove(upos, upos+1, len-(upos-mode));
if (mode[0] == 'w' || mode[0] == 'a') {
PyErr_Format(PyExc_ValueError, "universal newline "
"mode can only be used with modes "
"starting with 'r'");
return -1;
}
if (mode[0] != 'r') {
memmove(mode+1, mode, strlen(mode)+1);
mode[0] = 'r';
}
if (!strchr(mode, 'b')) {
memmove(mode+2, mode+1, strlen(mode));
mode[1] = 'b';
}
} else if (mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a') {
PyErr_Format(PyExc_ValueError, "mode string must begin with "
"one of 'r', 'w', 'a' or 'U', not '%.200s'", mode);
return -1;
}
return 0;
}
static PyObject *
open_the_file(PyFileObject *f, char *name, char *mode) {
char *newmode;
assert(f != NULL);
assert(PyFile_Check(f));
#if defined(MS_WINDOWS)
assert(f->f_name != NULL);
#else
assert(name != NULL);
#endif
assert(mode != NULL);
assert(f->f_fp == NULL);
newmode = PyMem_MALLOC(strlen(mode) + 3);
if (!newmode) {
PyErr_NoMemory();
return NULL;
}
strcpy(newmode, mode);
if (_PyFile_SanitizeMode(newmode)) {
f = NULL;
goto cleanup;
}
if (PyEval_GetRestricted()) {
PyErr_SetString(PyExc_IOError,
"file() constructor not accessible in restricted mode");
f = NULL;
goto cleanup;
}
errno = 0;
#if defined(MS_WINDOWS)
if (PyUnicode_Check(f->f_name)) {
PyObject *wmode;
wmode = PyUnicode_DecodeASCII(newmode, strlen(newmode), NULL);
if (f->f_name && wmode) {
FILE_BEGIN_ALLOW_THREADS(f)
f->f_fp = _wfopen(PyUnicode_AS_UNICODE(f->f_name),
PyUnicode_AS_UNICODE(wmode));
FILE_END_ALLOW_THREADS(f)
}
Py_XDECREF(wmode);
}
#endif
if (NULL == f->f_fp && NULL != name) {
FILE_BEGIN_ALLOW_THREADS(f)
f->f_fp = fopen(name, newmode);
FILE_END_ALLOW_THREADS(f)
}
if (f->f_fp == NULL) {
#if defined _MSC_VER && (_MSC_VER < 1400 || !defined(__STDC_SECURE_LIB__))
if (errno == 0)
errno = EINVAL;
else if (errno == EINVAL)
errno = ENOENT;
#endif
if (errno == EINVAL) {
PyObject *v;
char message[100];
PyOS_snprintf(message, 100,
"invalid mode ('%.50s') or filename", mode);
v = Py_BuildValue("(isO)", errno, message, f->f_name);
if (v != NULL) {
PyErr_SetObject(PyExc_IOError, v);
Py_DECREF(v);
}
} else
PyErr_SetFromErrnoWithFilenameObject(PyExc_IOError, f->f_name);
f = NULL;
}
if (f != NULL)
f = dircheck(f);
cleanup:
PyMem_FREE(newmode);
return (PyObject *)f;
}
static PyObject *
close_the_file(PyFileObject *f) {
int sts = 0;
int (*local_close)(FILE *);
FILE *local_fp = f->f_fp;
if (local_fp != NULL) {
local_close = f->f_close;
if (local_close != NULL && f->unlocked_count > 0) {
if (f->ob_refcnt > 0) {
PyErr_SetString(PyExc_IOError,
"close() called during concurrent "
"operation on the same file object.");
} else {
PyErr_SetString(PyExc_SystemError,
"PyFileObject locking error in "
"destructor (refcnt <= 0 at close).");
}
return NULL;
}
f->f_fp = NULL;
if (local_close != NULL) {
Py_BEGIN_ALLOW_THREADS
errno = 0;
sts = (*local_close)(local_fp);
Py_END_ALLOW_THREADS
if (sts == EOF)
return PyErr_SetFromErrno(PyExc_IOError);
if (sts != 0)
return PyInt_FromLong((long)sts);
}
}
Py_RETURN_NONE;
}
PyObject *
PyFile_FromFile(FILE *fp, char *name, char *mode, int (*close)(FILE *)) {
PyFileObject *f = (PyFileObject *)PyFile_Type.tp_new(&PyFile_Type,
NULL, NULL);
if (f != NULL) {
PyObject *o_name = PyString_FromString(name);
if (o_name == NULL)
return NULL;
if (fill_file_fields(f, fp, o_name, mode, close) == NULL) {
Py_DECREF(f);
f = NULL;
}
Py_DECREF(o_name);
}
return (PyObject *) f;
}
PyObject *
PyFile_FromString(char *name, char *mode) {
extern int fclose(FILE *);
PyFileObject *f;
f = (PyFileObject *)PyFile_FromFile((FILE *)NULL, name, mode, fclose);
if (f != NULL) {
if (open_the_file(f, name, mode) == NULL) {
Py_DECREF(f);
f = NULL;
}
}
return (PyObject *)f;
}
void
PyFile_SetBufSize(PyObject *f, int bufsize) {
PyFileObject *file = (PyFileObject *)f;
if (bufsize >= 0) {
int type;
switch (bufsize) {
case 0:
type = _IONBF;
break;
#if defined(HAVE_SETVBUF)
case 1:
type = _IOLBF;
bufsize = BUFSIZ;
break;
#endif
default:
type = _IOFBF;
#if !defined(HAVE_SETVBUF)
bufsize = BUFSIZ;
#endif
break;
}
fflush(file->f_fp);
if (type == _IONBF) {
PyMem_Free(file->f_setbuf);
file->f_setbuf = NULL;
} else {
file->f_setbuf = (char *)PyMem_Realloc(file->f_setbuf,
bufsize);
}
#if defined(HAVE_SETVBUF)
setvbuf(file->f_fp, file->f_setbuf, type, bufsize);
#else
setbuf(file->f_fp, file->f_setbuf);
#endif
}
}
int
PyFile_SetEncoding(PyObject *f, const char *enc) {
return PyFile_SetEncodingAndErrors(f, enc, NULL);
}
int
PyFile_SetEncodingAndErrors(PyObject *f, const char *enc, char* errors) {
PyFileObject *file = (PyFileObject*)f;
PyObject *str, *oerrors;
assert(PyFile_Check(f));
str = PyString_FromString(enc);
if (!str)
return 0;
if (errors) {
oerrors = PyString_FromString(errors);
if (!oerrors) {
Py_DECREF(str);
return 0;
}
} else {
oerrors = Py_None;
Py_INCREF(Py_None);
}
Py_DECREF(file->f_encoding);
file->f_encoding = str;
Py_DECREF(file->f_errors);
file->f_errors = oerrors;
return 1;
}
static PyObject *
err_closed(void) {
PyErr_SetString(PyExc_ValueError, "I/O operation on closed file");
return NULL;
}
static PyObject *
err_iterbuffered(void) {
PyErr_SetString(PyExc_ValueError,
"Mixing iteration and read methods would lose data");
return NULL;
}
static void drop_readahead(PyFileObject *);
static void
file_dealloc(PyFileObject *f) {
PyObject *ret;
if (f->weakreflist != NULL)
PyObject_ClearWeakRefs((PyObject *) f);
ret = close_the_file(f);
if (!ret) {
PySys_WriteStderr("close failed in file object destructor:\n");
PyErr_Print();
} else {
Py_DECREF(ret);
}
PyMem_Free(f->f_setbuf);
Py_XDECREF(f->f_name);
Py_XDECREF(f->f_mode);
Py_XDECREF(f->f_encoding);
Py_XDECREF(f->f_errors);
drop_readahead(f);
Py_TYPE(f)->tp_free((PyObject *)f);
}
static PyObject *
file_repr(PyFileObject *f) {
if (PyUnicode_Check(f->f_name)) {
#if defined(Py_USING_UNICODE)
PyObject *ret = NULL;
PyObject *name = PyUnicode_AsUnicodeEscapeString(f->f_name);
const char *name_str = name ? PyString_AsString(name) : "?";
ret = PyString_FromFormat("<%s file u'%s', mode '%s' at %p>",
f->f_fp == NULL ? "closed" : "open",
name_str,
PyString_AsString(f->f_mode),
f);
Py_XDECREF(name);
return ret;
#endif
} else {
return PyString_FromFormat("<%s file '%s', mode '%s' at %p>",
f->f_fp == NULL ? "closed" : "open",
PyString_AsString(f->f_name),
PyString_AsString(f->f_mode),
f);
}
}
static PyObject *
file_close(PyFileObject *f) {
PyObject *sts = close_the_file(f);
PyMem_Free(f->f_setbuf);
f->f_setbuf = NULL;
return sts;
}
#if !defined(HAVE_LARGEFILE_SUPPORT)
typedef off_t Py_off_t;
#elif SIZEOF_OFF_T >= 8
typedef off_t Py_off_t;
#elif SIZEOF_FPOS_T >= 8
typedef fpos_t Py_off_t;
#else
#error "Large file support, but neither off_t nor fpos_t is large enough."
#endif
static int
_portable_fseek(FILE *fp, Py_off_t offset, int whence) {
#if !defined(HAVE_LARGEFILE_SUPPORT)
return fseek(fp, offset, whence);
#elif defined(HAVE_FSEEKO) && SIZEOF_OFF_T >= 8
return fseeko(fp, offset, whence);
#elif defined(HAVE_FSEEK64)
return fseek64(fp, offset, whence);
#elif defined(__BEOS__)
return _fseek(fp, offset, whence);
#elif SIZEOF_FPOS_T >= 8
fpos_t pos;
switch (whence) {
case SEEK_END:
#if defined(MS_WINDOWS)
fflush(fp);
if (_lseeki64(fileno(fp), 0, 2) == -1)
return -1;
#else
if (fseek(fp, 0, SEEK_END) != 0)
return -1;
#endif
case SEEK_CUR:
if (fgetpos(fp, &pos) != 0)
return -1;
offset += pos;
break;
}
return fsetpos(fp, &offset);
#else
#error "Large file support, but no way to fseek."
#endif
}
static Py_off_t
_portable_ftell(FILE* fp) {
#if !defined(HAVE_LARGEFILE_SUPPORT)
return ftell(fp);
#elif defined(HAVE_FTELLO) && SIZEOF_OFF_T >= 8
return ftello(fp);
#elif defined(HAVE_FTELL64)
return ftell64(fp);
#elif SIZEOF_FPOS_T >= 8
fpos_t pos;
if (fgetpos(fp, &pos) != 0)
return -1;
return pos;
#else
#error "Large file support, but no way to ftell."
#endif
}
static PyObject *
file_seek(PyFileObject *f, PyObject *args) {
int whence;
int ret;
Py_off_t offset;
PyObject *offobj, *off_index;
if (f->f_fp == NULL)
return err_closed();
drop_readahead(f);
whence = 0;
if (!PyArg_ParseTuple(args, "O|i:seek", &offobj, &whence))
return NULL;
off_index = PyNumber_Index(offobj);
if (!off_index) {
if (!PyFloat_Check(offobj))
return NULL;
PyErr_Clear();
if (PyErr_WarnEx(PyExc_DeprecationWarning,
"integer argument expected, got float",
1) < 0)
return NULL;
off_index = offobj;
Py_INCREF(offobj);
}
#if !defined(HAVE_LARGEFILE_SUPPORT)
offset = PyInt_AsLong(off_index);
#else
offset = PyLong_Check(off_index) ?
PyLong_AsLongLong(off_index) : PyInt_AsLong(off_index);
#endif
Py_DECREF(off_index);
if (PyErr_Occurred())
return NULL;
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
ret = _portable_fseek(f->f_fp, offset, whence);
FILE_END_ALLOW_THREADS(f)
if (ret != 0) {
PyErr_SetFromErrno(PyExc_IOError);
clearerr(f->f_fp);
return NULL;
}
f->f_skipnextlf = 0;
Py_INCREF(Py_None);
return Py_None;
}
#if defined(HAVE_FTRUNCATE)
static PyObject *
file_truncate(PyFileObject *f, PyObject *args) {
Py_off_t newsize;
PyObject *newsizeobj = NULL;
Py_off_t initialpos;
int ret;
if (f->f_fp == NULL)
return err_closed();
if (!PyArg_UnpackTuple(args, "truncate", 0, 1, &newsizeobj))
return NULL;
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
initialpos = _portable_ftell(f->f_fp);
FILE_END_ALLOW_THREADS(f)
if (initialpos == -1)
goto onioerror;
if (newsizeobj != NULL) {
#if !defined(HAVE_LARGEFILE_SUPPORT)
newsize = PyInt_AsLong(newsizeobj);
#else
newsize = PyLong_Check(newsizeobj) ?
PyLong_AsLongLong(newsizeobj) :
PyInt_AsLong(newsizeobj);
#endif
if (PyErr_Occurred())
return NULL;
} else
newsize = initialpos;
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
ret = fflush(f->f_fp);
FILE_END_ALLOW_THREADS(f)
if (ret != 0)
goto onioerror;
#if defined(MS_WINDOWS)
{
HANDLE hFile;
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
ret = _portable_fseek(f->f_fp, newsize, SEEK_SET) != 0;
FILE_END_ALLOW_THREADS(f)
if (ret)
goto onioerror;
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
hFile = (HANDLE)_get_osfhandle(fileno(f->f_fp));
ret = hFile == (HANDLE)-1;
if (ret == 0) {
ret = SetEndOfFile(hFile) == 0;
if (ret)
errno = EACCES;
}
FILE_END_ALLOW_THREADS(f)
if (ret)
goto onioerror;
}
#else
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
ret = ftruncate(fileno(f->f_fp), newsize);
FILE_END_ALLOW_THREADS(f)
if (ret != 0)
goto onioerror;
#endif
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
ret = _portable_fseek(f->f_fp, initialpos, SEEK_SET) != 0;
FILE_END_ALLOW_THREADS(f)
if (ret)
goto onioerror;
Py_INCREF(Py_None);
return Py_None;
onioerror:
PyErr_SetFromErrno(PyExc_IOError);
clearerr(f->f_fp);
return NULL;
}
#endif
static PyObject *
file_tell(PyFileObject *f) {
Py_off_t pos;
if (f->f_fp == NULL)
return err_closed();
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
pos = _portable_ftell(f->f_fp);
FILE_END_ALLOW_THREADS(f)
if (pos == -1) {
PyErr_SetFromErrno(PyExc_IOError);
clearerr(f->f_fp);
return NULL;
}
if (f->f_skipnextlf) {
int c;
c = GETC(f->f_fp);
if (c == '\n') {
f->f_newlinetypes |= NEWLINE_CRLF;
pos++;
f->f_skipnextlf = 0;
} else if (c != EOF) ungetc(c, f->f_fp);
}
#if !defined(HAVE_LARGEFILE_SUPPORT)
return PyInt_FromLong(pos);
#else
return PyLong_FromLongLong(pos);
#endif
}
static PyObject *
file_fileno(PyFileObject *f) {
if (f->f_fp == NULL)
return err_closed();
return PyInt_FromLong((long) fileno(f->f_fp));
}
static PyObject *
file_flush(PyFileObject *f) {
int res;
if (f->f_fp == NULL)
return err_closed();
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
res = fflush(f->f_fp);
FILE_END_ALLOW_THREADS(f)
if (res != 0) {
PyErr_SetFromErrno(PyExc_IOError);
clearerr(f->f_fp);
return NULL;
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
file_isatty(PyFileObject *f) {
long res;
if (f->f_fp == NULL)
return err_closed();
FILE_BEGIN_ALLOW_THREADS(f)
res = isatty((int)fileno(f->f_fp));
FILE_END_ALLOW_THREADS(f)
return PyBool_FromLong(res);
}
#if BUFSIZ < 8192
#define SMALLCHUNK 8192
#else
#define SMALLCHUNK BUFSIZ
#endif
#if SIZEOF_INT < 4
#define BIGCHUNK (512 * 32)
#else
#define BIGCHUNK (512 * 1024)
#endif
static size_t
new_buffersize(PyFileObject *f, size_t currentsize) {
#if defined(HAVE_FSTAT)
off_t pos, end;
struct stat st;
if (fstat(fileno(f->f_fp), &st) == 0) {
end = st.st_size;
pos = lseek(fileno(f->f_fp), 0L, SEEK_CUR);
if (pos >= 0) {
pos = ftell(f->f_fp);
}
if (pos < 0)
clearerr(f->f_fp);
if (end > pos && pos >= 0)
return currentsize + end - pos + 1;
}
#endif
if (currentsize > SMALLCHUNK) {
if (currentsize <= BIGCHUNK)
return currentsize + currentsize;
else
return currentsize + BIGCHUNK;
}
return currentsize + SMALLCHUNK;
}
#if defined(EWOULDBLOCK) && defined(EAGAIN) && EWOULDBLOCK != EAGAIN
#define BLOCKED_ERRNO(x) ((x) == EWOULDBLOCK || (x) == EAGAIN)
#else
#if defined(EWOULDBLOCK)
#define BLOCKED_ERRNO(x) ((x) == EWOULDBLOCK)
#else
#if defined(EAGAIN)
#define BLOCKED_ERRNO(x) ((x) == EAGAIN)
#else
#define BLOCKED_ERRNO(x) 0
#endif
#endif
#endif
static PyObject *
file_read(PyFileObject *f, PyObject *args) {
long bytesrequested = -1;
size_t bytesread, buffersize, chunksize;
PyObject *v;
if (f->f_fp == NULL)
return err_closed();
if (f->f_buf != NULL &&
(f->f_bufend - f->f_bufptr) > 0 &&
f->f_buf[0] != '\0')
return err_iterbuffered();
if (!PyArg_ParseTuple(args, "|l:read", &bytesrequested))
return NULL;
if (bytesrequested < 0)
buffersize = new_buffersize(f, (size_t)0);
else
buffersize = bytesrequested;
if (buffersize > PY_SSIZE_T_MAX) {
PyErr_SetString(PyExc_OverflowError,
"requested number of bytes is more than a Python string can hold");
return NULL;
}
v = PyString_FromStringAndSize((char *)NULL, buffersize);
if (v == NULL)
return NULL;
bytesread = 0;
for (;;) {
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
chunksize = Py_UniversalNewlineFread(BUF(v) + bytesread,
buffersize - bytesread, f->f_fp, (PyObject *)f);
FILE_END_ALLOW_THREADS(f)
if (chunksize == 0) {
if (!ferror(f->f_fp))
break;
clearerr(f->f_fp);
if (bytesread > 0 && BLOCKED_ERRNO(errno))
break;
PyErr_SetFromErrno(PyExc_IOError);
Py_DECREF(v);
return NULL;
}
bytesread += chunksize;
if (bytesread < buffersize) {
clearerr(f->f_fp);
break;
}
if (bytesrequested < 0) {
buffersize = new_buffersize(f, buffersize);
if (_PyString_Resize(&v, buffersize) < 0)
return NULL;
} else {
break;
}
}
if (bytesread != buffersize)
_PyString_Resize(&v, bytesread);
return v;
}
static PyObject *
file_readinto(PyFileObject *f, PyObject *args) {
char *ptr;
Py_ssize_t ntodo;
Py_ssize_t ndone, nnow;
Py_buffer pbuf;
if (f->f_fp == NULL)
return err_closed();
if (f->f_buf != NULL &&
(f->f_bufend - f->f_bufptr) > 0 &&
f->f_buf[0] != '\0')
return err_iterbuffered();
if (!PyArg_ParseTuple(args, "w*", &pbuf))
return NULL;
ptr = pbuf.buf;
ntodo = pbuf.len;
ndone = 0;
while (ntodo > 0) {
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
nnow = Py_UniversalNewlineFread(ptr+ndone, ntodo, f->f_fp,
(PyObject *)f);
FILE_END_ALLOW_THREADS(f)
if (nnow == 0) {
if (!ferror(f->f_fp))
break;
PyErr_SetFromErrno(PyExc_IOError);
clearerr(f->f_fp);
PyBuffer_Release(&pbuf);
return NULL;
}
ndone += nnow;
ntodo -= nnow;
}
PyBuffer_Release(&pbuf);
return PyInt_FromSsize_t(ndone);
}
#if !defined(USE_FGETS_IN_GETLINE) && !defined(HAVE_GETC_UNLOCKED)
#define USE_FGETS_IN_GETLINE
#endif
#if defined(DONT_USE_FGETS_IN_GETLINE) && defined(USE_FGETS_IN_GETLINE)
#undef USE_FGETS_IN_GETLINE
#endif
#if defined(USE_FGETS_IN_GETLINE)
static PyObject*
getline_via_fgets(PyFileObject *f, FILE *fp) {
#define INITBUFSIZE 100
#define MAXBUFSIZE 300
char* p;
char buf[MAXBUFSIZE];
PyObject* v;
char* pvfree;
char* pvend;
size_t nfree;
size_t total_v_size;
size_t increment;
size_t prev_v_size;
total_v_size = INITBUFSIZE;
pvfree = buf;
for (;;) {
FILE_BEGIN_ALLOW_THREADS(f)
pvend = buf + total_v_size;
nfree = pvend - pvfree;
memset(pvfree, '\n', nfree);
assert(nfree < INT_MAX);
p = fgets(pvfree, (int)nfree, fp);
FILE_END_ALLOW_THREADS(f)
if (p == NULL) {
clearerr(fp);
if (PyErr_CheckSignals())
return NULL;
v = PyString_FromStringAndSize(buf, pvfree - buf);
return v;
}
p = memchr(pvfree, '\n', nfree);
if (p != NULL) {
if (p+1 < pvend && *(p+1) == '\0') {
++p;
} else {
assert(p > pvfree && *(p-1) == '\0');
--p;
}
v = PyString_FromStringAndSize(buf, p - buf);
return v;
}
assert(*(pvend-1) == '\0');
if (pvfree == buf) {
pvfree = pvend - 1;
total_v_size = MAXBUFSIZE;
} else
break;
}
total_v_size = MAXBUFSIZE << 1;
v = PyString_FromStringAndSize((char*)NULL, (int)total_v_size);
if (v == NULL)
return v;
memcpy(BUF(v), buf, MAXBUFSIZE-1);
pvfree = BUF(v) + MAXBUFSIZE - 1;
for (;;) {
FILE_BEGIN_ALLOW_THREADS(f)
pvend = BUF(v) + total_v_size;
nfree = pvend - pvfree;
memset(pvfree, '\n', nfree);
assert(nfree < INT_MAX);
p = fgets(pvfree, (int)nfree, fp);
FILE_END_ALLOW_THREADS(f)
if (p == NULL) {
clearerr(fp);
if (PyErr_CheckSignals()) {
Py_DECREF(v);
return NULL;
}
p = pvfree;
break;
}
p = memchr(pvfree, '\n', nfree);
if (p != NULL) {
if (p+1 < pvend && *(p+1) == '\0') {
++p;
break;
}
assert(p > pvfree && *(p-1) == '\0');
--p;
break;
}
assert(*(pvend-1) == '\0');
increment = total_v_size >> 2;
prev_v_size = total_v_size;
total_v_size += increment;
if (total_v_size <= prev_v_size ||
total_v_size > PY_SSIZE_T_MAX) {
PyErr_SetString(PyExc_OverflowError,
"line is longer than a Python string can hold");
Py_DECREF(v);
return NULL;
}
if (_PyString_Resize(&v, (int)total_v_size) < 0)
return NULL;
pvfree = BUF(v) + (prev_v_size - 1);
}
if (BUF(v) + total_v_size != p)
_PyString_Resize(&v, p - BUF(v));
return v;
#undef INITBUFSIZE
#undef MAXBUFSIZE
}
#endif
static PyObject *
get_line(PyFileObject *f, int n) {
FILE *fp = f->f_fp;
int c;
char *buf, *end;
size_t total_v_size;
size_t used_v_size;
size_t increment;
PyObject *v;
int newlinetypes = f->f_newlinetypes;
int skipnextlf = f->f_skipnextlf;
int univ_newline = f->f_univ_newline;
#if defined(USE_FGETS_IN_GETLINE)
if (n <= 0 && !univ_newline )
return getline_via_fgets(f, fp);
#endif
total_v_size = n > 0 ? n : 100;
v = PyString_FromStringAndSize((char *)NULL, total_v_size);
if (v == NULL)
return NULL;
buf = BUF(v);
end = buf + total_v_size;
for (;;) {
FILE_BEGIN_ALLOW_THREADS(f)
FLOCKFILE(fp);
if (univ_newline) {
c = 'x';
while ( buf != end && (c = GETC(fp)) != EOF ) {
if (skipnextlf ) {
skipnextlf = 0;
if (c == '\n') {
newlinetypes |= NEWLINE_CRLF;
c = GETC(fp);
if (c == EOF) break;
} else {
newlinetypes |= NEWLINE_CR;
}
}
if (c == '\r') {
skipnextlf = 1;
c = '\n';
} else if ( c == '\n')
newlinetypes |= NEWLINE_LF;
*buf++ = c;
if (c == '\n') break;
}
if ( c == EOF && skipnextlf )
newlinetypes |= NEWLINE_CR;
} else
while ((c = GETC(fp)) != EOF &&
(*buf++ = c) != '\n' &&
buf != end)
;
FUNLOCKFILE(fp);
FILE_END_ALLOW_THREADS(f)
f->f_newlinetypes = newlinetypes;
f->f_skipnextlf = skipnextlf;
if (c == '\n')
break;
if (c == EOF) {
if (ferror(fp)) {
PyErr_SetFromErrno(PyExc_IOError);
clearerr(fp);
Py_DECREF(v);
return NULL;
}
clearerr(fp);
if (PyErr_CheckSignals()) {
Py_DECREF(v);
return NULL;
}
break;
}
if (n > 0)
break;
used_v_size = total_v_size;
increment = total_v_size >> 2;
total_v_size += increment;
if (total_v_size > PY_SSIZE_T_MAX) {
PyErr_SetString(PyExc_OverflowError,
"line is longer than a Python string can hold");
Py_DECREF(v);
return NULL;
}
if (_PyString_Resize(&v, total_v_size) < 0)
return NULL;
buf = BUF(v) + used_v_size;
end = BUF(v) + total_v_size;
}
used_v_size = buf - BUF(v);
if (used_v_size != total_v_size)
_PyString_Resize(&v, used_v_size);
return v;
}
PyObject *
PyFile_GetLine(PyObject *f, int n) {
PyObject *result;
if (f == NULL) {
PyErr_BadInternalCall();
return NULL;
}
if (PyFile_Check(f)) {
PyFileObject *fo = (PyFileObject *)f;
if (fo->f_fp == NULL)
return err_closed();
if (fo->f_buf != NULL &&
(fo->f_bufend - fo->f_bufptr) > 0 &&
fo->f_buf[0] != '\0')
return err_iterbuffered();
result = get_line(fo, n);
} else {
PyObject *reader;
PyObject *args;
reader = PyObject_GetAttrString(f, "readline");
if (reader == NULL)
return NULL;
if (n <= 0)
args = PyTuple_New(0);
else
args = Py_BuildValue("(i)", n);
if (args == NULL) {
Py_DECREF(reader);
return NULL;
}
result = PyEval_CallObject(reader, args);
Py_DECREF(reader);
Py_DECREF(args);
if (result != NULL && !PyString_Check(result) &&
!PyUnicode_Check(result)) {
Py_DECREF(result);
result = NULL;
PyErr_SetString(PyExc_TypeError,
"object.readline() returned non-string");
}
}
if (n < 0 && result != NULL && PyString_Check(result)) {
char *s = PyString_AS_STRING(result);
Py_ssize_t len = PyString_GET_SIZE(result);
if (len == 0) {
Py_DECREF(result);
result = NULL;
PyErr_SetString(PyExc_EOFError,
"EOF when reading a line");
} else if (s[len-1] == '\n') {
if (result->ob_refcnt == 1)
_PyString_Resize(&result, len-1);
else {
PyObject *v;
v = PyString_FromStringAndSize(s, len-1);
Py_DECREF(result);
result = v;
}
}
}
#if defined(Py_USING_UNICODE)
if (n < 0 && result != NULL && PyUnicode_Check(result)) {
Py_UNICODE *s = PyUnicode_AS_UNICODE(result);
Py_ssize_t len = PyUnicode_GET_SIZE(result);
if (len == 0) {
Py_DECREF(result);
result = NULL;
PyErr_SetString(PyExc_EOFError,
"EOF when reading a line");
} else if (s[len-1] == '\n') {
if (result->ob_refcnt == 1)
PyUnicode_Resize(&result, len-1);
else {
PyObject *v;
v = PyUnicode_FromUnicode(s, len-1);
Py_DECREF(result);
result = v;
}
}
}
#endif
return result;
}
static PyObject *
file_readline(PyFileObject *f, PyObject *args) {
int n = -1;
if (f->f_fp == NULL)
return err_closed();
if (f->f_buf != NULL &&
(f->f_bufend - f->f_bufptr) > 0 &&
f->f_buf[0] != '\0')
return err_iterbuffered();
if (!PyArg_ParseTuple(args, "|i:readline", &n))
return NULL;
if (n == 0)
return PyString_FromString("");
if (n < 0)
n = 0;
return get_line(f, n);
}
static PyObject *
file_readlines(PyFileObject *f, PyObject *args) {
long sizehint = 0;
PyObject *list = NULL;
PyObject *line;
char small_buffer[SMALLCHUNK];
char *buffer = small_buffer;
size_t buffersize = SMALLCHUNK;
PyObject *big_buffer = NULL;
size_t nfilled = 0;
size_t nread;
size_t totalread = 0;
char *p, *q, *end;
int err;
int shortread = 0;
if (f->f_fp == NULL)
return err_closed();
if (f->f_buf != NULL &&
(f->f_bufend - f->f_bufptr) > 0 &&
f->f_buf[0] != '\0')
return err_iterbuffered();
if (!PyArg_ParseTuple(args, "|l:readlines", &sizehint))
return NULL;
if ((list = PyList_New(0)) == NULL)
return NULL;
for (;;) {
if (shortread)
nread = 0;
else {
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
nread = Py_UniversalNewlineFread(buffer+nfilled,
buffersize-nfilled, f->f_fp, (PyObject *)f);
FILE_END_ALLOW_THREADS(f)
shortread = (nread < buffersize-nfilled);
}
if (nread == 0) {
sizehint = 0;
if (!ferror(f->f_fp))
break;
PyErr_SetFromErrno(PyExc_IOError);
clearerr(f->f_fp);
goto error;
}
totalread += nread;
p = (char *)memchr(buffer+nfilled, '\n', nread);
if (p == NULL) {
nfilled += nread;
buffersize *= 2;
if (buffersize > PY_SSIZE_T_MAX) {
PyErr_SetString(PyExc_OverflowError,
"line is longer than a Python string can hold");
goto error;
}
if (big_buffer == NULL) {
big_buffer = PyString_FromStringAndSize(
NULL, buffersize);
if (big_buffer == NULL)
goto error;
buffer = PyString_AS_STRING(big_buffer);
memcpy(buffer, small_buffer, nfilled);
} else {
if ( _PyString_Resize(&big_buffer, buffersize) < 0 )
goto error;
buffer = PyString_AS_STRING(big_buffer);
}
continue;
}
end = buffer+nfilled+nread;
q = buffer;
do {
p++;
line = PyString_FromStringAndSize(q, p-q);
if (line == NULL)
goto error;
err = PyList_Append(list, line);
Py_DECREF(line);
if (err != 0)
goto error;
q = p;
p = (char *)memchr(q, '\n', end-q);
} while (p != NULL);
nfilled = end-q;
memmove(buffer, q, nfilled);
if (sizehint > 0)
if (totalread >= (size_t)sizehint)
break;
}
if (nfilled != 0) {
line = PyString_FromStringAndSize(buffer, nfilled);
if (line == NULL)
goto error;
if (sizehint > 0) {
PyObject *rest = get_line(f, 0);
if (rest == NULL) {
Py_DECREF(line);
goto error;
}
PyString_Concat(&line, rest);
Py_DECREF(rest);
if (line == NULL)
goto error;
}
err = PyList_Append(list, line);
Py_DECREF(line);
if (err != 0)
goto error;
}
cleanup:
Py_XDECREF(big_buffer);
return list;
error:
Py_CLEAR(list);
goto cleanup;
}
static PyObject *
file_write(PyFileObject *f, PyObject *args) {
Py_buffer pbuf;
char *s;
Py_ssize_t n, n2;
if (f->f_fp == NULL)
return err_closed();
if (f->f_binary) {
if (!PyArg_ParseTuple(args, "s*", &pbuf))
return NULL;
s = pbuf.buf;
n = pbuf.len;
} else if (!PyArg_ParseTuple(args, "t#", &s, &n))
return NULL;
f->f_softspace = 0;
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
n2 = fwrite(s, 1, n, f->f_fp);
FILE_END_ALLOW_THREADS(f)
if (f->f_binary)
PyBuffer_Release(&pbuf);
if (n2 != n) {
PyErr_SetFromErrno(PyExc_IOError);
clearerr(f->f_fp);
return NULL;
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
file_writelines(PyFileObject *f, PyObject *seq) {
#define CHUNKSIZE 1000
PyObject *list, *line;
PyObject *it;
PyObject *result;
int index, islist;
Py_ssize_t i, j, nwritten, len;
assert(seq != NULL);
if (f->f_fp == NULL)
return err_closed();
result = NULL;
list = NULL;
islist = PyList_Check(seq);
if (islist)
it = NULL;
else {
it = PyObject_GetIter(seq);
if (it == NULL) {
PyErr_SetString(PyExc_TypeError,
"writelines() requires an iterable argument");
return NULL;
}
list = PyList_New(CHUNKSIZE);
if (list == NULL)
goto error;
}
for (index = 0; ; index += CHUNKSIZE) {
if (islist) {
Py_XDECREF(list);
list = PyList_GetSlice(seq, index, index+CHUNKSIZE);
if (list == NULL)
goto error;
j = PyList_GET_SIZE(list);
} else {
for (j = 0; j < CHUNKSIZE; j++) {
line = PyIter_Next(it);
if (line == NULL) {
if (PyErr_Occurred())
goto error;
break;
}
PyList_SetItem(list, j, line);
}
}
if (j == 0)
break;
for (i = 0; i < j; i++) {
PyObject *v = PyList_GET_ITEM(list, i);
if (!PyString_Check(v)) {
const char *buffer;
if (((f->f_binary &&
PyObject_AsReadBuffer(v,
(const void**)&buffer,
&len)) ||
PyObject_AsCharBuffer(v,
&buffer,
&len))) {
PyErr_SetString(PyExc_TypeError,
"writelines() argument must be a sequence of strings");
goto error;
}
line = PyString_FromStringAndSize(buffer,
len);
if (line == NULL)
goto error;
Py_DECREF(v);
PyList_SET_ITEM(list, i, line);
}
}
f->f_softspace = 0;
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
for (i = 0; i < j; i++) {
line = PyList_GET_ITEM(list, i);
len = PyString_GET_SIZE(line);
nwritten = fwrite(PyString_AS_STRING(line),
1, len, f->f_fp);
if (nwritten != len) {
FILE_ABORT_ALLOW_THREADS(f)
PyErr_SetFromErrno(PyExc_IOError);
clearerr(f->f_fp);
goto error;
}
}
FILE_END_ALLOW_THREADS(f)
if (j < CHUNKSIZE)
break;
}
Py_INCREF(Py_None);
result = Py_None;
error:
Py_XDECREF(list);
Py_XDECREF(it);
return result;
#undef CHUNKSIZE
}
static PyObject *
file_self(PyFileObject *f) {
if (f->f_fp == NULL)
return err_closed();
Py_INCREF(f);
return (PyObject *)f;
}
static PyObject *
file_xreadlines(PyFileObject *f) {
if (PyErr_WarnPy3k("f.xreadlines() not supported in 3.x, "
"try 'for line in f' instead", 1) < 0)
return NULL;
return file_self(f);
}
static PyObject *
file_exit(PyObject *f, PyObject *args) {
PyObject *ret = PyObject_CallMethod(f, "close", NULL);
if (!ret)
return NULL;
Py_DECREF(ret);
Py_RETURN_NONE;
}
PyDoc_STRVAR(readline_doc,
"readline([size]) -> next line from the file, as a string.\n"
"\n"
"Retain newline. A non-negative size argument limits the maximum\n"
"number of bytes to return (an incomplete line may be returned then).\n"
"Return an empty string at EOF.");
PyDoc_STRVAR(read_doc,
"read([size]) -> read at most size bytes, returned as a string.\n"
"\n"
"If the size argument is negative or omitted, read until EOF is reached.\n"
"Notice that when in non-blocking mode, less data than what was requested\n"
"may be returned, even if no size parameter was given.");
PyDoc_STRVAR(write_doc,
"write(str) -> None. Write string str to file.\n"
"\n"
"Note that due to buffering, flush() or close() may be needed before\n"
"the file on disk reflects the data written.");
PyDoc_STRVAR(fileno_doc,
"fileno() -> integer \"file descriptor\".\n"
"\n"
"This is needed for lower-level file interfaces, such os.read().");
PyDoc_STRVAR(seek_doc,
"seek(offset[, whence]) -> None. Move to new file position.\n"
"\n"
"Argument offset is a byte count. Optional argument whence defaults to\n"
"0 (offset from start of file, offset should be >= 0); other values are 1\n"
"(move relative to current position, positive or negative), and 2 (move\n"
"relative to end of file, usually negative, although many platforms allow\n"
"seeking beyond the end of a file). If the file is opened in text mode,\n"
"only offsets returned by tell() are legal. Use of other offsets causes\n"
"undefined behavior."
"\n"
"Note that not all file objects are seekable.");
#if defined(HAVE_FTRUNCATE)
PyDoc_STRVAR(truncate_doc,
"truncate([size]) -> None. Truncate the file to at most size bytes.\n"
"\n"
"Size defaults to the current file position, as returned by tell().");
#endif
PyDoc_STRVAR(tell_doc,
"tell() -> current file position, an integer (may be a long integer).");
PyDoc_STRVAR(readinto_doc,
"readinto() -> Undocumented. Don't use this; it may go away.");
PyDoc_STRVAR(readlines_doc,
"readlines([size]) -> list of strings, each a line from the file.\n"
"\n"
"Call readline() repeatedly and return a list of the lines so read.\n"
"The optional size argument, if given, is an approximate bound on the\n"
"total number of bytes in the lines returned.");
PyDoc_STRVAR(xreadlines_doc,
"xreadlines() -> returns self.\n"
"\n"
"For backward compatibility. File objects now include the performance\n"
"optimizations previously implemented in the xreadlines module.");
PyDoc_STRVAR(writelines_doc,
"writelines(sequence_of_strings) -> None. Write the strings to the file.\n"
"\n"
"Note that newlines are not added. The sequence can be any iterable object\n"
"producing strings. This is equivalent to calling write() for each string.");
PyDoc_STRVAR(flush_doc,
"flush() -> None. Flush the internal I/O buffer.");
PyDoc_STRVAR(close_doc,
"close() -> None or (perhaps) an integer. Close the file.\n"
"\n"
"Sets data attribute .closed to True. A closed file cannot be used for\n"
"further I/O operations. close() may be called more than once without\n"
"error. Some kinds of file objects (for example, opened by popen())\n"
"may return an exit status upon closing.");
PyDoc_STRVAR(isatty_doc,
"isatty() -> true or false. True if the file is connected to a tty device.");
PyDoc_STRVAR(enter_doc,
"__enter__() -> self.");
PyDoc_STRVAR(exit_doc,
"__exit__(*excinfo) -> None. Closes the file.");
static PyMethodDef file_methods[] = {
{"readline", (PyCFunction)file_readline, METH_VARARGS, readline_doc},
{"read", (PyCFunction)file_read, METH_VARARGS, read_doc},
{"write", (PyCFunction)file_write, METH_VARARGS, write_doc},
{"fileno", (PyCFunction)file_fileno, METH_NOARGS, fileno_doc},
{"seek", (PyCFunction)file_seek, METH_VARARGS, seek_doc},
#if defined(HAVE_FTRUNCATE)
{"truncate", (PyCFunction)file_truncate, METH_VARARGS, truncate_doc},
#endif
{"tell", (PyCFunction)file_tell, METH_NOARGS, tell_doc},
{"readinto", (PyCFunction)file_readinto, METH_VARARGS, readinto_doc},
{"readlines", (PyCFunction)file_readlines, METH_VARARGS, readlines_doc},
{"xreadlines",(PyCFunction)file_xreadlines, METH_NOARGS, xreadlines_doc},
{"writelines",(PyCFunction)file_writelines, METH_O, writelines_doc},
{"flush", (PyCFunction)file_flush, METH_NOARGS, flush_doc},
{"close", (PyCFunction)file_close, METH_NOARGS, close_doc},
{"isatty", (PyCFunction)file_isatty, METH_NOARGS, isatty_doc},
{"__enter__", (PyCFunction)file_self, METH_NOARGS, enter_doc},
{"__exit__", (PyCFunction)file_exit, METH_VARARGS, exit_doc},
{NULL, NULL}
};
#define OFF(x) offsetof(PyFileObject, x)
static PyMemberDef file_memberlist[] = {
{
"mode", T_OBJECT, OFF(f_mode), RO,
"file mode ('r', 'U', 'w', 'a', possibly with 'b' or '+' added)"
},
{
"name", T_OBJECT, OFF(f_name), RO,
"file name"
},
{
"encoding", T_OBJECT, OFF(f_encoding), RO,
"file encoding"
},
{
"errors", T_OBJECT, OFF(f_errors), RO,
"Unicode error handler"
},
{NULL}
};
static PyObject *
get_closed(PyFileObject *f, void *closure) {
return PyBool_FromLong((long)(f->f_fp == 0));
}
static PyObject *
get_newlines(PyFileObject *f, void *closure) {
switch (f->f_newlinetypes) {
case NEWLINE_UNKNOWN:
Py_INCREF(Py_None);
return Py_None;
case NEWLINE_CR:
return PyString_FromString("\r");
case NEWLINE_LF:
return PyString_FromString("\n");
case NEWLINE_CR|NEWLINE_LF:
return Py_BuildValue("(ss)", "\r", "\n");
case NEWLINE_CRLF:
return PyString_FromString("\r\n");
case NEWLINE_CR|NEWLINE_CRLF:
return Py_BuildValue("(ss)", "\r", "\r\n");
case NEWLINE_LF|NEWLINE_CRLF:
return Py_BuildValue("(ss)", "\n", "\r\n");
case NEWLINE_CR|NEWLINE_LF|NEWLINE_CRLF:
return Py_BuildValue("(sss)", "\r", "\n", "\r\n");
default:
PyErr_Format(PyExc_SystemError,
"Unknown newlines value 0x%x\n",
f->f_newlinetypes);
return NULL;
}
}
static PyObject *
get_softspace(PyFileObject *f, void *closure) {
if (PyErr_WarnPy3k("file.softspace not supported in 3.x", 1) < 0)
return NULL;
return PyInt_FromLong(f->f_softspace);
}
static int
set_softspace(PyFileObject *f, PyObject *value) {
int new;
if (PyErr_WarnPy3k("file.softspace not supported in 3.x", 1) < 0)
return -1;
if (value == NULL) {
PyErr_SetString(PyExc_TypeError,
"can't delete softspace attribute");
return -1;
}
new = PyInt_AsLong(value);
if (new == -1 && PyErr_Occurred())
return -1;
f->f_softspace = new;
return 0;
}
static PyGetSetDef file_getsetlist[] = {
{"closed", (getter)get_closed, NULL, "True if the file is closed"},
{
"newlines", (getter)get_newlines, NULL,
"end-of-line convention used in this file"
},
{
"softspace", (getter)get_softspace, (setter)set_softspace,
"flag indicating that a space needs to be printed; used by print"
},
{0},
};
static void
drop_readahead(PyFileObject *f) {
if (f->f_buf != NULL) {
PyMem_Free(f->f_buf);
f->f_buf = NULL;
}
}
static int
readahead(PyFileObject *f, int bufsize) {
Py_ssize_t chunksize;
if (f->f_buf != NULL) {
if( (f->f_bufend - f->f_bufptr) >= 1)
return 0;
else
drop_readahead(f);
}
if ((f->f_buf = (char *)PyMem_Malloc(bufsize)) == NULL) {
PyErr_NoMemory();
return -1;
}
FILE_BEGIN_ALLOW_THREADS(f)
errno = 0;
chunksize = Py_UniversalNewlineFread(
f->f_buf, bufsize, f->f_fp, (PyObject *)f);
FILE_END_ALLOW_THREADS(f)
if (chunksize == 0) {
if (ferror(f->f_fp)) {
PyErr_SetFromErrno(PyExc_IOError);
clearerr(f->f_fp);
drop_readahead(f);
return -1;
}
}
f->f_bufptr = f->f_buf;
f->f_bufend = f->f_buf + chunksize;
return 0;
}
static PyStringObject *
readahead_get_line_skip(PyFileObject *f, int skip, int bufsize) {
PyStringObject* s;
char *bufptr;
char *buf;
Py_ssize_t len;
if (f->f_buf == NULL)
if (readahead(f, bufsize) < 0)
return NULL;
len = f->f_bufend - f->f_bufptr;
if (len == 0)
return (PyStringObject *)
PyString_FromStringAndSize(NULL, skip);
bufptr = (char *)memchr(f->f_bufptr, '\n', len);
if (bufptr != NULL) {
bufptr++;
len = bufptr - f->f_bufptr;
s = (PyStringObject *)
PyString_FromStringAndSize(NULL, skip+len);
if (s == NULL)
return NULL;
memcpy(PyString_AS_STRING(s)+skip, f->f_bufptr, len);
f->f_bufptr = bufptr;
if (bufptr == f->f_bufend)
drop_readahead(f);
} else {
bufptr = f->f_bufptr;
buf = f->f_buf;
f->f_buf = NULL;
assert(skip+len < INT_MAX);
s = readahead_get_line_skip(
f, (int)(skip+len), bufsize + (bufsize>>2) );
if (s == NULL) {
PyMem_Free(buf);
return NULL;
}
memcpy(PyString_AS_STRING(s)+skip, bufptr, len);
PyMem_Free(buf);
}
return s;
}
#define READAHEAD_BUFSIZE 8192
static PyObject *
file_iternext(PyFileObject *f) {
PyStringObject* l;
if (f->f_fp == NULL)
return err_closed();
l = readahead_get_line_skip(f, 0, READAHEAD_BUFSIZE);
if (l == NULL || PyString_GET_SIZE(l) == 0) {
Py_XDECREF(l);
return NULL;
}
return (PyObject *)l;
}
static PyObject *
file_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyObject *self;
static PyObject *not_yet_string;
assert(type != NULL && type->tp_alloc != NULL);
if (not_yet_string == NULL) {
not_yet_string = PyString_InternFromString("<uninitialized file>");
if (not_yet_string == NULL)
return NULL;
}
self = type->tp_alloc(type, 0);
if (self != NULL) {
Py_INCREF(not_yet_string);
((PyFileObject *)self)->f_name = not_yet_string;
Py_INCREF(not_yet_string);
((PyFileObject *)self)->f_mode = not_yet_string;
Py_INCREF(Py_None);
((PyFileObject *)self)->f_encoding = Py_None;
Py_INCREF(Py_None);
((PyFileObject *)self)->f_errors = Py_None;
((PyFileObject *)self)->weakreflist = NULL;
((PyFileObject *)self)->unlocked_count = 0;
}
return self;
}
static int
file_init(PyObject *self, PyObject *args, PyObject *kwds) {
PyFileObject *foself = (PyFileObject *)self;
int ret = 0;
static char *kwlist[] = {"name", "mode", "buffering", 0};
char *name = NULL;
char *mode = "r";
int bufsize = -1;
int wideargument = 0;
assert(PyFile_Check(self));
if (foself->f_fp != NULL) {
PyObject *closeresult = file_close(foself);
if (closeresult == NULL)
return -1;
Py_DECREF(closeresult);
}
#if defined(Py_WIN_WIDE_FILENAMES)
if (GetVersion() < 0x80000000) {
PyObject *po;
if (PyArg_ParseTupleAndKeywords(args, kwds, "U|si:file",
kwlist, &po, &mode, &bufsize)) {
wideargument = 1;
if (fill_file_fields(foself, NULL, po, mode,
fclose) == NULL)
goto Error;
} else {
PyErr_Clear();
}
}
#endif
if (!wideargument) {
PyObject *o_name;
if (!PyArg_ParseTupleAndKeywords(args, kwds, "et|si:file", kwlist,
Py_FileSystemDefaultEncoding,
&name,
&mode, &bufsize))
return -1;
if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|si:file",
kwlist, &o_name, &mode,
&bufsize))
goto Error;
if (fill_file_fields(foself, NULL, o_name, mode,
fclose) == NULL)
goto Error;
}
if (open_the_file(foself, name, mode) == NULL)
goto Error;
foself->f_setbuf = NULL;
PyFile_SetBufSize(self, bufsize);
goto Done;
Error:
ret = -1;
Done:
PyMem_Free(name);
return ret;
}
PyDoc_VAR(file_doc) =
PyDoc_STR(
"file(name[, mode[, buffering]]) -> file object\n"
"\n"
"Open a file. The mode can be 'r', 'w' or 'a' for reading (default),\n"
"writing or appending. The file will be created if it doesn't exist\n"
"when opened for writing or appending; it will be truncated when\n"
"opened for writing. Add a 'b' to the mode for binary files.\n"
"Add a '+' to the mode to allow simultaneous reading and writing.\n"
"If the buffering argument is given, 0 means unbuffered, 1 means line\n"
"buffered, and larger numbers specify the buffer size. The preferred way\n"
"to open a file is with the builtin open() function.\n"
)
PyDoc_STR(
"Add a 'U' to mode to open the file for input with universal newline\n"
"support. Any line ending in the input file will be seen as a '\\n'\n"
"in Python. Also, a file so opened gains the attribute 'newlines';\n"
"the value for this attribute is one of None (no newline read yet),\n"
"'\\r', '\\n', '\\r\\n' or a tuple containing all the newline types seen.\n"
"\n"
"'U' cannot be combined with 'w' or '+' mode.\n"
);
PyTypeObject PyFile_Type = {
PyVarObject_HEAD_INIT(&PyType_Type, 0)
"file",
sizeof(PyFileObject),
0,
(destructor)file_dealloc,
0,
0,
0,
0,
(reprfunc)file_repr,
0,
0,
0,
0,
0,
0,
PyObject_GenericGetAttr,
PyObject_GenericSetAttr,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_WEAKREFS,
file_doc,
0,
0,
0,
offsetof(PyFileObject, weakreflist),
(getiterfunc)file_self,
(iternextfunc)file_iternext,
file_methods,
file_memberlist,
file_getsetlist,
0,
0,
0,
0,
0,
file_init,
PyType_GenericAlloc,
file_new,
PyObject_Del,
};
int
PyFile_SoftSpace(PyObject *f, int newflag) {
long oldflag = 0;
if (f == NULL) {
} else if (PyFile_Check(f)) {
oldflag = ((PyFileObject *)f)->f_softspace;
((PyFileObject *)f)->f_softspace = newflag;
} else {
PyObject *v;
v = PyObject_GetAttrString(f, "softspace");
if (v == NULL)
PyErr_Clear();
else {
if (PyInt_Check(v))
oldflag = PyInt_AsLong(v);
assert(oldflag < INT_MAX);
Py_DECREF(v);
}
v = PyInt_FromLong((long)newflag);
if (v == NULL)
PyErr_Clear();
else {
if (PyObject_SetAttrString(f, "softspace", v) != 0)
PyErr_Clear();
Py_DECREF(v);
}
}
return (int)oldflag;
}
int
PyFile_WriteObject(PyObject *v, PyObject *f, int flags) {
PyObject *writer, *value, *args, *result;
if (f == NULL) {
PyErr_SetString(PyExc_TypeError, "writeobject with NULL file");
return -1;
} else if (PyFile_Check(f)) {
PyFileObject *fobj = (PyFileObject *) f;
#if defined(Py_USING_UNICODE)
PyObject *enc = fobj->f_encoding;
int result;
#endif
if (fobj->f_fp == NULL) {
err_closed();
return -1;
}
#if defined(Py_USING_UNICODE)
if ((flags & Py_PRINT_RAW) &&
PyUnicode_Check(v) && enc != Py_None) {
char *cenc = PyString_AS_STRING(enc);
char *errors = fobj->f_errors == Py_None ?
"strict" : PyString_AS_STRING(fobj->f_errors);
value = PyUnicode_AsEncodedString(v, cenc, errors);
if (value == NULL)
return -1;
} else {
value = v;
Py_INCREF(value);
}
result = file_PyObject_Print(value, fobj, flags);
Py_DECREF(value);
return result;
#else
return file_PyObject_Print(v, fobj, flags);
#endif
}
writer = PyObject_GetAttrString(f, "write");
if (writer == NULL)
return -1;
if (flags & Py_PRINT_RAW) {
if (PyUnicode_Check(v)) {
value = v;
Py_INCREF(value);
} else
value = PyObject_Str(v);
} else
value = PyObject_Repr(v);
if (value == NULL) {
Py_DECREF(writer);
return -1;
}
args = PyTuple_Pack(1, value);
if (args == NULL) {
Py_DECREF(value);
Py_DECREF(writer);
return -1;
}
result = PyEval_CallObject(writer, args);
Py_DECREF(args);
Py_DECREF(value);
Py_DECREF(writer);
if (result == NULL)
return -1;
Py_DECREF(result);
return 0;
}
int
PyFile_WriteString(const char *s, PyObject *f) {
if (f == NULL) {
if (!PyErr_Occurred())
PyErr_SetString(PyExc_SystemError,
"null file for PyFile_WriteString");
return -1;
} else if (PyFile_Check(f)) {
PyFileObject *fobj = (PyFileObject *) f;
FILE *fp = PyFile_AsFile(f);
if (fp == NULL) {
err_closed();
return -1;
}
FILE_BEGIN_ALLOW_THREADS(fobj)
fputs(s, fp);
FILE_END_ALLOW_THREADS(fobj)
return 0;
} else if (!PyErr_Occurred()) {
PyObject *v = PyString_FromString(s);
int err;
if (v == NULL)
return -1;
err = PyFile_WriteObject(v, f, Py_PRINT_RAW);
Py_DECREF(v);
return err;
} else
return -1;
}
int PyObject_AsFileDescriptor(PyObject *o) {
int fd;
PyObject *meth;
if (PyInt_Check(o)) {
fd = PyInt_AsLong(o);
} else if (PyLong_Check(o)) {
fd = PyLong_AsLong(o);
} else if ((meth = PyObject_GetAttrString(o, "fileno")) != NULL) {
PyObject *fno = PyEval_CallObject(meth, NULL);
Py_DECREF(meth);
if (fno == NULL)
return -1;
if (PyInt_Check(fno)) {
fd = PyInt_AsLong(fno);
Py_DECREF(fno);
} else if (PyLong_Check(fno)) {
fd = PyLong_AsLong(fno);
Py_DECREF(fno);
} else {
PyErr_SetString(PyExc_TypeError,
"fileno() returned a non-integer");
Py_DECREF(fno);
return -1;
}
} else {
PyErr_SetString(PyExc_TypeError,
"argument must be an int, or have a fileno() method.");
return -1;
}
if (fd < 0) {
PyErr_Format(PyExc_ValueError,
"file descriptor cannot be a negative integer (%i)",
fd);
return -1;
}
return fd;
}
#undef fgets
#undef fread
char *
Py_UniversalNewlineFgets(char *buf, int n, FILE *stream, PyObject *fobj) {
char *p = buf;
int c;
int newlinetypes = 0;
int skipnextlf = 0;
int univ_newline = 1;
if (fobj) {
if (!PyFile_Check(fobj)) {
errno = ENXIO;
return NULL;
}
univ_newline = ((PyFileObject *)fobj)->f_univ_newline;
if ( !univ_newline )
return fgets(buf, n, stream);
newlinetypes = ((PyFileObject *)fobj)->f_newlinetypes;
skipnextlf = ((PyFileObject *)fobj)->f_skipnextlf;
}
FLOCKFILE(stream);
c = 'x';
while (--n > 0 && (c = GETC(stream)) != EOF ) {
if (skipnextlf ) {
skipnextlf = 0;
if (c == '\n') {
newlinetypes |= NEWLINE_CRLF;
c = GETC(stream);
if (c == EOF) break;
} else {
newlinetypes |= NEWLINE_CR;
}
}
if (c == '\r') {
skipnextlf = 1;
c = '\n';
} else if ( c == '\n') {
newlinetypes |= NEWLINE_LF;
}
*p++ = c;
if (c == '\n') break;
}
if ( c == EOF && skipnextlf )
newlinetypes |= NEWLINE_CR;
FUNLOCKFILE(stream);
*p = '\0';
if (fobj) {
((PyFileObject *)fobj)->f_newlinetypes = newlinetypes;
((PyFileObject *)fobj)->f_skipnextlf = skipnextlf;
} else if ( skipnextlf ) {
c = GETC(stream);
if ( c != '\n' )
ungetc(c, stream);
}
if (p == buf)
return NULL;
return buf;
}
size_t
Py_UniversalNewlineFread(char *buf, size_t n,
FILE *stream, PyObject *fobj) {
char *dst = buf;
PyFileObject *f = (PyFileObject *)fobj;
int newlinetypes, skipnextlf;
assert(buf != NULL);
assert(stream != NULL);
if (!fobj || !PyFile_Check(fobj)) {
errno = ENXIO;
return 0;
}
if (!f->f_univ_newline)
return fread(buf, 1, n, stream);
newlinetypes = f->f_newlinetypes;
skipnextlf = f->f_skipnextlf;
while (n) {
size_t nread;
int shortread;
char *src = dst;
nread = fread(dst, 1, n, stream);
assert(nread <= n);
if (nread == 0)
break;
n -= nread;
shortread = n != 0;
while (nread--) {
char c = *src++;
if (c == '\r') {
*dst++ = '\n';
skipnextlf = 1;
} else if (skipnextlf && c == '\n') {
skipnextlf = 0;
newlinetypes |= NEWLINE_CRLF;
++n;
} else {
if (c == '\n')
newlinetypes |= NEWLINE_LF;
else if (skipnextlf)
newlinetypes |= NEWLINE_CR;
*dst++ = c;
skipnextlf = 0;
}
}
if (shortread) {
if (skipnextlf && feof(stream))
newlinetypes |= NEWLINE_CR;
break;
}
}
f->f_newlinetypes = newlinetypes;
f->f_skipnextlf = skipnextlf;
return dst - buf;
}
#if defined(__cplusplus)
}
#endif