#include "Python.h"
#if defined(MS_WINDOWS)
#include <windows.h>
#endif
static PyMethodDef errno_methods[] = {
{NULL, NULL}
};
static void
_inscode(PyObject *d, PyObject *de, char *name, int code) {
PyObject *u = PyString_FromString(name);
PyObject *v = PyInt_FromLong((long) code);
if (u && v) {
PyDict_SetItem(d, u, v);
PyDict_SetItem(de, v, u);
}
Py_XDECREF(u);
Py_XDECREF(v);
}
PyDoc_STRVAR(errno__doc__,
"This module makes available standard errno system symbols.\n\
\n\
The value of each symbol is the corresponding integer value,\n\
e.g., on most systems, errno.ENOENT equals the integer 2.\n\
\n\
The dictionary errno.errorcode maps numeric codes to symbol names,\n\
e.g., errno.errorcode[2] could be the string 'ENOENT'.\n\
\n\
Symbols that are not relevant to the underlying system are not defined.\n\
\n\
To map error codes to error messages, use the function os.strerror(),\n\
e.g. os.strerror(2) could return 'No such file or directory'.");
PyMODINIT_FUNC
initerrno(void) {
PyObject *m, *d, *de;
m = Py_InitModule3("errno", errno_methods, errno__doc__);
if (m == NULL)
return;
d = PyModule_GetDict(m);
de = PyDict_New();
if (!d || !de || PyDict_SetItemString(d, "errorcode", de) < 0)
return;
#define inscode(d, ds, de, name, code, comment) _inscode(d, de, name, code)
#if defined(ENODEV)
inscode(d, ds, de, "ENODEV", ENODEV, "No such device");
#endif
#if defined(ENOCSI)
inscode(d, ds, de, "ENOCSI", ENOCSI, "No CSI structure available");
#endif
#if defined(EHOSTUNREACH)
inscode(d, ds, de, "EHOSTUNREACH", EHOSTUNREACH, "No route to host");
#else
#if defined(WSAEHOSTUNREACH)
inscode(d, ds, de, "EHOSTUNREACH", WSAEHOSTUNREACH, "No route to host");
#endif
#endif
#if defined(ENOMSG)
inscode(d, ds, de, "ENOMSG", ENOMSG, "No message of desired type");
#endif
#if defined(EUCLEAN)
inscode(d, ds, de, "EUCLEAN", EUCLEAN, "Structure needs cleaning");
#endif
#if defined(EL2NSYNC)
inscode(d, ds, de, "EL2NSYNC", EL2NSYNC, "Level 2 not synchronized");
#endif
#if defined(EL2HLT)
inscode(d, ds, de, "EL2HLT", EL2HLT, "Level 2 halted");
#endif
#if defined(ENODATA)
inscode(d, ds, de, "ENODATA", ENODATA, "No data available");
#endif
#if defined(ENOTBLK)
inscode(d, ds, de, "ENOTBLK", ENOTBLK, "Block device required");
#endif
#if defined(ENOSYS)
inscode(d, ds, de, "ENOSYS", ENOSYS, "Function not implemented");
#endif
#if defined(EPIPE)
inscode(d, ds, de, "EPIPE", EPIPE, "Broken pipe");
#endif
#if defined(EINVAL)
inscode(d, ds, de, "EINVAL", EINVAL, "Invalid argument");
#else
#if defined(WSAEINVAL)
inscode(d, ds, de, "EINVAL", WSAEINVAL, "Invalid argument");
#endif
#endif
#if defined(EOVERFLOW)
inscode(d, ds, de, "EOVERFLOW", EOVERFLOW, "Value too large for defined data type");
#endif
#if defined(EADV)
inscode(d, ds, de, "EADV", EADV, "Advertise error");
#endif
#if defined(EINTR)
inscode(d, ds, de, "EINTR", EINTR, "Interrupted system call");
#else
#if defined(WSAEINTR)
inscode(d, ds, de, "EINTR", WSAEINTR, "Interrupted system call");
#endif
#endif
#if defined(EUSERS)
inscode(d, ds, de, "EUSERS", EUSERS, "Too many users");
#else
#if defined(WSAEUSERS)
inscode(d, ds, de, "EUSERS", WSAEUSERS, "Too many users");
#endif
#endif
#if defined(ENOTEMPTY)
inscode(d, ds, de, "ENOTEMPTY", ENOTEMPTY, "Directory not empty");
#else
#if defined(WSAENOTEMPTY)
inscode(d, ds, de, "ENOTEMPTY", WSAENOTEMPTY, "Directory not empty");
#endif
#endif
#if defined(ENOBUFS)
inscode(d, ds, de, "ENOBUFS", ENOBUFS, "No buffer space available");
#else
#if defined(WSAENOBUFS)
inscode(d, ds, de, "ENOBUFS", WSAENOBUFS, "No buffer space available");
#endif
#endif
#if defined(EPROTO)
inscode(d, ds, de, "EPROTO", EPROTO, "Protocol error");
#endif
#if defined(EREMOTE)
inscode(d, ds, de, "EREMOTE", EREMOTE, "Object is remote");
#else
#if defined(WSAEREMOTE)
inscode(d, ds, de, "EREMOTE", WSAEREMOTE, "Object is remote");
#endif
#endif
#if defined(ENAVAIL)
inscode(d, ds, de, "ENAVAIL", ENAVAIL, "No XENIX semaphores available");
#endif
#if defined(ECHILD)
inscode(d, ds, de, "ECHILD", ECHILD, "No child processes");
#endif
#if defined(ELOOP)
inscode(d, ds, de, "ELOOP", ELOOP, "Too many symbolic links encountered");
#else
#if defined(WSAELOOP)
inscode(d, ds, de, "ELOOP", WSAELOOP, "Too many symbolic links encountered");
#endif
#endif
#if defined(EXDEV)
inscode(d, ds, de, "EXDEV", EXDEV, "Cross-device link");
#endif
#if defined(E2BIG)
inscode(d, ds, de, "E2BIG", E2BIG, "Arg list too long");
#endif
#if defined(ESRCH)
inscode(d, ds, de, "ESRCH", ESRCH, "No such process");
#endif
#if defined(EMSGSIZE)
inscode(d, ds, de, "EMSGSIZE", EMSGSIZE, "Message too long");
#else
#if defined(WSAEMSGSIZE)
inscode(d, ds, de, "EMSGSIZE", WSAEMSGSIZE, "Message too long");
#endif
#endif
#if defined(EAFNOSUPPORT)
inscode(d, ds, de, "EAFNOSUPPORT", EAFNOSUPPORT, "Address family not supported by protocol");
#else
#if defined(WSAEAFNOSUPPORT)
inscode(d, ds, de, "EAFNOSUPPORT", WSAEAFNOSUPPORT, "Address family not supported by protocol");
#endif
#endif
#if defined(EBADR)
inscode(d, ds, de, "EBADR", EBADR, "Invalid request descriptor");
#endif
#if defined(EHOSTDOWN)
inscode(d, ds, de, "EHOSTDOWN", EHOSTDOWN, "Host is down");
#else
#if defined(WSAEHOSTDOWN)
inscode(d, ds, de, "EHOSTDOWN", WSAEHOSTDOWN, "Host is down");
#endif
#endif
#if defined(EPFNOSUPPORT)
inscode(d, ds, de, "EPFNOSUPPORT", EPFNOSUPPORT, "Protocol family not supported");
#else
#if defined(WSAEPFNOSUPPORT)
inscode(d, ds, de, "EPFNOSUPPORT", WSAEPFNOSUPPORT, "Protocol family not supported");
#endif
#endif
#if defined(ENOPROTOOPT)
inscode(d, ds, de, "ENOPROTOOPT", ENOPROTOOPT, "Protocol not available");
#else
#if defined(WSAENOPROTOOPT)
inscode(d, ds, de, "ENOPROTOOPT", WSAENOPROTOOPT, "Protocol not available");
#endif
#endif
#if defined(EBUSY)
inscode(d, ds, de, "EBUSY", EBUSY, "Device or resource busy");
#endif
#if defined(EWOULDBLOCK)
inscode(d, ds, de, "EWOULDBLOCK", EWOULDBLOCK, "Operation would block");
#else
#if defined(WSAEWOULDBLOCK)
inscode(d, ds, de, "EWOULDBLOCK", WSAEWOULDBLOCK, "Operation would block");
#endif
#endif
#if defined(EBADFD)
inscode(d, ds, de, "EBADFD", EBADFD, "File descriptor in bad state");
#endif
#if defined(EDOTDOT)
inscode(d, ds, de, "EDOTDOT", EDOTDOT, "RFS specific error");
#endif
#if defined(EISCONN)
inscode(d, ds, de, "EISCONN", EISCONN, "Transport endpoint is already connected");
#else
#if defined(WSAEISCONN)
inscode(d, ds, de, "EISCONN", WSAEISCONN, "Transport endpoint is already connected");
#endif
#endif
#if defined(ENOANO)
inscode(d, ds, de, "ENOANO", ENOANO, "No anode");
#endif
#if defined(ESHUTDOWN)
inscode(d, ds, de, "ESHUTDOWN", ESHUTDOWN, "Cannot send after transport endpoint shutdown");
#else
#if defined(WSAESHUTDOWN)
inscode(d, ds, de, "ESHUTDOWN", WSAESHUTDOWN, "Cannot send after transport endpoint shutdown");
#endif
#endif
#if defined(ECHRNG)
inscode(d, ds, de, "ECHRNG", ECHRNG, "Channel number out of range");
#endif
#if defined(ELIBBAD)
inscode(d, ds, de, "ELIBBAD", ELIBBAD, "Accessing a corrupted shared library");
#endif
#if defined(ENONET)
inscode(d, ds, de, "ENONET", ENONET, "Machine is not on the network");
#endif
#if defined(EBADE)
inscode(d, ds, de, "EBADE", EBADE, "Invalid exchange");
#endif
#if defined(EBADF)
inscode(d, ds, de, "EBADF", EBADF, "Bad file number");
#else
#if defined(WSAEBADF)
inscode(d, ds, de, "EBADF", WSAEBADF, "Bad file number");
#endif
#endif
#if defined(EMULTIHOP)
inscode(d, ds, de, "EMULTIHOP", EMULTIHOP, "Multihop attempted");
#endif
#if defined(EIO)
inscode(d, ds, de, "EIO", EIO, "I/O error");
#endif
#if defined(EUNATCH)
inscode(d, ds, de, "EUNATCH", EUNATCH, "Protocol driver not attached");
#endif
#if defined(EPROTOTYPE)
inscode(d, ds, de, "EPROTOTYPE", EPROTOTYPE, "Protocol wrong type for socket");
#else
#if defined(WSAEPROTOTYPE)
inscode(d, ds, de, "EPROTOTYPE", WSAEPROTOTYPE, "Protocol wrong type for socket");
#endif
#endif
#if defined(ENOSPC)
inscode(d, ds, de, "ENOSPC", ENOSPC, "No space left on device");
#endif
#if defined(ENOEXEC)
inscode(d, ds, de, "ENOEXEC", ENOEXEC, "Exec format error");
#endif
#if defined(EALREADY)
inscode(d, ds, de, "EALREADY", EALREADY, "Operation already in progress");
#else
#if defined(WSAEALREADY)
inscode(d, ds, de, "EALREADY", WSAEALREADY, "Operation already in progress");
#endif
#endif
#if defined(ENETDOWN)
inscode(d, ds, de, "ENETDOWN", ENETDOWN, "Network is down");
#else
#if defined(WSAENETDOWN)
inscode(d, ds, de, "ENETDOWN", WSAENETDOWN, "Network is down");
#endif
#endif
#if defined(ENOTNAM)
inscode(d, ds, de, "ENOTNAM", ENOTNAM, "Not a XENIX named type file");
#endif
#if defined(EACCES)
inscode(d, ds, de, "EACCES", EACCES, "Permission denied");
#else
#if defined(WSAEACCES)
inscode(d, ds, de, "EACCES", WSAEACCES, "Permission denied");
#endif
#endif
#if defined(ELNRNG)
inscode(d, ds, de, "ELNRNG", ELNRNG, "Link number out of range");
#endif
#if defined(EILSEQ)
inscode(d, ds, de, "EILSEQ", EILSEQ, "Illegal byte sequence");
#endif
#if defined(ENOTDIR)
inscode(d, ds, de, "ENOTDIR", ENOTDIR, "Not a directory");
#endif
#if defined(ENOTUNIQ)
inscode(d, ds, de, "ENOTUNIQ", ENOTUNIQ, "Name not unique on network");
#endif
#if defined(EPERM)
inscode(d, ds, de, "EPERM", EPERM, "Operation not permitted");
#endif
#if defined(EDOM)
inscode(d, ds, de, "EDOM", EDOM, "Math argument out of domain of func");
#endif
#if defined(EXFULL)
inscode(d, ds, de, "EXFULL", EXFULL, "Exchange full");
#endif
#if defined(ECONNREFUSED)
inscode(d, ds, de, "ECONNREFUSED", ECONNREFUSED, "Connection refused");
#else
#if defined(WSAECONNREFUSED)
inscode(d, ds, de, "ECONNREFUSED", WSAECONNREFUSED, "Connection refused");
#endif
#endif
#if defined(EISDIR)
inscode(d, ds, de, "EISDIR", EISDIR, "Is a directory");
#endif
#if defined(EPROTONOSUPPORT)
inscode(d, ds, de, "EPROTONOSUPPORT", EPROTONOSUPPORT, "Protocol not supported");
#else
#if defined(WSAEPROTONOSUPPORT)
inscode(d, ds, de, "EPROTONOSUPPORT", WSAEPROTONOSUPPORT, "Protocol not supported");
#endif
#endif
#if defined(EROFS)
inscode(d, ds, de, "EROFS", EROFS, "Read-only file system");
#endif
#if defined(EADDRNOTAVAIL)
inscode(d, ds, de, "EADDRNOTAVAIL", EADDRNOTAVAIL, "Cannot assign requested address");
#else
#if defined(WSAEADDRNOTAVAIL)
inscode(d, ds, de, "EADDRNOTAVAIL", WSAEADDRNOTAVAIL, "Cannot assign requested address");
#endif
#endif
#if defined(EIDRM)
inscode(d, ds, de, "EIDRM", EIDRM, "Identifier removed");
#endif
#if defined(ECOMM)
inscode(d, ds, de, "ECOMM", ECOMM, "Communication error on send");
#endif
#if defined(ESRMNT)
inscode(d, ds, de, "ESRMNT", ESRMNT, "Srmount error");
#endif
#if defined(EREMOTEIO)
inscode(d, ds, de, "EREMOTEIO", EREMOTEIO, "Remote I/O error");
#endif
#if defined(EL3RST)
inscode(d, ds, de, "EL3RST", EL3RST, "Level 3 reset");
#endif
#if defined(EBADMSG)
inscode(d, ds, de, "EBADMSG", EBADMSG, "Not a data message");
#endif
#if defined(ENFILE)
inscode(d, ds, de, "ENFILE", ENFILE, "File table overflow");
#endif
#if defined(ELIBMAX)
inscode(d, ds, de, "ELIBMAX", ELIBMAX, "Attempting to link in too many shared libraries");
#endif
#if defined(ESPIPE)
inscode(d, ds, de, "ESPIPE", ESPIPE, "Illegal seek");
#endif
#if defined(ENOLINK)
inscode(d, ds, de, "ENOLINK", ENOLINK, "Link has been severed");
#endif
#if defined(ENETRESET)
inscode(d, ds, de, "ENETRESET", ENETRESET, "Network dropped connection because of reset");
#else
#if defined(WSAENETRESET)
inscode(d, ds, de, "ENETRESET", WSAENETRESET, "Network dropped connection because of reset");
#endif
#endif
#if defined(ETIMEDOUT)
inscode(d, ds, de, "ETIMEDOUT", ETIMEDOUT, "Connection timed out");
#else
#if defined(WSAETIMEDOUT)
inscode(d, ds, de, "ETIMEDOUT", WSAETIMEDOUT, "Connection timed out");
#endif
#endif
#if defined(ENOENT)
inscode(d, ds, de, "ENOENT", ENOENT, "No such file or directory");
#endif
#if defined(EEXIST)
inscode(d, ds, de, "EEXIST", EEXIST, "File exists");
#endif
#if defined(EDQUOT)
inscode(d, ds, de, "EDQUOT", EDQUOT, "Quota exceeded");
#else
#if defined(WSAEDQUOT)
inscode(d, ds, de, "EDQUOT", WSAEDQUOT, "Quota exceeded");
#endif
#endif
#if defined(ENOSTR)
inscode(d, ds, de, "ENOSTR", ENOSTR, "Device not a stream");
#endif
#if defined(EBADSLT)
inscode(d, ds, de, "EBADSLT", EBADSLT, "Invalid slot");
#endif
#if defined(EBADRQC)
inscode(d, ds, de, "EBADRQC", EBADRQC, "Invalid request code");
#endif
#if defined(ELIBACC)
inscode(d, ds, de, "ELIBACC", ELIBACC, "Can not access a needed shared library");
#endif
#if defined(EFAULT)
inscode(d, ds, de, "EFAULT", EFAULT, "Bad address");
#else
#if defined(WSAEFAULT)
inscode(d, ds, de, "EFAULT", WSAEFAULT, "Bad address");
#endif
#endif
#if defined(EFBIG)
inscode(d, ds, de, "EFBIG", EFBIG, "File too large");
#endif
#if defined(EDEADLK)
inscode(d, ds, de, "EDEADLK", EDEADLK, "Resource deadlock would occur");
#endif
#if defined(ENOTCONN)
inscode(d, ds, de, "ENOTCONN", ENOTCONN, "Transport endpoint is not connected");
#else
#if defined(WSAENOTCONN)
inscode(d, ds, de, "ENOTCONN", WSAENOTCONN, "Transport endpoint is not connected");
#endif
#endif
#if defined(EDESTADDRREQ)
inscode(d, ds, de, "EDESTADDRREQ", EDESTADDRREQ, "Destination address required");
#else
#if defined(WSAEDESTADDRREQ)
inscode(d, ds, de, "EDESTADDRREQ", WSAEDESTADDRREQ, "Destination address required");
#endif
#endif
#if defined(ELIBSCN)
inscode(d, ds, de, "ELIBSCN", ELIBSCN, ".lib section in a.out corrupted");
#endif
#if defined(ENOLCK)
inscode(d, ds, de, "ENOLCK", ENOLCK, "No record locks available");
#endif
#if defined(EISNAM)
inscode(d, ds, de, "EISNAM", EISNAM, "Is a named type file");
#endif
#if defined(ECONNABORTED)
inscode(d, ds, de, "ECONNABORTED", ECONNABORTED, "Software caused connection abort");
#else
#if defined(WSAECONNABORTED)
inscode(d, ds, de, "ECONNABORTED", WSAECONNABORTED, "Software caused connection abort");
#endif
#endif
#if defined(ENETUNREACH)
inscode(d, ds, de, "ENETUNREACH", ENETUNREACH, "Network is unreachable");
#else
#if defined(WSAENETUNREACH)
inscode(d, ds, de, "ENETUNREACH", WSAENETUNREACH, "Network is unreachable");
#endif
#endif
#if defined(ESTALE)
inscode(d, ds, de, "ESTALE", ESTALE, "Stale NFS file handle");
#else
#if defined(WSAESTALE)
inscode(d, ds, de, "ESTALE", WSAESTALE, "Stale NFS file handle");
#endif
#endif
#if defined(ENOSR)
inscode(d, ds, de, "ENOSR", ENOSR, "Out of streams resources");
#endif
#if defined(ENOMEM)
inscode(d, ds, de, "ENOMEM", ENOMEM, "Out of memory");
#endif
#if defined(ENOTSOCK)
inscode(d, ds, de, "ENOTSOCK", ENOTSOCK, "Socket operation on non-socket");
#else
#if defined(WSAENOTSOCK)
inscode(d, ds, de, "ENOTSOCK", WSAENOTSOCK, "Socket operation on non-socket");
#endif
#endif
#if defined(ESTRPIPE)
inscode(d, ds, de, "ESTRPIPE", ESTRPIPE, "Streams pipe error");
#endif
#if defined(EMLINK)
inscode(d, ds, de, "EMLINK", EMLINK, "Too many links");
#endif
#if defined(ERANGE)
inscode(d, ds, de, "ERANGE", ERANGE, "Math result not representable");
#endif
#if defined(ELIBEXEC)
inscode(d, ds, de, "ELIBEXEC", ELIBEXEC, "Cannot exec a shared library directly");
#endif
#if defined(EL3HLT)
inscode(d, ds, de, "EL3HLT", EL3HLT, "Level 3 halted");
#endif
#if defined(ECONNRESET)
inscode(d, ds, de, "ECONNRESET", ECONNRESET, "Connection reset by peer");
#else
#if defined(WSAECONNRESET)
inscode(d, ds, de, "ECONNRESET", WSAECONNRESET, "Connection reset by peer");
#endif
#endif
#if defined(EADDRINUSE)
inscode(d, ds, de, "EADDRINUSE", EADDRINUSE, "Address already in use");
#else
#if defined(WSAEADDRINUSE)
inscode(d, ds, de, "EADDRINUSE", WSAEADDRINUSE, "Address already in use");
#endif
#endif
#if defined(EOPNOTSUPP)
inscode(d, ds, de, "EOPNOTSUPP", EOPNOTSUPP, "Operation not supported on transport endpoint");
#else
#if defined(WSAEOPNOTSUPP)
inscode(d, ds, de, "EOPNOTSUPP", WSAEOPNOTSUPP, "Operation not supported on transport endpoint");
#endif
#endif
#if defined(EREMCHG)
inscode(d, ds, de, "EREMCHG", EREMCHG, "Remote address changed");
#endif
#if defined(EAGAIN)
inscode(d, ds, de, "EAGAIN", EAGAIN, "Try again");
#endif
#if defined(ENAMETOOLONG)
inscode(d, ds, de, "ENAMETOOLONG", ENAMETOOLONG, "File name too long");
#else
#if defined(WSAENAMETOOLONG)
inscode(d, ds, de, "ENAMETOOLONG", WSAENAMETOOLONG, "File name too long");
#endif
#endif
#if defined(ENOTTY)
inscode(d, ds, de, "ENOTTY", ENOTTY, "Not a typewriter");
#endif
#if defined(ERESTART)
inscode(d, ds, de, "ERESTART", ERESTART, "Interrupted system call should be restarted");
#endif
#if defined(ESOCKTNOSUPPORT)
inscode(d, ds, de, "ESOCKTNOSUPPORT", ESOCKTNOSUPPORT, "Socket type not supported");
#else
#if defined(WSAESOCKTNOSUPPORT)
inscode(d, ds, de, "ESOCKTNOSUPPORT", WSAESOCKTNOSUPPORT, "Socket type not supported");
#endif
#endif
#if defined(ETIME)
inscode(d, ds, de, "ETIME", ETIME, "Timer expired");
#endif
#if defined(EBFONT)
inscode(d, ds, de, "EBFONT", EBFONT, "Bad font file format");
#endif
#if defined(EDEADLOCK)
inscode(d, ds, de, "EDEADLOCK", EDEADLOCK, "Error EDEADLOCK");
#endif
#if defined(ETOOMANYREFS)
inscode(d, ds, de, "ETOOMANYREFS", ETOOMANYREFS, "Too many references: cannot splice");
#else
#if defined(WSAETOOMANYREFS)
inscode(d, ds, de, "ETOOMANYREFS", WSAETOOMANYREFS, "Too many references: cannot splice");
#endif
#endif
#if defined(EMFILE)
inscode(d, ds, de, "EMFILE", EMFILE, "Too many open files");
#else
#if defined(WSAEMFILE)
inscode(d, ds, de, "EMFILE", WSAEMFILE, "Too many open files");
#endif
#endif
#if defined(ETXTBSY)
inscode(d, ds, de, "ETXTBSY", ETXTBSY, "Text file busy");
#endif
#if defined(EINPROGRESS)
inscode(d, ds, de, "EINPROGRESS", EINPROGRESS, "Operation now in progress");
#else
#if defined(WSAEINPROGRESS)
inscode(d, ds, de, "EINPROGRESS", WSAEINPROGRESS, "Operation now in progress");
#endif
#endif
#if defined(ENXIO)
inscode(d, ds, de, "ENXIO", ENXIO, "No such device or address");
#endif
#if defined(ENOPKG)
inscode(d, ds, de, "ENOPKG", ENOPKG, "Package not installed");
#endif
#if defined(WSASY)
inscode(d, ds, de, "WSASY", WSASY, "Error WSASY");
#endif
#if defined(WSAEHOSTDOWN)
inscode(d, ds, de, "WSAEHOSTDOWN", WSAEHOSTDOWN, "Host is down");
#endif
#if defined(WSAENETDOWN)
inscode(d, ds, de, "WSAENETDOWN", WSAENETDOWN, "Network is down");
#endif
#if defined(WSAENOTSOCK)
inscode(d, ds, de, "WSAENOTSOCK", WSAENOTSOCK, "Socket operation on non-socket");
#endif
#if defined(WSAEHOSTUNREACH)
inscode(d, ds, de, "WSAEHOSTUNREACH", WSAEHOSTUNREACH, "No route to host");
#endif
#if defined(WSAELOOP)
inscode(d, ds, de, "WSAELOOP", WSAELOOP, "Too many symbolic links encountered");
#endif
#if defined(WSAEMFILE)
inscode(d, ds, de, "WSAEMFILE", WSAEMFILE, "Too many open files");
#endif
#if defined(WSAESTALE)
inscode(d, ds, de, "WSAESTALE", WSAESTALE, "Stale NFS file handle");
#endif
#if defined(WSAVERNOTSUPPORTED)
inscode(d, ds, de, "WSAVERNOTSUPPORTED", WSAVERNOTSUPPORTED, "Error WSAVERNOTSUPPORTED");
#endif
#if defined(WSAENETUNREACH)
inscode(d, ds, de, "WSAENETUNREACH", WSAENETUNREACH, "Network is unreachable");
#endif
#if defined(WSAEPROCLIM)
inscode(d, ds, de, "WSAEPROCLIM", WSAEPROCLIM, "Error WSAEPROCLIM");
#endif
#if defined(WSAEFAULT)
inscode(d, ds, de, "WSAEFAULT", WSAEFAULT, "Bad address");
#endif
#if defined(WSANOTINITIALISED)
inscode(d, ds, de, "WSANOTINITIALISED", WSANOTINITIALISED, "Error WSANOTINITIALISED");
#endif
#if defined(WSAEUSERS)
inscode(d, ds, de, "WSAEUSERS", WSAEUSERS, "Too many users");
#endif
#if defined(WSAMAKEASYNCREPL)
inscode(d, ds, de, "WSAMAKEASYNCREPL", WSAMAKEASYNCREPL, "Error WSAMAKEASYNCREPL");
#endif
#if defined(WSAENOPROTOOPT)
inscode(d, ds, de, "WSAENOPROTOOPT", WSAENOPROTOOPT, "Protocol not available");
#endif
#if defined(WSAECONNABORTED)
inscode(d, ds, de, "WSAECONNABORTED", WSAECONNABORTED, "Software caused connection abort");
#endif
#if defined(WSAENAMETOOLONG)
inscode(d, ds, de, "WSAENAMETOOLONG", WSAENAMETOOLONG, "File name too long");
#endif
#if defined(WSAENOTEMPTY)
inscode(d, ds, de, "WSAENOTEMPTY", WSAENOTEMPTY, "Directory not empty");
#endif
#if defined(WSAESHUTDOWN)
inscode(d, ds, de, "WSAESHUTDOWN", WSAESHUTDOWN, "Cannot send after transport endpoint shutdown");
#endif
#if defined(WSAEAFNOSUPPORT)
inscode(d, ds, de, "WSAEAFNOSUPPORT", WSAEAFNOSUPPORT, "Address family not supported by protocol");
#endif
#if defined(WSAETOOMANYREFS)
inscode(d, ds, de, "WSAETOOMANYREFS", WSAETOOMANYREFS, "Too many references: cannot splice");
#endif
#if defined(WSAEACCES)
inscode(d, ds, de, "WSAEACCES", WSAEACCES, "Permission denied");
#endif
#if defined(WSATR)
inscode(d, ds, de, "WSATR", WSATR, "Error WSATR");
#endif
#if defined(WSABASEERR)
inscode(d, ds, de, "WSABASEERR", WSABASEERR, "Error WSABASEERR");
#endif
#if defined(WSADESCRIPTIO)
inscode(d, ds, de, "WSADESCRIPTIO", WSADESCRIPTIO, "Error WSADESCRIPTIO");
#endif
#if defined(WSAEMSGSIZE)
inscode(d, ds, de, "WSAEMSGSIZE", WSAEMSGSIZE, "Message too long");
#endif
#if defined(WSAEBADF)
inscode(d, ds, de, "WSAEBADF", WSAEBADF, "Bad file number");
#endif
#if defined(WSAECONNRESET)
inscode(d, ds, de, "WSAECONNRESET", WSAECONNRESET, "Connection reset by peer");
#endif
#if defined(WSAGETSELECTERRO)
inscode(d, ds, de, "WSAGETSELECTERRO", WSAGETSELECTERRO, "Error WSAGETSELECTERRO");
#endif
#if defined(WSAETIMEDOUT)
inscode(d, ds, de, "WSAETIMEDOUT", WSAETIMEDOUT, "Connection timed out");
#endif
#if defined(WSAENOBUFS)
inscode(d, ds, de, "WSAENOBUFS", WSAENOBUFS, "No buffer space available");
#endif
#if defined(WSAEDISCON)
inscode(d, ds, de, "WSAEDISCON", WSAEDISCON, "Error WSAEDISCON");
#endif
#if defined(WSAEINTR)
inscode(d, ds, de, "WSAEINTR", WSAEINTR, "Interrupted system call");
#endif
#if defined(WSAEPROTOTYPE)
inscode(d, ds, de, "WSAEPROTOTYPE", WSAEPROTOTYPE, "Protocol wrong type for socket");
#endif
#if defined(WSAHOS)
inscode(d, ds, de, "WSAHOS", WSAHOS, "Error WSAHOS");
#endif
#if defined(WSAEADDRINUSE)
inscode(d, ds, de, "WSAEADDRINUSE", WSAEADDRINUSE, "Address already in use");
#endif
#if defined(WSAEADDRNOTAVAIL)
inscode(d, ds, de, "WSAEADDRNOTAVAIL", WSAEADDRNOTAVAIL, "Cannot assign requested address");
#endif
#if defined(WSAEALREADY)
inscode(d, ds, de, "WSAEALREADY", WSAEALREADY, "Operation already in progress");
#endif
#if defined(WSAEPROTONOSUPPORT)
inscode(d, ds, de, "WSAEPROTONOSUPPORT", WSAEPROTONOSUPPORT, "Protocol not supported");
#endif
#if defined(WSASYSNOTREADY)
inscode(d, ds, de, "WSASYSNOTREADY", WSASYSNOTREADY, "Error WSASYSNOTREADY");
#endif
#if defined(WSAEWOULDBLOCK)
inscode(d, ds, de, "WSAEWOULDBLOCK", WSAEWOULDBLOCK, "Operation would block");
#endif
#if defined(WSAEPFNOSUPPORT)
inscode(d, ds, de, "WSAEPFNOSUPPORT", WSAEPFNOSUPPORT, "Protocol family not supported");
#endif
#if defined(WSAEOPNOTSUPP)
inscode(d, ds, de, "WSAEOPNOTSUPP", WSAEOPNOTSUPP, "Operation not supported on transport endpoint");
#endif
#if defined(WSAEISCONN)
inscode(d, ds, de, "WSAEISCONN", WSAEISCONN, "Transport endpoint is already connected");
#endif
#if defined(WSAEDQUOT)
inscode(d, ds, de, "WSAEDQUOT", WSAEDQUOT, "Quota exceeded");
#endif
#if defined(WSAENOTCONN)
inscode(d, ds, de, "WSAENOTCONN", WSAENOTCONN, "Transport endpoint is not connected");
#endif
#if defined(WSAEREMOTE)
inscode(d, ds, de, "WSAEREMOTE", WSAEREMOTE, "Object is remote");
#endif
#if defined(WSAEINVAL)
inscode(d, ds, de, "WSAEINVAL", WSAEINVAL, "Invalid argument");
#endif
#if defined(WSAEINPROGRESS)
inscode(d, ds, de, "WSAEINPROGRESS", WSAEINPROGRESS, "Operation now in progress");
#endif
#if defined(WSAGETSELECTEVEN)
inscode(d, ds, de, "WSAGETSELECTEVEN", WSAGETSELECTEVEN, "Error WSAGETSELECTEVEN");
#endif
#if defined(WSAESOCKTNOSUPPORT)
inscode(d, ds, de, "WSAESOCKTNOSUPPORT", WSAESOCKTNOSUPPORT, "Socket type not supported");
#endif
#if defined(WSAGETASYNCERRO)
inscode(d, ds, de, "WSAGETASYNCERRO", WSAGETASYNCERRO, "Error WSAGETASYNCERRO");
#endif
#if defined(WSAMAKESELECTREPL)
inscode(d, ds, de, "WSAMAKESELECTREPL", WSAMAKESELECTREPL, "Error WSAMAKESELECTREPL");
#endif
#if defined(WSAGETASYNCBUFLE)
inscode(d, ds, de, "WSAGETASYNCBUFLE", WSAGETASYNCBUFLE, "Error WSAGETASYNCBUFLE");
#endif
#if defined(WSAEDESTADDRREQ)
inscode(d, ds, de, "WSAEDESTADDRREQ", WSAEDESTADDRREQ, "Destination address required");
#endif
#if defined(WSAECONNREFUSED)
inscode(d, ds, de, "WSAECONNREFUSED", WSAECONNREFUSED, "Connection refused");
#endif
#if defined(WSAENETRESET)
inscode(d, ds, de, "WSAENETRESET", WSAENETRESET, "Network dropped connection because of reset");
#endif
#if defined(WSAN)
inscode(d, ds, de, "WSAN", WSAN, "Error WSAN");
#endif
Py_DECREF(de);
}