#if defined(__APPLE__)
#pragma weak inet_aton
#endif
#include "Python.h"
#include "structmember.h"
#undef MAX
#define MAX(x, y) ((x) < (y) ? (y) : (x))
PyDoc_STRVAR(sock_doc,
"socket([family[, type[, proto]]]) -> socket object\n\
\n\
Open a socket of the given type. The family argument specifies the\n\
address family; it defaults to AF_INET. The type argument specifies\n\
whether this is a stream (SOCK_STREAM, this is the default)\n\
or datagram (SOCK_DGRAM) socket. The protocol argument defaults to 0,\n\
specifying the default protocol. Keyword arguments are accepted.\n\
\n\
A socket object represents one endpoint of a network connection.\n\
\n\
Methods of socket objects (keyword arguments not allowed):\n\
\n\
accept() -- accept a connection, returning new socket and client address\n\
bind(addr) -- bind the socket to a local address\n\
close() -- close the socket\n\
connect(addr) -- connect the socket to a remote address\n\
connect_ex(addr) -- connect, return an error code instead of an exception\n\
dup() -- return a new socket object identical to the current one [*]\n\
fileno() -- return underlying file descriptor\n\
getpeername() -- return remote address [*]\n\
getsockname() -- return local address\n\
getsockopt(level, optname[, buflen]) -- get socket options\n\
gettimeout() -- return timeout or None\n\
listen(n) -- start listening for incoming connections\n\
makefile([mode, [bufsize]]) -- return a file object for the socket [*]\n\
recv(buflen[, flags]) -- receive data\n\
recv_into(buffer[, nbytes[, flags]]) -- receive data (into a buffer)\n\
recvfrom(buflen[, flags]) -- receive data and sender\'s address\n\
recvfrom_into(buffer[, nbytes, [, flags])\n\
-- receive data and sender\'s address (into a buffer)\n\
sendall(data[, flags]) -- send all data\n\
send(data[, flags]) -- send data, may not send all of it\n\
sendto(data[, flags], addr) -- send data to a given address\n\
setblocking(0 | 1) -- set or clear the blocking I/O flag\n\
setsockopt(level, optname, value) -- set socket options\n\
settimeout(None | float) -- set or clear the timeout\n\
shutdown(how) -- shut down traffic in one or both directions\n\
\n\
[*] not available on all platforms!");
#if !defined(linux)
#undef HAVE_GETHOSTBYNAME_R_3_ARG
#undef HAVE_GETHOSTBYNAME_R_5_ARG
#undef HAVE_GETHOSTBYNAME_R_6_ARG
#endif
#if !defined(WITH_THREAD)
#undef HAVE_GETHOSTBYNAME_R
#endif
#if defined(HAVE_GETHOSTBYNAME_R)
#if defined(_AIX) || defined(__osf__)
#define HAVE_GETHOSTBYNAME_R_3_ARG
#elif defined(__sun) || defined(__sgi)
#define HAVE_GETHOSTBYNAME_R_5_ARG
#elif defined(linux)
#else
#undef HAVE_GETHOSTBYNAME_R
#endif
#endif
#if !defined(HAVE_GETHOSTBYNAME_R) && defined(WITH_THREAD) && !defined(MS_WINDOWS)
#define USE_GETHOSTBYNAME_LOCK
#endif
#if defined(HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif
#if defined(WITH_THREAD) && (defined(__APPLE__) || (defined(__FreeBSD__) && __FreeBSD_version+0 < 503000) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__VMS) || !defined(HAVE_GETADDRINFO))
#define USE_GETADDRINFO_LOCK
#endif
#if defined(USE_GETADDRINFO_LOCK)
#define ACQUIRE_GETADDRINFO_LOCK PyThread_acquire_lock(netdb_lock, 1);
#define RELEASE_GETADDRINFO_LOCK PyThread_release_lock(netdb_lock);
#else
#define ACQUIRE_GETADDRINFO_LOCK
#define RELEASE_GETADDRINFO_LOCK
#endif
#if defined(USE_GETHOSTBYNAME_LOCK) || defined(USE_GETADDRINFO_LOCK)
#include "pythread.h"
#endif
#if defined(PYCC_VACPP)
#include <types.h>
#include <io.h>
#include <sys/ioctl.h>
#include <utils.h>
#include <ctype.h>
#endif
#if defined(__VMS)
#include <ioctl.h>
#endif
#if defined(PYOS_OS2)
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>
#endif
#if defined(__sgi) && _COMPILER_VERSION>700 && !_SGIAPI
#undef _SGIAPI
#define _SGIAPI 1
#undef _XOPEN_SOURCE
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#if defined(_SS_ALIGNSIZE)
#define HAVE_GETADDRINFO 1
#define HAVE_GETNAMEINFO 1
#endif
#define HAVE_INET_PTON
#include <netdb.h>
#endif
#if (defined(__sgi) || defined(sun)) && !defined(INET_ADDRSTRLEN)
#define INET_ADDRSTRLEN 16
#endif
#if defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif
#define PySocket_BUILDING_SOCKET
#include "socketmodule.h"
#if !defined(MS_WINDOWS)
#include <netdb.h>
#if defined(__BEOS__)
#include <net/netdb.h>
#elif defined(PYOS_OS2) && defined(PYCC_VACPP)
#include <netdb.h>
typedef size_t socklen_t;
#else
#include <arpa/inet.h>
#endif
#if !defined(RISCOS)
#include <fcntl.h>
#else
#include <sys/ioctl.h>
#include <socklib.h>
#define NO_DUP
int h_errno;
#define INET_ADDRSTRLEN 16
#endif
#else
#if defined(HAVE_FCNTL_H)
#include <fcntl.h>
#endif
#endif
#include <stddef.h>
#if !defined(offsetof)
#define offsetof(type, member) ((size_t)(&((type *)0)->member))
#endif
#if !defined(O_NONBLOCK)
#define O_NONBLOCK O_NDELAY
#endif
#if defined(__sgi) && _COMPILER_VERSION>700 && defined(_SS_ALIGNSIZE)
#elif defined(_MSC_VER) && _MSC_VER>1201
#else
#include "addrinfo.h"
#endif
#if !defined(HAVE_INET_PTON)
#if !defined(NTDDI_VERSION) || (NTDDI_VERSION < NTDDI_LONGHORN)
int inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
#endif
#endif
#if defined(__APPLE__)
#if !defined(HAVE_GETNAMEINFO)
#undef HAVE_GETADDRINFO
#endif
#if defined(HAVE_INET_ATON)
#define USE_INET_ATON_WEAKLINK
#endif
#endif
#if !defined(HAVE_GETADDRINFO)
#define getaddrinfo fake_getaddrinfo
#define gai_strerror fake_gai_strerror
#define freeaddrinfo fake_freeaddrinfo
#include "getaddrinfo.c"
#endif
#if !defined(HAVE_GETNAMEINFO)
#define getnameinfo fake_getnameinfo
#include "getnameinfo.c"
#endif
#if defined(MS_WINDOWS) || defined(__BEOS__)
#define SOCKETCLOSE closesocket
#define NO_DUP
#endif
#if defined(MS_WIN32)
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#define snprintf _snprintf
#endif
#if defined(PYOS_OS2) && !defined(PYCC_GCC)
#define SOCKETCLOSE soclose
#define NO_DUP
#endif
#if !defined(SOCKETCLOSE)
#define SOCKETCLOSE close
#endif
#if defined(HAVE_BLUETOOTH_H) || defined(HAVE_BLUETOOTH_BLUETOOTH_H)
#define USE_BLUETOOTH 1
#if defined(__FreeBSD__)
#define BTPROTO_L2CAP BLUETOOTH_PROTO_L2CAP
#define BTPROTO_RFCOMM BLUETOOTH_PROTO_RFCOMM
#define BTPROTO_HCI BLUETOOTH_PROTO_HCI
#define SOL_HCI SOL_HCI_RAW
#define HCI_FILTER SO_HCI_RAW_FILTER
#define sockaddr_l2 sockaddr_l2cap
#define sockaddr_rc sockaddr_rfcomm
#define hci_dev hci_node
#define _BT_L2_MEMB(sa, memb) ((sa)->l2cap_##memb)
#define _BT_RC_MEMB(sa, memb) ((sa)->rfcomm_##memb)
#define _BT_HCI_MEMB(sa, memb) ((sa)->hci_##memb)
#elif defined(__NetBSD__)
#define sockaddr_l2 sockaddr_bt
#define sockaddr_rc sockaddr_bt
#define sockaddr_hci sockaddr_bt
#define sockaddr_sco sockaddr_bt
#define _BT_L2_MEMB(sa, memb) ((sa)->bt_##memb)
#define _BT_RC_MEMB(sa, memb) ((sa)->bt_##memb)
#define _BT_HCI_MEMB(sa, memb) ((sa)->bt_##memb)
#define _BT_SCO_MEMB(sa, memb) ((sa)->bt_##memb)
#else
#define _BT_L2_MEMB(sa, memb) ((sa)->l2_##memb)
#define _BT_RC_MEMB(sa, memb) ((sa)->rc_##memb)
#define _BT_HCI_MEMB(sa, memb) ((sa)->hci_##memb)
#define _BT_SCO_MEMB(sa, memb) ((sa)->sco_##memb)
#endif
#endif
#if defined(__VMS)
#define SEGMENT_SIZE (32 * 1024 -1)
#endif
#define SAS2SA(x) ((struct sockaddr *)(x))
#if !defined(NI_MAXHOST)
#define NI_MAXHOST 1025
#endif
#if !defined(NI_MAXSERV)
#define NI_MAXSERV 32
#endif
static PyObject *socket_error;
static PyObject *socket_herror;
static PyObject *socket_gaierror;
static PyObject *socket_timeout;
#if defined(RISCOS)
static int taskwindow;
#endif
static PyTypeObject sock_type;
#if defined(HAVE_POLL_H)
#include <poll.h>
#elif defined(HAVE_SYS_POLL_H)
#include <sys/poll.h>
#endif
#if defined(Py_SOCKET_FD_CAN_BE_GE_FD_SETSIZE)
#define IS_SELECTABLE(s) 1
#elif defined(HAVE_POLL)
#define IS_SELECTABLE(s) 1
#else
#define IS_SELECTABLE(s) ((s)->sock_fd < FD_SETSIZE || s->sock_timeout <= 0.0)
#endif
static PyObject*
select_error(void) {
PyErr_SetString(socket_error, "unable to select on socket");
return NULL;
}
static PyObject *
set_error(void) {
#if defined(MS_WINDOWS)
int err_no = WSAGetLastError();
if (err_no)
return PyErr_SetExcFromWindowsErr(socket_error, err_no);
#endif
#if defined(PYOS_OS2) && !defined(PYCC_GCC)
if (sock_errno() != NO_ERROR) {
APIRET rc;
ULONG msglen;
char outbuf[100];
int myerrorcode = sock_errno();
rc = DosGetMessage(NULL, 0, outbuf, sizeof(outbuf),
myerrorcode - SOCBASEERR + 26,
"mptn.msg",
&msglen);
if (rc == NO_ERROR) {
PyObject *v;
outbuf[msglen] = '\0';
if (strlen(outbuf) > 0) {
char *lastc = &outbuf[ strlen(outbuf)-1 ];
while (lastc > outbuf &&
isspace(Py_CHARMASK(*lastc))) {
*lastc-- = '\0';
}
}
v = Py_BuildValue("(is)", myerrorcode, outbuf);
if (v != NULL) {
PyErr_SetObject(socket_error, v);
Py_DECREF(v);
}
return NULL;
}
}
#endif
#if defined(RISCOS)
if (_inet_error.errnum != NULL) {
PyObject *v;
v = Py_BuildValue("(is)", errno, _inet_err());
if (v != NULL) {
PyErr_SetObject(socket_error, v);
Py_DECREF(v);
}
return NULL;
}
#endif
return PyErr_SetFromErrno(socket_error);
}
static PyObject *
set_herror(int h_error) {
PyObject *v;
#if defined(HAVE_HSTRERROR)
v = Py_BuildValue("(is)", h_error, (char *)hstrerror(h_error));
#else
v = Py_BuildValue("(is)", h_error, "host not found");
#endif
if (v != NULL) {
PyErr_SetObject(socket_herror, v);
Py_DECREF(v);
}
return NULL;
}
static PyObject *
set_gaierror(int error) {
PyObject *v;
#if defined(EAI_SYSTEM)
if (error == EAI_SYSTEM)
return set_error();
#endif
#if defined(HAVE_GAI_STRERROR)
v = Py_BuildValue("(is)", error, gai_strerror(error));
#else
v = Py_BuildValue("(is)", error, "getaddrinfo failed");
#endif
if (v != NULL) {
PyErr_SetObject(socket_gaierror, v);
Py_DECREF(v);
}
return NULL;
}
#if defined(__VMS)
static int
sendsegmented(int sock_fd, char *buf, int len, int flags) {
int n = 0;
int remaining = len;
while (remaining > 0) {
unsigned int segment;
segment = (remaining >= SEGMENT_SIZE ? SEGMENT_SIZE : remaining);
n = send(sock_fd, buf, segment, flags);
if (n < 0) {
return n;
}
remaining -= segment;
buf += segment;
}
return len;
}
#endif
static int
internal_setblocking(PySocketSockObject *s, int block) {
#if !defined(RISCOS)
#if !defined(MS_WINDOWS)
int delay_flag;
#endif
#endif
Py_BEGIN_ALLOW_THREADS
#if defined(__BEOS__)
block = !block;
setsockopt(s->sock_fd, SOL_SOCKET, SO_NONBLOCK,
(void *)(&block), sizeof(int));
#else
#if !defined(RISCOS)
#if !defined(MS_WINDOWS)
#if defined(PYOS_OS2) && !defined(PYCC_GCC)
block = !block;
ioctl(s->sock_fd, FIONBIO, (caddr_t)&block, sizeof(block));
#elif defined(__VMS)
block = !block;
ioctl(s->sock_fd, FIONBIO, (unsigned int *)&block);
#else
delay_flag = fcntl(s->sock_fd, F_GETFL, 0);
if (block)
delay_flag &= (~O_NONBLOCK);
else
delay_flag |= O_NONBLOCK;
fcntl(s->sock_fd, F_SETFL, delay_flag);
#endif
#else
block = !block;
ioctlsocket(s->sock_fd, FIONBIO, (u_long*)&block);
#endif
#else
block = !block;
socketioctl(s->sock_fd, FIONBIO, (u_long*)&block);
#endif
#endif
Py_END_ALLOW_THREADS
return 1;
}
static int
internal_select(PySocketSockObject *s, int writing) {
int n;
if (s->sock_timeout <= 0.0)
return 0;
if (s->sock_fd < 0)
return 0;
#if defined(HAVE_POLL)
{
struct pollfd pollfd;
int timeout;
pollfd.fd = s->sock_fd;
pollfd.events = writing ? POLLOUT : POLLIN;
timeout = (int)(s->sock_timeout * 1000 + 0.5);
n = poll(&pollfd, 1, timeout);
}
#else
{
fd_set fds;
struct timeval tv;
tv.tv_sec = (int)s->sock_timeout;
tv.tv_usec = (int)((s->sock_timeout - tv.tv_sec) * 1e6);
FD_ZERO(&fds);
FD_SET(s->sock_fd, &fds);
if (writing)
n = select(s->sock_fd+1, NULL, &fds, NULL, &tv);
else
n = select(s->sock_fd+1, &fds, NULL, NULL, &tv);
}
#endif
if (n < 0)
return -1;
if (n == 0)
return 1;
return 0;
}
static double defaulttimeout = -1.0;
PyMODINIT_FUNC
init_sockobject(PySocketSockObject *s,
SOCKET_T fd, int family, int type, int proto) {
#if defined(RISCOS)
int block = 1;
#endif
s->sock_fd = fd;
s->sock_family = family;
s->sock_type = type;
s->sock_proto = proto;
s->sock_timeout = defaulttimeout;
s->errorhandler = &set_error;
if (defaulttimeout >= 0.0)
internal_setblocking(s, 0);
#if defined(RISCOS)
if (taskwindow)
socketioctl(s->sock_fd, 0x80046679, (u_long*)&block);
#endif
}
static PySocketSockObject *
new_sockobject(SOCKET_T fd, int family, int type, int proto) {
PySocketSockObject *s;
s = (PySocketSockObject *)
PyType_GenericNew(&sock_type, NULL, NULL);
if (s != NULL)
init_sockobject(s, fd, family, type, proto);
return s;
}
#if defined(USE_GETHOSTBYNAME_LOCK) || defined(USE_GETADDRINFO_LOCK)
PyThread_type_lock netdb_lock;
#endif
static int
setipaddr(char *name, struct sockaddr *addr_ret, size_t addr_ret_size, int af) {
struct addrinfo hints, *res;
int error;
int d1, d2, d3, d4;
char ch;
memset((void *) addr_ret, '\0', sizeof(*addr_ret));
if (name[0] == '\0') {
int siz;
memset(&hints, 0, sizeof(hints));
hints.ai_family = af;
hints.ai_socktype = SOCK_DGRAM;
hints.ai_flags = AI_PASSIVE;
Py_BEGIN_ALLOW_THREADS
ACQUIRE_GETADDRINFO_LOCK
error = getaddrinfo(NULL, "0", &hints, &res);
Py_END_ALLOW_THREADS
RELEASE_GETADDRINFO_LOCK
if (error) {
set_gaierror(error);
return -1;
}
switch (res->ai_family) {
case AF_INET:
siz = 4;
break;
#if defined(ENABLE_IPV6)
case AF_INET6:
siz = 16;
break;
#endif
default:
freeaddrinfo(res);
PyErr_SetString(socket_error,
"unsupported address family");
return -1;
}
if (res->ai_next) {
freeaddrinfo(res);
PyErr_SetString(socket_error,
"wildcard resolved to multiple address");
return -1;
}
if (res->ai_addrlen < addr_ret_size)
addr_ret_size = res->ai_addrlen;
memcpy(addr_ret, res->ai_addr, addr_ret_size);
freeaddrinfo(res);
return siz;
}
if (name[0] == '<' && strcmp(name, "<broadcast>") == 0) {
struct sockaddr_in *sin;
if (af != AF_INET && af != AF_UNSPEC) {
PyErr_SetString(socket_error,
"address family mismatched");
return -1;
}
sin = (struct sockaddr_in *)addr_ret;
memset((void *) sin, '\0', sizeof(*sin));
sin->sin_family = AF_INET;
#if defined(HAVE_SOCKADDR_SA_LEN)
sin->sin_len = sizeof(*sin);
#endif
sin->sin_addr.s_addr = INADDR_BROADCAST;
return sizeof(sin->sin_addr);
}
if (sscanf(name, "%d.%d.%d.%d%c", &d1, &d2, &d3, &d4, &ch) == 4 &&
0 <= d1 && d1 <= 255 && 0 <= d2 && d2 <= 255 &&
0 <= d3 && d3 <= 255 && 0 <= d4 && d4 <= 255) {
struct sockaddr_in *sin;
sin = (struct sockaddr_in *)addr_ret;
sin->sin_addr.s_addr = htonl(
((long) d1 << 24) | ((long) d2 << 16) |
((long) d3 << 8) | ((long) d4 << 0));
sin->sin_family = AF_INET;
#if defined(HAVE_SOCKADDR_SA_LEN)
sin->sin_len = sizeof(*sin);
#endif
return 4;
}
memset(&hints, 0, sizeof(hints));
hints.ai_family = af;
Py_BEGIN_ALLOW_THREADS
ACQUIRE_GETADDRINFO_LOCK
error = getaddrinfo(name, NULL, &hints, &res);
#if defined(__digital__) && defined(__unix__)
if (error == EAI_NONAME && af == AF_UNSPEC) {
hints.ai_family = AF_INET;
error = getaddrinfo(name, NULL, &hints, &res);
}
#endif
Py_END_ALLOW_THREADS
RELEASE_GETADDRINFO_LOCK
if (error) {
set_gaierror(error);
return -1;
}
if (res->ai_addrlen < addr_ret_size)
addr_ret_size = res->ai_addrlen;
memcpy((char *) addr_ret, res->ai_addr, addr_ret_size);
freeaddrinfo(res);
switch (addr_ret->sa_family) {
case AF_INET:
return 4;
#if defined(ENABLE_IPV6)
case AF_INET6:
return 16;
#endif
default:
PyErr_SetString(socket_error, "unknown address family");
return -1;
}
}
static PyObject *
makeipaddr(struct sockaddr *addr, int addrlen) {
char buf[NI_MAXHOST];
int error;
error = getnameinfo(addr, addrlen, buf, sizeof(buf), NULL, 0,
NI_NUMERICHOST);
if (error) {
set_gaierror(error);
return NULL;
}
return PyString_FromString(buf);
}
#if defined(USE_BLUETOOTH)
static int
setbdaddr(char *name, bdaddr_t *bdaddr) {
unsigned int b0, b1, b2, b3, b4, b5;
char ch;
int n;
n = sscanf(name, "%X:%X:%X:%X:%X:%X%c",
&b5, &b4, &b3, &b2, &b1, &b0, &ch);
if (n == 6 && (b0 | b1 | b2 | b3 | b4 | b5) < 256) {
bdaddr->b[0] = b0;
bdaddr->b[1] = b1;
bdaddr->b[2] = b2;
bdaddr->b[3] = b3;
bdaddr->b[4] = b4;
bdaddr->b[5] = b5;
return 6;
} else {
PyErr_SetString(socket_error, "bad bluetooth address");
return -1;
}
}
static PyObject *
makebdaddr(bdaddr_t *bdaddr) {
char buf[(6 * 2) + 5 + 1];
sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
bdaddr->b[5], bdaddr->b[4], bdaddr->b[3],
bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);
return PyString_FromString(buf);
}
#endif
static PyObject *
makesockaddr(int sockfd, struct sockaddr *addr, int addrlen, int proto) {
if (addrlen == 0) {
Py_INCREF(Py_None);
return Py_None;
}
#if defined(__BEOS__)
addr->sa_family = AF_INET;
#endif
switch (addr->sa_family) {
case AF_INET: {
struct sockaddr_in *a;
PyObject *addrobj = makeipaddr(addr, sizeof(*a));
PyObject *ret = NULL;
if (addrobj) {
a = (struct sockaddr_in *)addr;
ret = Py_BuildValue("Oi", addrobj, ntohs(a->sin_port));
Py_DECREF(addrobj);
}
return ret;
}
#if defined(AF_UNIX)
case AF_UNIX: {
struct sockaddr_un *a = (struct sockaddr_un *) addr;
#if defined(linux)
if (a->sun_path[0] == 0) {
addrlen -= offsetof(struct sockaddr_un, sun_path);
return PyString_FromStringAndSize(a->sun_path,
addrlen);
} else
#endif
{
return PyString_FromString(a->sun_path);
}
}
#endif
#if defined(AF_NETLINK)
case AF_NETLINK: {
struct sockaddr_nl *a = (struct sockaddr_nl *) addr;
return Py_BuildValue("II", a->nl_pid, a->nl_groups);
}
#endif
#if defined(ENABLE_IPV6)
case AF_INET6: {
struct sockaddr_in6 *a;
PyObject *addrobj = makeipaddr(addr, sizeof(*a));
PyObject *ret = NULL;
if (addrobj) {
a = (struct sockaddr_in6 *)addr;
ret = Py_BuildValue("Oiii",
addrobj,
ntohs(a->sin6_port),
a->sin6_flowinfo,
a->sin6_scope_id);
Py_DECREF(addrobj);
}
return ret;
}
#endif
#if defined(USE_BLUETOOTH)
case AF_BLUETOOTH:
switch (proto) {
case BTPROTO_L2CAP: {
struct sockaddr_l2 *a = (struct sockaddr_l2 *) addr;
PyObject *addrobj = makebdaddr(&_BT_L2_MEMB(a, bdaddr));
PyObject *ret = NULL;
if (addrobj) {
ret = Py_BuildValue("Oi",
addrobj,
_BT_L2_MEMB(a, psm));
Py_DECREF(addrobj);
}
return ret;
}
case BTPROTO_RFCOMM: {
struct sockaddr_rc *a = (struct sockaddr_rc *) addr;
PyObject *addrobj = makebdaddr(&_BT_RC_MEMB(a, bdaddr));
PyObject *ret = NULL;
if (addrobj) {
ret = Py_BuildValue("Oi",
addrobj,
_BT_RC_MEMB(a, channel));
Py_DECREF(addrobj);
}
return ret;
}
case BTPROTO_HCI: {
struct sockaddr_hci *a = (struct sockaddr_hci *) addr;
PyObject *ret = NULL;
ret = Py_BuildValue("i", _BT_HCI_MEMB(a, dev));
return ret;
}
#if !defined(__FreeBSD__)
case BTPROTO_SCO: {
struct sockaddr_sco *a = (struct sockaddr_sco *) addr;
return makebdaddr(&_BT_SCO_MEMB(a, bdaddr));
}
#endif
}
#endif
#if defined(HAVE_NETPACKET_PACKET_H)
case AF_PACKET: {
struct sockaddr_ll *a = (struct sockaddr_ll *)addr;
char *ifname = "";
struct ifreq ifr;
if (a->sll_ifindex) {
ifr.ifr_ifindex = a->sll_ifindex;
if (ioctl(sockfd, SIOCGIFNAME, &ifr) == 0)
ifname = ifr.ifr_name;
}
return Py_BuildValue("shbhs#",
ifname,
ntohs(a->sll_protocol),
a->sll_pkttype,
a->sll_hatype,
a->sll_addr,
a->sll_halen);
}
#endif
#if defined(HAVE_LINUX_TIPC_H)
case AF_TIPC: {
struct sockaddr_tipc *a = (struct sockaddr_tipc *) addr;
if (a->addrtype == TIPC_ADDR_NAMESEQ) {
return Py_BuildValue("IIIII",
a->addrtype,
a->addr.nameseq.type,
a->addr.nameseq.lower,
a->addr.nameseq.upper,
a->scope);
} else if (a->addrtype == TIPC_ADDR_NAME) {
return Py_BuildValue("IIIII",
a->addrtype,
a->addr.name.name.type,
a->addr.name.name.instance,
a->addr.name.name.instance,
a->scope);
} else if (a->addrtype == TIPC_ADDR_ID) {
return Py_BuildValue("IIIII",
a->addrtype,
a->addr.id.node,
a->addr.id.ref,
0,
a->scope);
} else {
PyErr_SetString(PyExc_TypeError,
"Invalid address type");
return NULL;
}
}
#endif
default:
return Py_BuildValue("is#",
addr->sa_family,
addr->sa_data,
sizeof(addr->sa_data));
}
}
static int
getsockaddrarg(PySocketSockObject *s, PyObject *args,
struct sockaddr *addr_ret, int *len_ret) {
switch (s->sock_family) {
#if defined(AF_UNIX)
case AF_UNIX: {
struct sockaddr_un* addr;
char *path;
int len;
if (!PyArg_Parse(args, "t#", &path, &len))
return 0;
addr = (struct sockaddr_un*)addr_ret;
#if defined(linux)
if (len > 0 && path[0] == 0) {
if (len > sizeof addr->sun_path) {
PyErr_SetString(socket_error,
"AF_UNIX path too long");
return 0;
}
} else
#endif
{
if (len >= sizeof addr->sun_path) {
PyErr_SetString(socket_error,
"AF_UNIX path too long");
return 0;
}
addr->sun_path[len] = 0;
}
addr->sun_family = s->sock_family;
memcpy(addr->sun_path, path, len);
#if defined(PYOS_OS2)
*len_ret = sizeof(*addr);
#else
*len_ret = len + offsetof(struct sockaddr_un, sun_path);
#endif
return 1;
}
#endif
#if defined(AF_NETLINK)
case AF_NETLINK: {
struct sockaddr_nl* addr;
int pid, groups;
addr = (struct sockaddr_nl *)addr_ret;
if (!PyTuple_Check(args)) {
PyErr_Format(
PyExc_TypeError,
"getsockaddrarg: "
"AF_NETLINK address must be tuple, not %.500s",
Py_TYPE(args)->tp_name);
return 0;
}
if (!PyArg_ParseTuple(args, "II:getsockaddrarg", &pid, &groups))
return 0;
addr->nl_family = AF_NETLINK;
addr->nl_pid = pid;
addr->nl_groups = groups;
*len_ret = sizeof(*addr);
return 1;
}
#endif
case AF_INET: {
struct sockaddr_in* addr;
char *host;
int port, result;
if (!PyTuple_Check(args)) {
PyErr_Format(
PyExc_TypeError,
"getsockaddrarg: "
"AF_INET address must be tuple, not %.500s",
Py_TYPE(args)->tp_name);
return 0;
}
if (!PyArg_ParseTuple(args, "eti:getsockaddrarg",
"idna", &host, &port))
return 0;
addr=(struct sockaddr_in*)addr_ret;
result = setipaddr(host, (struct sockaddr *)addr,
sizeof(*addr), AF_INET);
PyMem_Free(host);
if (result < 0)
return 0;
addr->sin_family = AF_INET;
addr->sin_port = htons((short)port);
*len_ret = sizeof *addr;
return 1;
}
#if defined(ENABLE_IPV6)
case AF_INET6: {
struct sockaddr_in6* addr;
char *host;
int port, flowinfo, scope_id, result;
flowinfo = scope_id = 0;
if (!PyTuple_Check(args)) {
PyErr_Format(
PyExc_TypeError,
"getsockaddrarg: "
"AF_INET6 address must be tuple, not %.500s",
Py_TYPE(args)->tp_name);
return 0;
}
if (!PyArg_ParseTuple(args, "eti|ii",
"idna", &host, &port, &flowinfo,
&scope_id)) {
return 0;
}
addr = (struct sockaddr_in6*)addr_ret;
result = setipaddr(host, (struct sockaddr *)addr,
sizeof(*addr), AF_INET6);
PyMem_Free(host);
if (result < 0)
return 0;
addr->sin6_family = s->sock_family;
addr->sin6_port = htons((short)port);
addr->sin6_flowinfo = flowinfo;
addr->sin6_scope_id = scope_id;
*len_ret = sizeof *addr;
return 1;
}
#endif
#if defined(USE_BLUETOOTH)
case AF_BLUETOOTH: {
switch (s->sock_proto) {
case BTPROTO_L2CAP: {
struct sockaddr_l2 *addr;
char *straddr;
addr = (struct sockaddr_l2 *)addr_ret;
_BT_L2_MEMB(addr, family) = AF_BLUETOOTH;
if (!PyArg_ParseTuple(args, "si", &straddr,
&_BT_L2_MEMB(addr, psm))) {
PyErr_SetString(socket_error, "getsockaddrarg: "
"wrong format");
return 0;
}
if (setbdaddr(straddr, &_BT_L2_MEMB(addr, bdaddr)) < 0)
return 0;
*len_ret = sizeof *addr;
return 1;
}
case BTPROTO_RFCOMM: {
struct sockaddr_rc *addr;
char *straddr;
addr = (struct sockaddr_rc *)addr_ret;
_BT_RC_MEMB(addr, family) = AF_BLUETOOTH;
if (!PyArg_ParseTuple(args, "si", &straddr,
&_BT_RC_MEMB(addr, channel))) {
PyErr_SetString(socket_error, "getsockaddrarg: "
"wrong format");
return 0;
}
if (setbdaddr(straddr, &_BT_RC_MEMB(addr, bdaddr)) < 0)
return 0;
*len_ret = sizeof *addr;
return 1;
}
case BTPROTO_HCI: {
struct sockaddr_hci *addr = (struct sockaddr_hci *)addr_ret;
_BT_HCI_MEMB(addr, family) = AF_BLUETOOTH;
if (!PyArg_ParseTuple(args, "i", &_BT_HCI_MEMB(addr, dev))) {
PyErr_SetString(socket_error, "getsockaddrarg: "
"wrong format");
return 0;
}
*len_ret = sizeof *addr;
return 1;
}
#if !defined(__FreeBSD__)
case BTPROTO_SCO: {
struct sockaddr_sco *addr;
char *straddr;
addr = (struct sockaddr_sco *)addr_ret;
_BT_SCO_MEMB(addr, family) = AF_BLUETOOTH;
straddr = PyString_AsString(args);
if (straddr == NULL) {
PyErr_SetString(socket_error, "getsockaddrarg: "
"wrong format");
return 0;
}
if (setbdaddr(straddr, &_BT_SCO_MEMB(addr, bdaddr)) < 0)
return 0;
*len_ret = sizeof *addr;
return 1;
}
#endif
default:
PyErr_SetString(socket_error, "getsockaddrarg: unknown Bluetooth protocol");
return 0;
}
}
#endif
#if defined(HAVE_NETPACKET_PACKET_H)
case AF_PACKET: {
struct sockaddr_ll* addr;
struct ifreq ifr;
char *interfaceName;
int protoNumber;
int hatype = 0;
int pkttype = 0;
char *haddr = NULL;
unsigned int halen = 0;
if (!PyTuple_Check(args)) {
PyErr_Format(
PyExc_TypeError,
"getsockaddrarg: "
"AF_PACKET address must be tuple, not %.500s",
Py_TYPE(args)->tp_name);
return 0;
}
if (!PyArg_ParseTuple(args, "si|iis#", &interfaceName,
&protoNumber, &pkttype, &hatype,
&haddr, &halen))
return 0;
strncpy(ifr.ifr_name, interfaceName, sizeof(ifr.ifr_name));
ifr.ifr_name[(sizeof(ifr.ifr_name))-1] = '\0';
if (ioctl(s->sock_fd, SIOCGIFINDEX, &ifr) < 0) {
s->errorhandler();
return 0;
}
if (halen > 8) {
PyErr_SetString(PyExc_ValueError,
"Hardware address must be 8 bytes or less");
return 0;
}
addr = (struct sockaddr_ll*)addr_ret;
addr->sll_family = AF_PACKET;
addr->sll_protocol = htons((short)protoNumber);
addr->sll_ifindex = ifr.ifr_ifindex;
addr->sll_pkttype = pkttype;
addr->sll_hatype = hatype;
if (halen != 0) {
memcpy(&addr->sll_addr, haddr, halen);
}
addr->sll_halen = halen;
*len_ret = sizeof *addr;
return 1;
}
#endif
#if defined(HAVE_LINUX_TIPC_H)
case AF_TIPC: {
unsigned int atype, v1, v2, v3;
unsigned int scope = TIPC_CLUSTER_SCOPE;
struct sockaddr_tipc *addr;
if (!PyTuple_Check(args)) {
PyErr_Format(
PyExc_TypeError,
"getsockaddrarg: "
"AF_TIPC address must be tuple, not %.500s",
Py_TYPE(args)->tp_name);
return 0;
}
if (!PyArg_ParseTuple(args,
"IIII|I;Invalid TIPC address format",
&atype, &v1, &v2, &v3, &scope))
return 0;
addr = (struct sockaddr_tipc *) addr_ret;
memset(addr, 0, sizeof(struct sockaddr_tipc));
addr->family = AF_TIPC;
addr->scope = scope;
addr->addrtype = atype;
if (atype == TIPC_ADDR_NAMESEQ) {
addr->addr.nameseq.type = v1;
addr->addr.nameseq.lower = v2;
addr->addr.nameseq.upper = v3;
} else if (atype == TIPC_ADDR_NAME) {
addr->addr.name.name.type = v1;
addr->addr.name.name.instance = v2;
} else if (atype == TIPC_ADDR_ID) {
addr->addr.id.node = v1;
addr->addr.id.ref = v2;
} else {
PyErr_SetString(PyExc_TypeError, "Invalid address type");
return 0;
}
*len_ret = sizeof(*addr);
return 1;
}
#endif
default:
PyErr_SetString(socket_error, "getsockaddrarg: bad family");
return 0;
}
}
static int
getsockaddrlen(PySocketSockObject *s, socklen_t *len_ret) {
switch (s->sock_family) {
#if defined(AF_UNIX)
case AF_UNIX: {
*len_ret = sizeof (struct sockaddr_un);
return 1;
}
#endif
#if defined(AF_NETLINK)
case AF_NETLINK: {
*len_ret = sizeof (struct sockaddr_nl);
return 1;
}
#endif
case AF_INET: {
*len_ret = sizeof (struct sockaddr_in);
return 1;
}
#if defined(ENABLE_IPV6)
case AF_INET6: {
*len_ret = sizeof (struct sockaddr_in6);
return 1;
}
#endif
#if defined(USE_BLUETOOTH)
case AF_BLUETOOTH: {
switch(s->sock_proto) {
case BTPROTO_L2CAP:
*len_ret = sizeof (struct sockaddr_l2);
return 1;
case BTPROTO_RFCOMM:
*len_ret = sizeof (struct sockaddr_rc);
return 1;
case BTPROTO_HCI:
*len_ret = sizeof (struct sockaddr_hci);
return 1;
#if !defined(__FreeBSD__)
case BTPROTO_SCO:
*len_ret = sizeof (struct sockaddr_sco);
return 1;
#endif
default:
PyErr_SetString(socket_error, "getsockaddrlen: "
"unknown BT protocol");
return 0;
}
}
#endif
#if defined(HAVE_NETPACKET_PACKET_H)
case AF_PACKET: {
*len_ret = sizeof (struct sockaddr_ll);
return 1;
}
#endif
#if defined(HAVE_LINUX_TIPC_H)
case AF_TIPC: {
*len_ret = sizeof (struct sockaddr_tipc);
return 1;
}
#endif
default:
PyErr_SetString(socket_error, "getsockaddrlen: bad family");
return 0;
}
}
static PyObject *
sock_accept(PySocketSockObject *s) {
sock_addr_t addrbuf;
SOCKET_T newfd;
socklen_t addrlen;
PyObject *sock = NULL;
PyObject *addr = NULL;
PyObject *res = NULL;
int timeout;
if (!getsockaddrlen(s, &addrlen))
return NULL;
memset(&addrbuf, 0, addrlen);
#if defined(MS_WINDOWS)
newfd = INVALID_SOCKET;
#else
newfd = -1;
#endif
if (!IS_SELECTABLE(s))
return select_error();
Py_BEGIN_ALLOW_THREADS
timeout = internal_select(s, 0);
if (!timeout)
newfd = accept(s->sock_fd, SAS2SA(&addrbuf), &addrlen);
Py_END_ALLOW_THREADS
if (timeout == 1) {
PyErr_SetString(socket_timeout, "timed out");
return NULL;
}
#if defined(MS_WINDOWS)
if (newfd == INVALID_SOCKET)
#else
if (newfd < 0)
#endif
return s->errorhandler();
sock = (PyObject *) new_sockobject(newfd,
s->sock_family,
s->sock_type,
s->sock_proto);
if (sock == NULL) {
SOCKETCLOSE(newfd);
goto finally;
}
addr = makesockaddr(s->sock_fd, SAS2SA(&addrbuf),
addrlen, s->sock_proto);
if (addr == NULL)
goto finally;
res = PyTuple_Pack(2, sock, addr);
finally:
Py_XDECREF(sock);
Py_XDECREF(addr);
return res;
}
PyDoc_STRVAR(accept_doc,
"accept() -> (socket object, address info)\n\
\n\
Wait for an incoming connection. Return a new socket representing the\n\
connection, and the address of the client. For IP sockets, the address\n\
info is a pair (hostaddr, port).");
static PyObject *
sock_setblocking(PySocketSockObject *s, PyObject *arg) {
int block;
block = PyInt_AsLong(arg);
if (block == -1 && PyErr_Occurred())
return NULL;
s->sock_timeout = block ? -1.0 : 0.0;
internal_setblocking(s, block);
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(setblocking_doc,
"setblocking(flag)\n\
\n\
Set the socket to blocking (flag is true) or non-blocking (false).\n\
setblocking(True) is equivalent to settimeout(None);\n\
setblocking(False) is equivalent to settimeout(0.0).");
static PyObject *
sock_settimeout(PySocketSockObject *s, PyObject *arg) {
double timeout;
if (arg == Py_None)
timeout = -1.0;
else {
timeout = PyFloat_AsDouble(arg);
if (timeout < 0.0) {
if (!PyErr_Occurred())
PyErr_SetString(PyExc_ValueError,
"Timeout value out of range");
return NULL;
}
}
s->sock_timeout = timeout;
internal_setblocking(s, timeout < 0.0);
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(settimeout_doc,
"settimeout(timeout)\n\
\n\
Set a timeout on socket operations. 'timeout' can be a float,\n\
giving in seconds, or None. Setting a timeout of None disables\n\
the timeout feature and is equivalent to setblocking(1).\n\
Setting a timeout of zero is the same as setblocking(0).");
static PyObject *
sock_gettimeout(PySocketSockObject *s) {
if (s->sock_timeout < 0.0) {
Py_INCREF(Py_None);
return Py_None;
} else
return PyFloat_FromDouble(s->sock_timeout);
}
PyDoc_STRVAR(gettimeout_doc,
"gettimeout() -> timeout\n\
\n\
Returns the timeout in floating seconds associated with socket \n\
operations. A timeout of None indicates that timeouts on socket \n\
operations are disabled.");
#if defined(RISCOS)
static PyObject *
sock_sleeptaskw(PySocketSockObject *s,PyObject *arg) {
int block;
block = PyInt_AsLong(arg);
if (block == -1 && PyErr_Occurred())
return NULL;
Py_BEGIN_ALLOW_THREADS
socketioctl(s->sock_fd, 0x80046679, (u_long*)&block);
Py_END_ALLOW_THREADS
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(sleeptaskw_doc,
"sleeptaskw(flag)\n\
\n\
Allow sleeps in taskwindows.");
#endif
static PyObject *
sock_setsockopt(PySocketSockObject *s, PyObject *args) {
int level;
int optname;
int res;
char *buf;
int buflen;
int flag;
if (PyArg_ParseTuple(args, "iii:setsockopt",
&level, &optname, &flag)) {
buf = (char *) &flag;
buflen = sizeof flag;
} else {
PyErr_Clear();
if (!PyArg_ParseTuple(args, "iis#:setsockopt",
&level, &optname, &buf, &buflen))
return NULL;
}
res = setsockopt(s->sock_fd, level, optname, (void *)buf, buflen);
if (res < 0)
return s->errorhandler();
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(setsockopt_doc,
"setsockopt(level, option, value)\n\
\n\
Set a socket option. See the Unix manual for level and option.\n\
The value argument can either be an integer or a string.");
static PyObject *
sock_getsockopt(PySocketSockObject *s, PyObject *args) {
int level;
int optname;
int res;
PyObject *buf;
socklen_t buflen = 0;
#if defined(__BEOS__)
PyErr_SetString(socket_error, "getsockopt not supported");
return NULL;
#else
if (!PyArg_ParseTuple(args, "ii|i:getsockopt",
&level, &optname, &buflen))
return NULL;
if (buflen == 0) {
int flag = 0;
socklen_t flagsize = sizeof flag;
res = getsockopt(s->sock_fd, level, optname,
(void *)&flag, &flagsize);
if (res < 0)
return s->errorhandler();
return PyInt_FromLong(flag);
}
#if defined(__VMS)
if (buflen > 1024) {
#else
if (buflen <= 0 || buflen > 1024) {
#endif
PyErr_SetString(socket_error,
"getsockopt buflen out of range");
return NULL;
}
buf = PyString_FromStringAndSize((char *)NULL, buflen);
if (buf == NULL)
return NULL;
res = getsockopt(s->sock_fd, level, optname,
(void *)PyString_AS_STRING(buf), &buflen);
if (res < 0) {
Py_DECREF(buf);
return s->errorhandler();
}
_PyString_Resize(&buf, buflen);
return buf;
#endif
}
PyDoc_STRVAR(getsockopt_doc,
"getsockopt(level, option[, buffersize]) -> value\n\
\n\
Get a socket option. See the Unix manual for level and option.\n\
If a nonzero buffersize argument is given, the return value is a\n\
string of that length; otherwise it is an integer.");
static PyObject *
sock_bind(PySocketSockObject *s, PyObject *addro) {
sock_addr_t addrbuf;
int addrlen;
int res;
if (!getsockaddrarg(s, addro, SAS2SA(&addrbuf), &addrlen))
return NULL;
Py_BEGIN_ALLOW_THREADS
res = bind(s->sock_fd, SAS2SA(&addrbuf), addrlen);
Py_END_ALLOW_THREADS
if (res < 0)
return s->errorhandler();
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(bind_doc,
"bind(address)\n\
\n\
Bind the socket to a local address. For IP sockets, the address is a\n\
pair (host, port); the host must refer to the local host. For raw packet\n\
sockets the address is a tuple (ifname, proto [,pkttype [,hatype]])");
static PyObject *
sock_close(PySocketSockObject *s) {
SOCKET_T fd;
if ((fd = s->sock_fd) != -1) {
s->sock_fd = -1;
Py_BEGIN_ALLOW_THREADS
(void) SOCKETCLOSE(fd);
Py_END_ALLOW_THREADS
}
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(close_doc,
"close()\n\
\n\
Close the socket. It cannot be used after this call.");
static int
internal_connect(PySocketSockObject *s, struct sockaddr *addr, int addrlen,
int *timeoutp) {
int res, timeout;
timeout = 0;
res = connect(s->sock_fd, addr, addrlen);
#if defined(MS_WINDOWS)
if (s->sock_timeout > 0.0) {
if (res < 0 && WSAGetLastError() == WSAEWOULDBLOCK &&
IS_SELECTABLE(s)) {
fd_set fds;
fd_set fds_exc;
struct timeval tv;
tv.tv_sec = (int)s->sock_timeout;
tv.tv_usec = (int)((s->sock_timeout - tv.tv_sec) * 1e6);
FD_ZERO(&fds);
FD_SET(s->sock_fd, &fds);
FD_ZERO(&fds_exc);
FD_SET(s->sock_fd, &fds_exc);
res = select(s->sock_fd+1, NULL, &fds, &fds_exc, &tv);
if (res == 0) {
res = WSAEWOULDBLOCK;
timeout = 1;
} else if (res > 0) {
if (FD_ISSET(s->sock_fd, &fds))
res = 0;
else {
int res_size = sizeof res;
assert(FD_ISSET(s->sock_fd, &fds_exc));
if (0 == getsockopt(s->sock_fd, SOL_SOCKET, SO_ERROR,
(char *)&res, &res_size))
WSASetLastError(res);
else
res = WSAGetLastError();
}
}
}
}
if (res < 0)
res = WSAGetLastError();
#else
if (s->sock_timeout > 0.0) {
if (res < 0 && errno == EINPROGRESS && IS_SELECTABLE(s)) {
timeout = internal_select(s, 1);
if (timeout == 0) {
socklen_t res_size = sizeof res;
(void)getsockopt(s->sock_fd, SOL_SOCKET,
SO_ERROR, &res, &res_size);
if (res == EISCONN)
res = 0;
errno = res;
} else if (timeout == -1) {
res = errno;
} else
res = EWOULDBLOCK;
}
}
if (res < 0)
res = errno;
#endif
*timeoutp = timeout;
return res;
}
static PyObject *
sock_connect(PySocketSockObject *s, PyObject *addro) {
sock_addr_t addrbuf;
int addrlen;
int res;
int timeout;
if (!getsockaddrarg(s, addro, SAS2SA(&addrbuf), &addrlen))
return NULL;
Py_BEGIN_ALLOW_THREADS
res = internal_connect(s, SAS2SA(&addrbuf), addrlen, &timeout);
Py_END_ALLOW_THREADS
if (timeout == 1) {
PyErr_SetString(socket_timeout, "timed out");
return NULL;
}
if (res != 0)
return s->errorhandler();
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(connect_doc,
"connect(address)\n\
\n\
Connect the socket to a remote address. For IP sockets, the address\n\
is a pair (host, port).");
static PyObject *
sock_connect_ex(PySocketSockObject *s, PyObject *addro) {
sock_addr_t addrbuf;
int addrlen;
int res;
int timeout;
if (!getsockaddrarg(s, addro, SAS2SA(&addrbuf), &addrlen))
return NULL;
Py_BEGIN_ALLOW_THREADS
res = internal_connect(s, SAS2SA(&addrbuf), addrlen, &timeout);
Py_END_ALLOW_THREADS
#if defined(EINTR)
if (res == EINTR && PyErr_CheckSignals())
return NULL;
#endif
return PyInt_FromLong((long) res);
}
PyDoc_STRVAR(connect_ex_doc,
"connect_ex(address) -> errno\n\
\n\
This is like connect(address), but returns an error code (the errno value)\n\
instead of raising an exception when an error occurs.");
static PyObject *
sock_fileno(PySocketSockObject *s) {
#if SIZEOF_SOCKET_T <= SIZEOF_LONG
return PyInt_FromLong((long) s->sock_fd);
#else
return PyLong_FromLongLong((PY_LONG_LONG)s->sock_fd);
#endif
}
PyDoc_STRVAR(fileno_doc,
"fileno() -> integer\n\
\n\
Return the integer file descriptor of the socket.");
#if !defined(NO_DUP)
static PyObject *
sock_dup(PySocketSockObject *s) {
SOCKET_T newfd;
PyObject *sock;
newfd = dup(s->sock_fd);
if (newfd < 0)
return s->errorhandler();
sock = (PyObject *) new_sockobject(newfd,
s->sock_family,
s->sock_type,
s->sock_proto);
if (sock == NULL)
SOCKETCLOSE(newfd);
return sock;
}
PyDoc_STRVAR(dup_doc,
"dup() -> socket object\n\
\n\
Return a new socket object connected to the same system resource.");
#endif
static PyObject *
sock_getsockname(PySocketSockObject *s) {
sock_addr_t addrbuf;
int res;
socklen_t addrlen;
if (!getsockaddrlen(s, &addrlen))
return NULL;
memset(&addrbuf, 0, addrlen);
Py_BEGIN_ALLOW_THREADS
res = getsockname(s->sock_fd, SAS2SA(&addrbuf), &addrlen);
Py_END_ALLOW_THREADS
if (res < 0)
return s->errorhandler();
return makesockaddr(s->sock_fd, SAS2SA(&addrbuf), addrlen,
s->sock_proto);
}
PyDoc_STRVAR(getsockname_doc,
"getsockname() -> address info\n\
\n\
Return the address of the local endpoint. For IP sockets, the address\n\
info is a pair (hostaddr, port).");
#if defined(HAVE_GETPEERNAME)
static PyObject *
sock_getpeername(PySocketSockObject *s) {
sock_addr_t addrbuf;
int res;
socklen_t addrlen;
if (!getsockaddrlen(s, &addrlen))
return NULL;
memset(&addrbuf, 0, addrlen);
Py_BEGIN_ALLOW_THREADS
res = getpeername(s->sock_fd, SAS2SA(&addrbuf), &addrlen);
Py_END_ALLOW_THREADS
if (res < 0)
return s->errorhandler();
return makesockaddr(s->sock_fd, SAS2SA(&addrbuf), addrlen,
s->sock_proto);
}
PyDoc_STRVAR(getpeername_doc,
"getpeername() -> address info\n\
\n\
Return the address of the remote endpoint. For IP sockets, the address\n\
info is a pair (hostaddr, port).");
#endif
static PyObject *
sock_listen(PySocketSockObject *s, PyObject *arg) {
int backlog;
int res;
backlog = PyInt_AsLong(arg);
if (backlog == -1 && PyErr_Occurred())
return NULL;
Py_BEGIN_ALLOW_THREADS
if (backlog < 1)
backlog = 1;
res = listen(s->sock_fd, backlog);
Py_END_ALLOW_THREADS
if (res < 0)
return s->errorhandler();
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(listen_doc,
"listen(backlog)\n\
\n\
Enable a server to accept connections. The backlog argument must be at\n\
least 1; it specifies the number of unaccepted connection that the system\n\
will allow before refusing new connections.");
#if !defined(NO_DUP)
static PyObject *
sock_makefile(PySocketSockObject *s, PyObject *args) {
extern int fclose(FILE *);
char *mode = "r";
int bufsize = -1;
#if defined(MS_WIN32)
Py_intptr_t fd;
#else
int fd;
#endif
FILE *fp;
PyObject *f;
#if defined(__VMS)
char *mode_r = "r";
char *mode_w = "w";
#endif
if (!PyArg_ParseTuple(args, "|si:makefile", &mode, &bufsize))
return NULL;
#if defined(__VMS)
if (strcmp(mode,"rb") == 0) {
mode = mode_r;
} else {
if (strcmp(mode,"wb") == 0) {
mode = mode_w;
}
}
#endif
#if defined(MS_WIN32)
if (((fd = _open_osfhandle(s->sock_fd, _O_BINARY)) < 0) ||
((fd = dup(fd)) < 0) || ((fp = fdopen(fd, mode)) == NULL))
#else
if ((fd = dup(s->sock_fd)) < 0 || (fp = fdopen(fd, mode)) == NULL)
#endif
{
if (fd >= 0)
SOCKETCLOSE(fd);
return s->errorhandler();
}
f = PyFile_FromFile(fp, "<socket>", mode, fclose);
if (f != NULL)
PyFile_SetBufSize(f, bufsize);
return f;
}
PyDoc_STRVAR(makefile_doc,
"makefile([mode[, buffersize]]) -> file object\n\
\n\
Return a regular file object corresponding to the socket.\n\
The mode and buffersize arguments are as for the built-in open() function.");
#endif
static ssize_t
sock_recv_guts(PySocketSockObject *s, char* cbuf, int len, int flags) {
ssize_t outlen = -1;
int timeout;
#if defined(__VMS)
int remaining;
char *read_buf;
#endif
if (!IS_SELECTABLE(s)) {
select_error();
return -1;
}
#if !defined(__VMS)
Py_BEGIN_ALLOW_THREADS
timeout = internal_select(s, 0);
if (!timeout)
outlen = recv(s->sock_fd, cbuf, len, flags);
Py_END_ALLOW_THREADS
if (timeout == 1) {
PyErr_SetString(socket_timeout, "timed out");
return -1;
}
if (outlen < 0) {
s->errorhandler();
return -1;
}
#else
read_buf = cbuf;
remaining = len;
while (remaining != 0) {
unsigned int segment;
int nread = -1;
segment = remaining /SEGMENT_SIZE;
if (segment != 0) {
segment = SEGMENT_SIZE;
} else {
segment = remaining;
}
Py_BEGIN_ALLOW_THREADS
timeout = internal_select(s, 0);
if (!timeout)
nread = recv(s->sock_fd, read_buf, segment, flags);
Py_END_ALLOW_THREADS
if (timeout == 1) {
PyErr_SetString(socket_timeout, "timed out");
return -1;
}
if (nread < 0) {
s->errorhandler();
return -1;
}
if (nread != remaining) {
read_buf += nread;
break;
}
remaining -= segment;
read_buf += segment;
}
outlen = read_buf - cbuf;
#endif
return outlen;
}
static PyObject *
sock_recv(PySocketSockObject *s, PyObject *args) {
int recvlen, flags = 0;
ssize_t outlen;
PyObject *buf;
if (!PyArg_ParseTuple(args, "i|i:recv", &recvlen, &flags))
return NULL;
if (recvlen < 0) {
PyErr_SetString(PyExc_ValueError,
"negative buffersize in recv");
return NULL;
}
buf = PyString_FromStringAndSize((char *) 0, recvlen);
if (buf == NULL)
return NULL;
outlen = sock_recv_guts(s, PyString_AS_STRING(buf), recvlen, flags);
if (outlen < 0) {
Py_DECREF(buf);
return NULL;
}
if (outlen != recvlen) {
if (_PyString_Resize(&buf, outlen) < 0)
return NULL;
}
return buf;
}
PyDoc_STRVAR(recv_doc,
"recv(buffersize[, flags]) -> data\n\
\n\
Receive up to buffersize bytes from the socket. For the optional flags\n\
argument, see the Unix manual. When no data is available, block until\n\
at least one byte is available or until the remote end is closed. When\n\
the remote end is closed and all data is read, return the empty string.");
static PyObject*
sock_recv_into(PySocketSockObject *s, PyObject *args, PyObject *kwds) {
static char *kwlist[] = {"buffer", "nbytes", "flags", 0};
int recvlen = 0, flags = 0;
ssize_t readlen;
char *buf;
int buflen;
if (!PyArg_ParseTupleAndKeywords(args, kwds, "w#|ii:recv_into", kwlist,
&buf, &buflen, &recvlen, &flags))
return NULL;
assert(buf != 0 && buflen > 0);
if (recvlen < 0) {
PyErr_SetString(PyExc_ValueError,
"negative buffersize in recv_into");
return NULL;
}
if (recvlen == 0) {
recvlen = buflen;
}
if (buflen < recvlen) {
PyErr_SetString(PyExc_ValueError,
"buffer too small for requested bytes");
return NULL;
}
readlen = sock_recv_guts(s, buf, recvlen, flags);
if (readlen < 0) {
return NULL;
}
return PyInt_FromSsize_t(readlen);
}
PyDoc_STRVAR(recv_into_doc,
"recv_into(buffer, [nbytes[, flags]]) -> nbytes_read\n\
\n\
A version of recv() that stores its data into a buffer rather than creating \n\
a new string. Receive up to buffersize bytes from the socket. If buffersize \n\
is not specified (or 0), receive up to the size available in the given buffer.\n\
\n\
See recv() for documentation about the flags.");
static ssize_t
sock_recvfrom_guts(PySocketSockObject *s, char* cbuf, int len, int flags,
PyObject** addr) {
sock_addr_t addrbuf;
int timeout;
ssize_t n = -1;
socklen_t addrlen;
*addr = NULL;
if (!getsockaddrlen(s, &addrlen))
return -1;
if (!IS_SELECTABLE(s)) {
select_error();
return -1;
}
Py_BEGIN_ALLOW_THREADS
memset(&addrbuf, 0, addrlen);
timeout = internal_select(s, 0);
if (!timeout) {
#if !defined(MS_WINDOWS)
#if defined(PYOS_OS2) && !defined(PYCC_GCC)
n = recvfrom(s->sock_fd, cbuf, len, flags,
SAS2SA(&addrbuf), &addrlen);
#else
n = recvfrom(s->sock_fd, cbuf, len, flags,
(void *) &addrbuf, &addrlen);
#endif
#else
n = recvfrom(s->sock_fd, cbuf, len, flags,
SAS2SA(&addrbuf), &addrlen);
#endif
}
Py_END_ALLOW_THREADS
if (timeout == 1) {
PyErr_SetString(socket_timeout, "timed out");
return -1;
}
if (n < 0) {
s->errorhandler();
return -1;
}
if (!(*addr = makesockaddr(s->sock_fd, SAS2SA(&addrbuf),
addrlen, s->sock_proto)))
return -1;
return n;
}
static PyObject *
sock_recvfrom(PySocketSockObject *s, PyObject *args) {
PyObject *buf = NULL;
PyObject *addr = NULL;
PyObject *ret = NULL;
int recvlen, flags = 0;
ssize_t outlen;
if (!PyArg_ParseTuple(args, "i|i:recvfrom", &recvlen, &flags))
return NULL;
if (recvlen < 0) {
PyErr_SetString(PyExc_ValueError,
"negative buffersize in recvfrom");
return NULL;
}
buf = PyString_FromStringAndSize((char *) 0, recvlen);
if (buf == NULL)
return NULL;
outlen = sock_recvfrom_guts(s, PyString_AS_STRING(buf),
recvlen, flags, &addr);
if (outlen < 0) {
goto finally;
}
if (outlen != recvlen) {
if (_PyString_Resize(&buf, outlen) < 0)
goto finally;
}
ret = PyTuple_Pack(2, buf, addr);
finally:
Py_XDECREF(buf);
Py_XDECREF(addr);
return ret;
}
PyDoc_STRVAR(recvfrom_doc,
"recvfrom(buffersize[, flags]) -> (data, address info)\n\
\n\
Like recv(buffersize, flags) but also return the sender's address info.");
static PyObject *
sock_recvfrom_into(PySocketSockObject *s, PyObject *args, PyObject* kwds) {
static char *kwlist[] = {"buffer", "nbytes", "flags", 0};
int recvlen = 0, flags = 0;
ssize_t readlen;
char *buf;
int buflen;
PyObject *addr = NULL;
if (!PyArg_ParseTupleAndKeywords(args, kwds, "w#|ii:recvfrom_into",
kwlist, &buf, &buflen,
&recvlen, &flags))
return NULL;
assert(buf != 0 && buflen > 0);
if (recvlen < 0) {
PyErr_SetString(PyExc_ValueError,
"negative buffersize in recvfrom_into");
return NULL;
}
if (recvlen == 0) {
recvlen = buflen;
}
readlen = sock_recvfrom_guts(s, buf, recvlen, flags, &addr);
if (readlen < 0) {
Py_XDECREF(addr);
return NULL;
}
return Py_BuildValue("lN", readlen, addr);
}
PyDoc_STRVAR(recvfrom_into_doc,
"recvfrom_into(buffer[, nbytes[, flags]]) -> (nbytes, address info)\n\
\n\
Like recv_into(buffer[, nbytes[, flags]]) but also return the sender's address info.");
static PyObject *
sock_send(PySocketSockObject *s, PyObject *args) {
char *buf;
int len, n = -1, flags = 0, timeout;
Py_buffer pbuf;
if (!PyArg_ParseTuple(args, "s*|i:send", &pbuf, &flags))
return NULL;
if (!IS_SELECTABLE(s)) {
PyBuffer_Release(&pbuf);
return select_error();
}
buf = pbuf.buf;
len = pbuf.len;
Py_BEGIN_ALLOW_THREADS
timeout = internal_select(s, 1);
if (!timeout)
#if defined(__VMS)
n = sendsegmented(s->sock_fd, buf, len, flags);
#else
n = send(s->sock_fd, buf, len, flags);
#endif
Py_END_ALLOW_THREADS
PyBuffer_Release(&pbuf);
if (timeout == 1) {
PyErr_SetString(socket_timeout, "timed out");
return NULL;
}
if (n < 0)
return s->errorhandler();
return PyInt_FromLong((long)n);
}
PyDoc_STRVAR(send_doc,
"send(data[, flags]) -> count\n\
\n\
Send a data string to the socket. For the optional flags\n\
argument, see the Unix manual. Return the number of bytes\n\
sent; this may be less than len(data) if the network is busy.");
static PyObject *
sock_sendall(PySocketSockObject *s, PyObject *args) {
char *buf;
int len, n = -1, flags = 0, timeout;
Py_buffer pbuf;
if (!PyArg_ParseTuple(args, "s*|i:sendall", &pbuf, &flags))
return NULL;
buf = pbuf.buf;
len = pbuf.len;
if (!IS_SELECTABLE(s)) {
PyBuffer_Release(&pbuf);
return select_error();
}
Py_BEGIN_ALLOW_THREADS
do {
timeout = internal_select(s, 1);
n = -1;
if (timeout)
break;
#if defined(__VMS)
n = sendsegmented(s->sock_fd, buf, len, flags);
#else
n = send(s->sock_fd, buf, len, flags);
#endif
if (n < 0)
break;
buf += n;
len -= n;
} while (len > 0);
Py_END_ALLOW_THREADS
PyBuffer_Release(&pbuf);
if (timeout == 1) {
PyErr_SetString(socket_timeout, "timed out");
return NULL;
}
if (n < 0)
return s->errorhandler();
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(sendall_doc,
"sendall(data[, flags])\n\
\n\
Send a data string to the socket. For the optional flags\n\
argument, see the Unix manual. This calls send() repeatedly\n\
until all data is sent. If an error occurs, it's impossible\n\
to tell how much data has been sent.");
static PyObject *
sock_sendto(PySocketSockObject *s, PyObject *args) {
Py_buffer pbuf;
PyObject *addro;
char *buf;
Py_ssize_t len;
sock_addr_t addrbuf;
int addrlen, n = -1, flags, timeout;
flags = 0;
if (!PyArg_ParseTuple(args, "s*O:sendto", &pbuf, &addro)) {
PyErr_Clear();
if (!PyArg_ParseTuple(args, "s*iO:sendto",
&pbuf, &flags, &addro))
return NULL;
}
buf = pbuf.buf;
len = pbuf.len;
if (!IS_SELECTABLE(s)) {
PyBuffer_Release(&pbuf);
return select_error();
}
if (!getsockaddrarg(s, addro, SAS2SA(&addrbuf), &addrlen)) {
PyBuffer_Release(&pbuf);
return NULL;
}
Py_BEGIN_ALLOW_THREADS
timeout = internal_select(s, 1);
if (!timeout)
n = sendto(s->sock_fd, buf, len, flags, SAS2SA(&addrbuf), addrlen);
Py_END_ALLOW_THREADS
PyBuffer_Release(&pbuf);
if (timeout == 1) {
PyErr_SetString(socket_timeout, "timed out");
return NULL;
}
if (n < 0)
return s->errorhandler();
return PyInt_FromLong((long)n);
}
PyDoc_STRVAR(sendto_doc,
"sendto(data[, flags], address) -> count\n\
\n\
Like send(data, flags) but allows specifying the destination address.\n\
For IP sockets, the address is a pair (hostaddr, port).");
static PyObject *
sock_shutdown(PySocketSockObject *s, PyObject *arg) {
int how;
int res;
how = PyInt_AsLong(arg);
if (how == -1 && PyErr_Occurred())
return NULL;
Py_BEGIN_ALLOW_THREADS
res = shutdown(s->sock_fd, how);
Py_END_ALLOW_THREADS
if (res < 0)
return s->errorhandler();
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(shutdown_doc,
"shutdown(flag)\n\
\n\
Shut down the reading side of the socket (flag == SHUT_RD), the writing side\n\
of the socket (flag == SHUT_WR), or both ends (flag == SHUT_RDWR).");
#if defined(MS_WINDOWS) && defined(SIO_RCVALL)
static PyObject*
sock_ioctl(PySocketSockObject *s, PyObject *arg) {
unsigned long cmd = SIO_RCVALL;
unsigned int option = RCVALL_ON;
DWORD recv;
if (!PyArg_ParseTuple(arg, "kI:ioctl", &cmd, &option))
return NULL;
if (WSAIoctl(s->sock_fd, cmd, &option, sizeof(option),
NULL, 0, &recv, NULL, NULL) == SOCKET_ERROR) {
return set_error();
}
return PyLong_FromUnsignedLong(recv);
}
PyDoc_STRVAR(sock_ioctl_doc,
"ioctl(cmd, option) -> long\n\
\n\
Control the socket with WSAIoctl syscall. Currently only socket.SIO_RCVALL\n\
is supported as control. Options must be one of the socket.RCVALL_*\n\
constants.");
#endif
static PyMethodDef sock_methods[] = {
{
"accept", (PyCFunction)sock_accept, METH_NOARGS,
accept_doc
},
{
"bind", (PyCFunction)sock_bind, METH_O,
bind_doc
},
{
"close", (PyCFunction)sock_close, METH_NOARGS,
close_doc
},
{
"connect", (PyCFunction)sock_connect, METH_O,
connect_doc
},
{
"connect_ex", (PyCFunction)sock_connect_ex, METH_O,
connect_ex_doc
},
#if !defined(NO_DUP)
{
"dup", (PyCFunction)sock_dup, METH_NOARGS,
dup_doc
},
#endif
{
"fileno", (PyCFunction)sock_fileno, METH_NOARGS,
fileno_doc
},
#if defined(HAVE_GETPEERNAME)
{
"getpeername", (PyCFunction)sock_getpeername,
METH_NOARGS, getpeername_doc
},
#endif
{
"getsockname", (PyCFunction)sock_getsockname,
METH_NOARGS, getsockname_doc
},
{
"getsockopt", (PyCFunction)sock_getsockopt, METH_VARARGS,
getsockopt_doc
},
#if defined(MS_WINDOWS) && defined(SIO_RCVALL)
{
"ioctl", (PyCFunction)sock_ioctl, METH_VARARGS,
sock_ioctl_doc
},
#endif
{
"listen", (PyCFunction)sock_listen, METH_O,
listen_doc
},
#if !defined(NO_DUP)
{
"makefile", (PyCFunction)sock_makefile, METH_VARARGS,
makefile_doc
},
#endif
{
"recv", (PyCFunction)sock_recv, METH_VARARGS,
recv_doc
},
{
"recv_into", (PyCFunction)sock_recv_into, METH_VARARGS | METH_KEYWORDS,
recv_into_doc
},
{
"recvfrom", (PyCFunction)sock_recvfrom, METH_VARARGS,
recvfrom_doc
},
{
"recvfrom_into", (PyCFunction)sock_recvfrom_into, METH_VARARGS | METH_KEYWORDS,
recvfrom_into_doc
},
{
"send", (PyCFunction)sock_send, METH_VARARGS,
send_doc
},
{
"sendall", (PyCFunction)sock_sendall, METH_VARARGS,
sendall_doc
},
{
"sendto", (PyCFunction)sock_sendto, METH_VARARGS,
sendto_doc
},
{
"setblocking", (PyCFunction)sock_setblocking, METH_O,
setblocking_doc
},
{
"settimeout", (PyCFunction)sock_settimeout, METH_O,
settimeout_doc
},
{
"gettimeout", (PyCFunction)sock_gettimeout, METH_NOARGS,
gettimeout_doc
},
{
"setsockopt", (PyCFunction)sock_setsockopt, METH_VARARGS,
setsockopt_doc
},
{
"shutdown", (PyCFunction)sock_shutdown, METH_O,
shutdown_doc
},
#if defined(RISCOS)
{
"sleeptaskw", (PyCFunction)sock_sleeptaskw, METH_O,
sleeptaskw_doc
},
#endif
{NULL, NULL}
};
static PyMemberDef sock_memberlist[] = {
{"family", T_INT, offsetof(PySocketSockObject, sock_family), READONLY, "the socket family"},
{"type", T_INT, offsetof(PySocketSockObject, sock_type), READONLY, "the socket type"},
{"proto", T_INT, offsetof(PySocketSockObject, sock_proto), READONLY, "the socket protocol"},
{"timeout", T_DOUBLE, offsetof(PySocketSockObject, sock_timeout), READONLY, "the socket timeout"},
{0},
};
static void
sock_dealloc(PySocketSockObject *s) {
if (s->sock_fd != -1)
(void) SOCKETCLOSE(s->sock_fd);
Py_TYPE(s)->tp_free((PyObject *)s);
}
static PyObject *
sock_repr(PySocketSockObject *s) {
char buf[512];
#if SIZEOF_SOCKET_T > SIZEOF_LONG
if (s->sock_fd > LONG_MAX) {
PyErr_SetString(PyExc_OverflowError,
"no printf formatter to display "
"the socket descriptor in decimal");
return NULL;
}
#endif
PyOS_snprintf(
buf, sizeof(buf),
"<socket object, fd=%ld, family=%d, type=%d, protocol=%d>",
(long)s->sock_fd, s->sock_family,
s->sock_type,
s->sock_proto);
return PyString_FromString(buf);
}
static PyObject *
sock_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
PyObject *new;
new = type->tp_alloc(type, 0);
if (new != NULL) {
((PySocketSockObject *)new)->sock_fd = -1;
((PySocketSockObject *)new)->sock_timeout = -1.0;
((PySocketSockObject *)new)->errorhandler = &set_error;
}
return new;
}
static int
sock_initobj(PyObject *self, PyObject *args, PyObject *kwds) {
PySocketSockObject *s = (PySocketSockObject *)self;
SOCKET_T fd;
int family = AF_INET, type = SOCK_STREAM, proto = 0;
static char *keywords[] = {"family", "type", "proto", 0};
if (!PyArg_ParseTupleAndKeywords(args, kwds,
"|iii:socket", keywords,
&family, &type, &proto))
return -1;
Py_BEGIN_ALLOW_THREADS
fd = socket(family, type, proto);
Py_END_ALLOW_THREADS
#if defined(MS_WINDOWS)
if (fd == INVALID_SOCKET)
#else
if (fd < 0)
#endif
{
set_error();
return -1;
}
init_sockobject(s, fd, family, type, proto);
return 0;
}
static PyTypeObject sock_type = {
PyVarObject_HEAD_INIT(0, 0)
"_socket.socket",
sizeof(PySocketSockObject),
0,
(destructor)sock_dealloc,
0,
0,
0,
0,
(reprfunc)sock_repr,
0,
0,
0,
0,
0,
0,
PyObject_GenericGetAttr,
0,
0,
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
sock_doc,
0,
0,
0,
0,
0,
0,
sock_methods,
sock_memberlist,
0,
0,
0,
0,
0,
0,
sock_initobj,
PyType_GenericAlloc,
sock_new,
PyObject_Del,
};
static PyObject *
socket_gethostname(PyObject *self, PyObject *unused) {
char buf[1024];
int res;
Py_BEGIN_ALLOW_THREADS
res = gethostname(buf, (int) sizeof buf - 1);
Py_END_ALLOW_THREADS
if (res < 0)
return set_error();
buf[sizeof buf - 1] = '\0';
return PyString_FromString(buf);
}
PyDoc_STRVAR(gethostname_doc,
"gethostname() -> string\n\
\n\
Return the current host name.");
static PyObject *
socket_gethostbyname(PyObject *self, PyObject *args) {
char *name;
sock_addr_t addrbuf;
if (!PyArg_ParseTuple(args, "s:gethostbyname", &name))
return NULL;
if (setipaddr(name, SAS2SA(&addrbuf), sizeof(addrbuf), AF_INET) < 0)
return NULL;
return makeipaddr(SAS2SA(&addrbuf), sizeof(struct sockaddr_in));
}
PyDoc_STRVAR(gethostbyname_doc,
"gethostbyname(host) -> address\n\
\n\
Return the IP address (a string of the form '255.255.255.255') for a host.");
static PyObject *
gethost_common(struct hostent *h, struct sockaddr *addr, int alen, int af) {
char **pch;
PyObject *rtn_tuple = (PyObject *)NULL;
PyObject *name_list = (PyObject *)NULL;
PyObject *addr_list = (PyObject *)NULL;
PyObject *tmp;
if (h == NULL) {
#if !defined(RISCOS)
set_herror(h_errno);
#else
PyErr_SetString(socket_error, "host not found");
#endif
return NULL;
}
if (h->h_addrtype != af) {
PyErr_SetString(socket_error,
(char *)strerror(EAFNOSUPPORT));
return NULL;
}
switch (af) {
case AF_INET:
if (alen < sizeof(struct sockaddr_in))
return NULL;
break;
#if defined(ENABLE_IPV6)
case AF_INET6:
if (alen < sizeof(struct sockaddr_in6))
return NULL;
break;
#endif
}
if ((name_list = PyList_New(0)) == NULL)
goto err;
if ((addr_list = PyList_New(0)) == NULL)
goto err;
if (h->h_aliases) {
for (pch = h->h_aliases; *pch != NULL; pch++) {
int status;
tmp = PyString_FromString(*pch);
if (tmp == NULL)
goto err;
status = PyList_Append(name_list, tmp);
Py_DECREF(tmp);
if (status)
goto err;
}
}
for (pch = h->h_addr_list; *pch != NULL; pch++) {
int status;
switch (af) {
case AF_INET: {
struct sockaddr_in sin;
memset(&sin, 0, sizeof(sin));
sin.sin_family = af;
#if defined(HAVE_SOCKADDR_SA_LEN)
sin.sin_len = sizeof(sin);
#endif
memcpy(&sin.sin_addr, *pch, sizeof(sin.sin_addr));
tmp = makeipaddr((struct sockaddr *)&sin, sizeof(sin));
if (pch == h->h_addr_list && alen >= sizeof(sin))
memcpy((char *) addr, &sin, sizeof(sin));
break;
}
#if defined(ENABLE_IPV6)
case AF_INET6: {
struct sockaddr_in6 sin6;
memset(&sin6, 0, sizeof(sin6));
sin6.sin6_family = af;
#if defined(HAVE_SOCKADDR_SA_LEN)
sin6.sin6_len = sizeof(sin6);
#endif
memcpy(&sin6.sin6_addr, *pch, sizeof(sin6.sin6_addr));
tmp = makeipaddr((struct sockaddr *)&sin6,
sizeof(sin6));
if (pch == h->h_addr_list && alen >= sizeof(sin6))
memcpy((char *) addr, &sin6, sizeof(sin6));
break;
}
#endif
default:
PyErr_SetString(socket_error,
"unsupported address family");
return NULL;
}
if (tmp == NULL)
goto err;
status = PyList_Append(addr_list, tmp);
Py_DECREF(tmp);
if (status)
goto err;
}
rtn_tuple = Py_BuildValue("sOO", h->h_name, name_list, addr_list);
err:
Py_XDECREF(name_list);
Py_XDECREF(addr_list);
return rtn_tuple;
}
static PyObject *
socket_gethostbyname_ex(PyObject *self, PyObject *args) {
char *name;
struct hostent *h;
#if defined(ENABLE_IPV6)
struct sockaddr_storage addr;
#else
struct sockaddr_in addr;
#endif
struct sockaddr *sa;
PyObject *ret;
#if defined(HAVE_GETHOSTBYNAME_R)
struct hostent hp_allocated;
#if defined(HAVE_GETHOSTBYNAME_R_3_ARG)
struct hostent_data data;
#else
char buf[16384];
int buf_len = (sizeof buf) - 1;
int errnop;
#endif
#if defined(HAVE_GETHOSTBYNAME_R_3_ARG) || defined(HAVE_GETHOSTBYNAME_R_6_ARG)
int result;
#endif
#endif
if (!PyArg_ParseTuple(args, "s:gethostbyname_ex", &name))
return NULL;
if (setipaddr(name, (struct sockaddr *)&addr, sizeof(addr), AF_INET) < 0)
return NULL;
Py_BEGIN_ALLOW_THREADS
#if defined(HAVE_GETHOSTBYNAME_R)
#if defined(HAVE_GETHOSTBYNAME_R_6_ARG)
result = gethostbyname_r(name, &hp_allocated, buf, buf_len,
&h, &errnop);
#elif defined(HAVE_GETHOSTBYNAME_R_5_ARG)
h = gethostbyname_r(name, &hp_allocated, buf, buf_len, &errnop);
#else
memset((void *) &data, '\0', sizeof(data));
result = gethostbyname_r(name, &hp_allocated, &data);
h = (result != 0) ? NULL : &hp_allocated;
#endif
#else
#if defined(USE_GETHOSTBYNAME_LOCK)
PyThread_acquire_lock(netdb_lock, 1);
#endif
h = gethostbyname(name);
#endif
Py_END_ALLOW_THREADS
sa = (struct sockaddr*)&addr;
ret = gethost_common(h, (struct sockaddr *)&addr, sizeof(addr),
sa->sa_family);
#if defined(USE_GETHOSTBYNAME_LOCK)
PyThread_release_lock(netdb_lock);
#endif
return ret;
}
PyDoc_STRVAR(ghbn_ex_doc,
"gethostbyname_ex(host) -> (name, aliaslist, addresslist)\n\
\n\
Return the true host name, a list of aliases, and a list of IP addresses,\n\
for a host. The host argument is a string giving a host name or IP number.");
static PyObject *
socket_gethostbyaddr(PyObject *self, PyObject *args) {
#if defined(ENABLE_IPV6)
struct sockaddr_storage addr;
#else
struct sockaddr_in addr;
#endif
struct sockaddr *sa = (struct sockaddr *)&addr;
char *ip_num;
struct hostent *h;
PyObject *ret;
#if defined(HAVE_GETHOSTBYNAME_R)
struct hostent hp_allocated;
#if defined(HAVE_GETHOSTBYNAME_R_3_ARG)
struct hostent_data data;
#else
char buf[16384];
int buf_len = (sizeof buf) - 1;
int errnop;
#endif
#if defined(HAVE_GETHOSTBYNAME_R_3_ARG) || defined(HAVE_GETHOSTBYNAME_R_6_ARG)
int result;
#endif
#endif
char *ap;
int al;
int af;
if (!PyArg_ParseTuple(args, "s:gethostbyaddr", &ip_num))
return NULL;
af = AF_UNSPEC;
if (setipaddr(ip_num, sa, sizeof(addr), af) < 0)
return NULL;
af = sa->sa_family;
ap = NULL;
al = 0;
switch (af) {
case AF_INET:
ap = (char *)&((struct sockaddr_in *)sa)->sin_addr;
al = sizeof(((struct sockaddr_in *)sa)->sin_addr);
break;
#if defined(ENABLE_IPV6)
case AF_INET6:
ap = (char *)&((struct sockaddr_in6 *)sa)->sin6_addr;
al = sizeof(((struct sockaddr_in6 *)sa)->sin6_addr);
break;
#endif
default:
PyErr_SetString(socket_error, "unsupported address family");
return NULL;
}
Py_BEGIN_ALLOW_THREADS
#if defined(HAVE_GETHOSTBYNAME_R)
#if defined(HAVE_GETHOSTBYNAME_R_6_ARG)
result = gethostbyaddr_r(ap, al, af,
&hp_allocated, buf, buf_len,
&h, &errnop);
#elif defined(HAVE_GETHOSTBYNAME_R_5_ARG)
h = gethostbyaddr_r(ap, al, af,
&hp_allocated, buf, buf_len, &errnop);
#else
memset((void *) &data, '\0', sizeof(data));
result = gethostbyaddr_r(ap, al, af, &hp_allocated, &data);
h = (result != 0) ? NULL : &hp_allocated;
#endif
#else
#if defined(USE_GETHOSTBYNAME_LOCK)
PyThread_acquire_lock(netdb_lock, 1);
#endif
h = gethostbyaddr(ap, al, af);
#endif
Py_END_ALLOW_THREADS
ret = gethost_common(h, (struct sockaddr *)&addr, sizeof(addr), af);
#if defined(USE_GETHOSTBYNAME_LOCK)
PyThread_release_lock(netdb_lock);
#endif
return ret;
}
PyDoc_STRVAR(gethostbyaddr_doc,
"gethostbyaddr(host) -> (name, aliaslist, addresslist)\n\
\n\
Return the true host name, a list of aliases, and a list of IP addresses,\n\
for a host. The host argument is a string giving a host name or IP number.");
static PyObject *
socket_getservbyname(PyObject *self, PyObject *args) {
char *name, *proto=NULL;
struct servent *sp;
if (!PyArg_ParseTuple(args, "s|s:getservbyname", &name, &proto))
return NULL;
Py_BEGIN_ALLOW_THREADS
sp = getservbyname(name, proto);
Py_END_ALLOW_THREADS
if (sp == NULL) {
PyErr_SetString(socket_error, "service/proto not found");
return NULL;
}
return PyInt_FromLong((long) ntohs(sp->s_port));
}
PyDoc_STRVAR(getservbyname_doc,
"getservbyname(servicename[, protocolname]) -> integer\n\
\n\
Return a port number from a service name and protocol name.\n\
The optional protocol name, if given, should be 'tcp' or 'udp',\n\
otherwise any protocol will match.");
static PyObject *
socket_getservbyport(PyObject *self, PyObject *args) {
unsigned short port;
char *proto=NULL;
struct servent *sp;
if (!PyArg_ParseTuple(args, "H|s:getservbyport", &port, &proto))
return NULL;
Py_BEGIN_ALLOW_THREADS
sp = getservbyport(htons(port), proto);
Py_END_ALLOW_THREADS
if (sp == NULL) {
PyErr_SetString(socket_error, "port/proto not found");
return NULL;
}
return PyString_FromString(sp->s_name);
}
PyDoc_STRVAR(getservbyport_doc,
"getservbyport(port[, protocolname]) -> string\n\
\n\
Return the service name from a port number and protocol name.\n\
The optional protocol name, if given, should be 'tcp' or 'udp',\n\
otherwise any protocol will match.");
static PyObject *
socket_getprotobyname(PyObject *self, PyObject *args) {
char *name;
struct protoent *sp;
#if defined(__BEOS__)
PyErr_SetString(socket_error, "getprotobyname not supported");
return NULL;
#else
if (!PyArg_ParseTuple(args, "s:getprotobyname", &name))
return NULL;
Py_BEGIN_ALLOW_THREADS
sp = getprotobyname(name);
Py_END_ALLOW_THREADS
if (sp == NULL) {
PyErr_SetString(socket_error, "protocol not found");
return NULL;
}
return PyInt_FromLong((long) sp->p_proto);
#endif
}
PyDoc_STRVAR(getprotobyname_doc,
"getprotobyname(name) -> integer\n\
\n\
Return the protocol number for the named protocol. (Rarely used.)");
#if defined(HAVE_SOCKETPAIR)
static PyObject *
socket_socketpair(PyObject *self, PyObject *args) {
PySocketSockObject *s0 = NULL, *s1 = NULL;
SOCKET_T sv[2];
int family, type = SOCK_STREAM, proto = 0;
PyObject *res = NULL;
#if defined(AF_UNIX)
family = AF_UNIX;
#else
family = AF_INET;
#endif
if (!PyArg_ParseTuple(args, "|iii:socketpair",
&family, &type, &proto))
return NULL;
if (socketpair(family, type, proto, sv) < 0)
return set_error();
s0 = new_sockobject(sv[0], family, type, proto);
if (s0 == NULL)
goto finally;
s1 = new_sockobject(sv[1], family, type, proto);
if (s1 == NULL)
goto finally;
res = PyTuple_Pack(2, s0, s1);
finally:
if (res == NULL) {
if (s0 == NULL)
SOCKETCLOSE(sv[0]);
if (s1 == NULL)
SOCKETCLOSE(sv[1]);
}
Py_XDECREF(s0);
Py_XDECREF(s1);
return res;
}
PyDoc_STRVAR(socketpair_doc,
"socketpair([family[, type[, proto]]]) -> (socket object, socket object)\n\
\n\
Create a pair of socket objects from the sockets returned by the platform\n\
socketpair() function.\n\
The arguments are the same as for socket() except the default family is\n\
AF_UNIX if defined on the platform; otherwise, the default is AF_INET.");
#endif
#if !defined(NO_DUP)
static PyObject *
socket_fromfd(PyObject *self, PyObject *args) {
PySocketSockObject *s;
SOCKET_T fd;
int family, type, proto = 0;
if (!PyArg_ParseTuple(args, "iii|i:fromfd",
&fd, &family, &type, &proto))
return NULL;
fd = dup(fd);
if (fd < 0)
return set_error();
s = new_sockobject(fd, family, type, proto);
return (PyObject *) s;
}
PyDoc_STRVAR(fromfd_doc,
"fromfd(fd, family, type[, proto]) -> socket object\n\
\n\
Create a socket object from a duplicate of the given\n\
file descriptor.\n\
The remaining arguments are the same as for socket().");
#endif
static PyObject *
socket_ntohs(PyObject *self, PyObject *args) {
int x1, x2;
if (!PyArg_ParseTuple(args, "i:ntohs", &x1)) {
return NULL;
}
if (x1 < 0) {
PyErr_SetString(PyExc_OverflowError,
"can't convert negative number to unsigned long");
return NULL;
}
x2 = (unsigned int)ntohs((unsigned short)x1);
return PyInt_FromLong(x2);
}
PyDoc_STRVAR(ntohs_doc,
"ntohs(integer) -> integer\n\
\n\
Convert a 16-bit integer from network to host byte order.");
static PyObject *
socket_ntohl(PyObject *self, PyObject *arg) {
unsigned long x;
if (PyInt_Check(arg)) {
x = PyInt_AS_LONG(arg);
if (x == (unsigned long) -1 && PyErr_Occurred())
return NULL;
if ((long)x < 0) {
PyErr_SetString(PyExc_OverflowError,
"can't convert negative number to unsigned long");
return NULL;
}
} else if (PyLong_Check(arg)) {
x = PyLong_AsUnsignedLong(arg);
if (x == (unsigned long) -1 && PyErr_Occurred())
return NULL;
#if SIZEOF_LONG > 4
{
unsigned long y;
y = x & 0xFFFFFFFFUL;
if (y ^ x)
return PyErr_Format(PyExc_OverflowError,
"long int larger than 32 bits");
x = y;
}
#endif
} else
return PyErr_Format(PyExc_TypeError,
"expected int/long, %s found",
Py_TYPE(arg)->tp_name);
if (x == (unsigned long) -1 && PyErr_Occurred())
return NULL;
return PyLong_FromUnsignedLong(ntohl(x));
}
PyDoc_STRVAR(ntohl_doc,
"ntohl(integer) -> integer\n\
\n\
Convert a 32-bit integer from network to host byte order.");
static PyObject *
socket_htons(PyObject *self, PyObject *args) {
int x1, x2;
if (!PyArg_ParseTuple(args, "i:htons", &x1)) {
return NULL;
}
if (x1 < 0) {
PyErr_SetString(PyExc_OverflowError,
"can't convert negative number to unsigned long");
return NULL;
}
x2 = (unsigned int)htons((unsigned short)x1);
return PyInt_FromLong(x2);
}
PyDoc_STRVAR(htons_doc,
"htons(integer) -> integer\n\
\n\
Convert a 16-bit integer from host to network byte order.");
static PyObject *
socket_htonl(PyObject *self, PyObject *arg) {
unsigned long x;
if (PyInt_Check(arg)) {
x = PyInt_AS_LONG(arg);
if (x == (unsigned long) -1 && PyErr_Occurred())
return NULL;
if ((long)x < 0) {
PyErr_SetString(PyExc_OverflowError,
"can't convert negative number to unsigned long");
return NULL;
}
} else if (PyLong_Check(arg)) {
x = PyLong_AsUnsignedLong(arg);
if (x == (unsigned long) -1 && PyErr_Occurred())
return NULL;
#if SIZEOF_LONG > 4
{
unsigned long y;
y = x & 0xFFFFFFFFUL;
if (y ^ x)
return PyErr_Format(PyExc_OverflowError,
"long int larger than 32 bits");
x = y;
}
#endif
} else
return PyErr_Format(PyExc_TypeError,
"expected int/long, %s found",
Py_TYPE(arg)->tp_name);
return PyLong_FromUnsignedLong(htonl((unsigned long)x));
}
PyDoc_STRVAR(htonl_doc,
"htonl(integer) -> integer\n\
\n\
Convert a 32-bit integer from host to network byte order.");
PyDoc_STRVAR(inet_aton_doc,
"inet_aton(string) -> packed 32-bit IP representation\n\
\n\
Convert an IP address in string format (123.45.67.89) to the 32-bit packed\n\
binary format used in low-level network functions.");
static PyObject*
socket_inet_aton(PyObject *self, PyObject *args) {
#if !defined(INADDR_NONE)
#define INADDR_NONE (-1)
#endif
#if defined(HAVE_INET_ATON)
struct in_addr buf;
#endif
#if !defined(HAVE_INET_ATON) || defined(USE_INET_ATON_WEAKLINK)
unsigned long packed_addr;
#endif
char *ip_addr;
if (!PyArg_ParseTuple(args, "s:inet_aton", &ip_addr))
return NULL;
#if defined(HAVE_INET_ATON)
#if defined(USE_INET_ATON_WEAKLINK)
if (inet_aton != NULL) {
#endif
if (inet_aton(ip_addr, &buf))
return PyString_FromStringAndSize((char *)(&buf),
sizeof(buf));
PyErr_SetString(socket_error,
"illegal IP address string passed to inet_aton");
return NULL;
#if defined(USE_INET_ATON_WEAKLINK)
} else {
#endif
#endif
#if !defined(HAVE_INET_ATON) || defined(USE_INET_ATON_WEAKLINK)
if (strcmp(ip_addr, "255.255.255.255") == 0) {
packed_addr = 0xFFFFFFFF;
} else {
packed_addr = inet_addr(ip_addr);
if (packed_addr == INADDR_NONE) {
PyErr_SetString(socket_error,
"illegal IP address string passed to inet_aton");
return NULL;
}
}
return PyString_FromStringAndSize((char *) &packed_addr,
sizeof(packed_addr));
#if defined(USE_INET_ATON_WEAKLINK)
}
#endif
#endif
}
PyDoc_STRVAR(inet_ntoa_doc,
"inet_ntoa(packed_ip) -> ip_address_string\n\
\n\
Convert an IP address from 32-bit packed binary format to string format");
static PyObject*
socket_inet_ntoa(PyObject *self, PyObject *args) {
char *packed_str;
int addr_len;
struct in_addr packed_addr;
if (!PyArg_ParseTuple(args, "s#:inet_ntoa", &packed_str, &addr_len)) {
return NULL;
}
if (addr_len != sizeof(packed_addr)) {
PyErr_SetString(socket_error,
"packed IP wrong length for inet_ntoa");
return NULL;
}
memcpy(&packed_addr, packed_str, addr_len);
return PyString_FromString(inet_ntoa(packed_addr));
}
#if defined(HAVE_INET_PTON)
PyDoc_STRVAR(inet_pton_doc,
"inet_pton(af, ip) -> packed IP address string\n\
\n\
Convert an IP address from string format to a packed string suitable\n\
for use with low-level network functions.");
static PyObject *
socket_inet_pton(PyObject *self, PyObject *args) {
int af;
char* ip;
int retval;
#if defined(ENABLE_IPV6)
char packed[MAX(sizeof(struct in_addr), sizeof(struct in6_addr))];
#else
char packed[sizeof(struct in_addr)];
#endif
if (!PyArg_ParseTuple(args, "is:inet_pton", &af, &ip)) {
return NULL;
}
#if !defined(ENABLE_IPV6) && defined(AF_INET6)
if(af == AF_INET6) {
PyErr_SetString(socket_error,
"can't use AF_INET6, IPv6 is disabled");
return NULL;
}
#endif
retval = inet_pton(af, ip, packed);
if (retval < 0) {
PyErr_SetFromErrno(socket_error);
return NULL;
} else if (retval == 0) {
PyErr_SetString(socket_error,
"illegal IP address string passed to inet_pton");
return NULL;
} else if (af == AF_INET) {
return PyString_FromStringAndSize(packed,
sizeof(struct in_addr));
#if defined(ENABLE_IPV6)
} else if (af == AF_INET6) {
return PyString_FromStringAndSize(packed,
sizeof(struct in6_addr));
#endif
} else {
PyErr_SetString(socket_error, "unknown address family");
return NULL;
}
}
PyDoc_STRVAR(inet_ntop_doc,
"inet_ntop(af, packed_ip) -> string formatted IP address\n\
\n\
Convert a packed IP address of the given family to string format.");
static PyObject *
socket_inet_ntop(PyObject *self, PyObject *args) {
int af;
char* packed;
int len;
const char* retval;
#if defined(ENABLE_IPV6)
char ip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 1];
#else
char ip[INET_ADDRSTRLEN + 1];
#endif
memset((void *) &ip[0], '\0', sizeof(ip));
if (!PyArg_ParseTuple(args, "is#:inet_ntop", &af, &packed, &len)) {
return NULL;
}
if (af == AF_INET) {
if (len != sizeof(struct in_addr)) {
PyErr_SetString(PyExc_ValueError,
"invalid length of packed IP address string");
return NULL;
}
#if defined(ENABLE_IPV6)
} else if (af == AF_INET6) {
if (len != sizeof(struct in6_addr)) {
PyErr_SetString(PyExc_ValueError,
"invalid length of packed IP address string");
return NULL;
}
#endif
} else {
PyErr_Format(PyExc_ValueError,
"unknown address family %d", af);
return NULL;
}
retval = inet_ntop(af, packed, ip, sizeof(ip));
if (!retval) {
PyErr_SetFromErrno(socket_error);
return NULL;
} else {
return PyString_FromString(retval);
}
PyErr_SetString(PyExc_RuntimeError, "invalid handling of inet_ntop");
return NULL;
}
#endif
static PyObject *
socket_getaddrinfo(PyObject *self, PyObject *args) {
struct addrinfo hints, *res;
struct addrinfo *res0 = NULL;
PyObject *hobj = NULL;
PyObject *pobj = (PyObject *)NULL;
char pbuf[30];
char *hptr, *pptr;
int family, socktype, protocol, flags;
int error;
PyObject *all = (PyObject *)NULL;
PyObject *single = (PyObject *)NULL;
PyObject *idna = NULL;
family = socktype = protocol = flags = 0;
family = AF_UNSPEC;
if (!PyArg_ParseTuple(args, "OO|iiii:getaddrinfo",
&hobj, &pobj, &family, &socktype,
&protocol, &flags)) {
return NULL;
}
if (hobj == Py_None) {
hptr = NULL;
} else if (PyUnicode_Check(hobj)) {
idna = PyObject_CallMethod(hobj, "encode", "s", "idna");
if (!idna)
return NULL;
hptr = PyString_AsString(idna);
} else if (PyString_Check(hobj)) {
hptr = PyString_AsString(hobj);
} else {
PyErr_SetString(PyExc_TypeError,
"getaddrinfo() argument 1 must be string or None");
return NULL;
}
if (PyInt_Check(pobj)) {
PyOS_snprintf(pbuf, sizeof(pbuf), "%ld", PyInt_AsLong(pobj));
pptr = pbuf;
} else if (PyString_Check(pobj)) {
pptr = PyString_AsString(pobj);
} else if (pobj == Py_None) {
pptr = (char *)NULL;
} else {
PyErr_SetString(socket_error, "Int or String expected");
goto err;
}
memset(&hints, 0, sizeof(hints));
hints.ai_family = family;
hints.ai_socktype = socktype;
hints.ai_protocol = protocol;
hints.ai_flags = flags;
Py_BEGIN_ALLOW_THREADS
ACQUIRE_GETADDRINFO_LOCK
error = getaddrinfo(hptr, pptr, &hints, &res0);
Py_END_ALLOW_THREADS
RELEASE_GETADDRINFO_LOCK
if (error) {
set_gaierror(error);
goto err;
}
if ((all = PyList_New(0)) == NULL)
goto err;
for (res = res0; res; res = res->ai_next) {
PyObject *addr =
makesockaddr(-1, res->ai_addr, res->ai_addrlen, protocol);
if (addr == NULL)
goto err;
single = Py_BuildValue("iiisO", res->ai_family,
res->ai_socktype, res->ai_protocol,
res->ai_canonname ? res->ai_canonname : "",
addr);
Py_DECREF(addr);
if (single == NULL)
goto err;
if (PyList_Append(all, single))
goto err;
Py_XDECREF(single);
}
Py_XDECREF(idna);
if (res0)
freeaddrinfo(res0);
return all;
err:
Py_XDECREF(single);
Py_XDECREF(all);
Py_XDECREF(idna);
if (res0)
freeaddrinfo(res0);
return (PyObject *)NULL;
}
PyDoc_STRVAR(getaddrinfo_doc,
"getaddrinfo(host, port [, family, socktype, proto, flags])\n\
-> list of (family, socktype, proto, canonname, sockaddr)\n\
\n\
Resolve host and port into addrinfo struct.");
static PyObject *
socket_getnameinfo(PyObject *self, PyObject *args) {
PyObject *sa = (PyObject *)NULL;
int flags;
char *hostp;
int port, flowinfo, scope_id;
char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
struct addrinfo hints, *res = NULL;
int error;
PyObject *ret = (PyObject *)NULL;
flags = flowinfo = scope_id = 0;
if (!PyArg_ParseTuple(args, "Oi:getnameinfo", &sa, &flags))
return NULL;
if (!PyArg_ParseTuple(sa, "si|ii",
&hostp, &port, &flowinfo, &scope_id))
return NULL;
PyOS_snprintf(pbuf, sizeof(pbuf), "%d", port);
memset(&hints, 0, sizeof(hints));
hints.ai_family = AF_UNSPEC;
hints.ai_socktype = SOCK_DGRAM;
Py_BEGIN_ALLOW_THREADS
ACQUIRE_GETADDRINFO_LOCK
error = getaddrinfo(hostp, pbuf, &hints, &res);
Py_END_ALLOW_THREADS
RELEASE_GETADDRINFO_LOCK
if (error) {
set_gaierror(error);
goto fail;
}
if (res->ai_next) {
PyErr_SetString(socket_error,
"sockaddr resolved to multiple addresses");
goto fail;
}
switch (res->ai_family) {
case AF_INET: {
char *t1;
int t2;
if (PyArg_ParseTuple(sa, "si", &t1, &t2) == 0) {
PyErr_SetString(socket_error,
"IPv4 sockaddr must be 2 tuple");
goto fail;
}
break;
}
#if defined(ENABLE_IPV6)
case AF_INET6: {
struct sockaddr_in6 *sin6;
sin6 = (struct sockaddr_in6 *)res->ai_addr;
sin6->sin6_flowinfo = flowinfo;
sin6->sin6_scope_id = scope_id;
break;
}
#endif
}
error = getnameinfo(res->ai_addr, res->ai_addrlen,
hbuf, sizeof(hbuf), pbuf, sizeof(pbuf), flags);
if (error) {
set_gaierror(error);
goto fail;
}
ret = Py_BuildValue("ss", hbuf, pbuf);
fail:
if (res)
freeaddrinfo(res);
return ret;
}
PyDoc_STRVAR(getnameinfo_doc,
"getnameinfo(sockaddr, flags) --> (host, port)\n\
\n\
Get host and port for a sockaddr.");
static PyObject *
socket_getdefaulttimeout(PyObject *self) {
if (defaulttimeout < 0.0) {
Py_INCREF(Py_None);
return Py_None;
} else
return PyFloat_FromDouble(defaulttimeout);
}
PyDoc_STRVAR(getdefaulttimeout_doc,
"getdefaulttimeout() -> timeout\n\
\n\
Returns the default timeout in floating seconds for new socket objects.\n\
A value of None indicates that new socket objects have no timeout.\n\
When the socket module is first imported, the default is None.");
static PyObject *
socket_setdefaulttimeout(PyObject *self, PyObject *arg) {
double timeout;
if (arg == Py_None)
timeout = -1.0;
else {
timeout = PyFloat_AsDouble(arg);
if (timeout < 0.0) {
if (!PyErr_Occurred())
PyErr_SetString(PyExc_ValueError,
"Timeout value out of range");
return NULL;
}
}
defaulttimeout = timeout;
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(setdefaulttimeout_doc,
"setdefaulttimeout(timeout)\n\
\n\
Set the default timeout in floating seconds for new socket objects.\n\
A value of None indicates that new socket objects have no timeout.\n\
When the socket module is first imported, the default is None.");
static PyMethodDef socket_methods[] = {
{
"gethostbyname", socket_gethostbyname,
METH_VARARGS, gethostbyname_doc
},
{
"gethostbyname_ex", socket_gethostbyname_ex,
METH_VARARGS, ghbn_ex_doc
},
{
"gethostbyaddr", socket_gethostbyaddr,
METH_VARARGS, gethostbyaddr_doc
},
{
"gethostname", socket_gethostname,
METH_NOARGS, gethostname_doc
},
{
"getservbyname", socket_getservbyname,
METH_VARARGS, getservbyname_doc
},
{
"getservbyport", socket_getservbyport,
METH_VARARGS, getservbyport_doc
},
{
"getprotobyname", socket_getprotobyname,
METH_VARARGS, getprotobyname_doc
},
#if !defined(NO_DUP)
{
"fromfd", socket_fromfd,
METH_VARARGS, fromfd_doc
},
#endif
#if defined(HAVE_SOCKETPAIR)
{
"socketpair", socket_socketpair,
METH_VARARGS, socketpair_doc
},
#endif
{
"ntohs", socket_ntohs,
METH_VARARGS, ntohs_doc
},
{
"ntohl", socket_ntohl,
METH_O, ntohl_doc
},
{
"htons", socket_htons,
METH_VARARGS, htons_doc
},
{
"htonl", socket_htonl,
METH_O, htonl_doc
},
{
"inet_aton", socket_inet_aton,
METH_VARARGS, inet_aton_doc
},
{
"inet_ntoa", socket_inet_ntoa,
METH_VARARGS, inet_ntoa_doc
},
#if defined(HAVE_INET_PTON)
{
"inet_pton", socket_inet_pton,
METH_VARARGS, inet_pton_doc
},
{
"inet_ntop", socket_inet_ntop,
METH_VARARGS, inet_ntop_doc
},
#endif
{
"getaddrinfo", socket_getaddrinfo,
METH_VARARGS, getaddrinfo_doc
},
{
"getnameinfo", socket_getnameinfo,
METH_VARARGS, getnameinfo_doc
},
{
"getdefaulttimeout", (PyCFunction)socket_getdefaulttimeout,
METH_NOARGS, getdefaulttimeout_doc
},
{
"setdefaulttimeout", socket_setdefaulttimeout,
METH_O, setdefaulttimeout_doc
},
{NULL, NULL}
};
#if defined(RISCOS)
#define OS_INIT_DEFINED
static int
os_init(void) {
_kernel_swi_regs r;
r.r[0] = 0;
_kernel_swi(0x43380, &r, &r);
taskwindow = r.r[0];
return 1;
}
#endif
#if defined(MS_WINDOWS)
#define OS_INIT_DEFINED
static void
os_cleanup(void) {
WSACleanup();
}
static int
os_init(void) {
WSADATA WSAData;
int ret;
char buf[100];
ret = WSAStartup(0x0101, &WSAData);
switch (ret) {
case 0:
Py_AtExit(os_cleanup);
return 1;
case WSASYSNOTREADY:
PyErr_SetString(PyExc_ImportError,
"WSAStartup failed: network not ready");
break;
case WSAVERNOTSUPPORTED:
case WSAEINVAL:
PyErr_SetString(
PyExc_ImportError,
"WSAStartup failed: requested version not supported");
break;
default:
PyOS_snprintf(buf, sizeof(buf),
"WSAStartup failed: error code %d", ret);
PyErr_SetString(PyExc_ImportError, buf);
break;
}
return 0;
}
#endif
#if defined(PYOS_OS2)
#define OS_INIT_DEFINED
static int
os_init(void) {
#if !defined(PYCC_GCC)
char reason[64];
int rc = sock_init();
if (rc == 0) {
return 1;
}
PyOS_snprintf(reason, sizeof(reason),
"OS/2 TCP/IP Error#%d", sock_errno());
PyErr_SetString(PyExc_ImportError, reason);
return 0;
#else
return 1;
#endif
}
#endif
#if !defined(OS_INIT_DEFINED)
static int
os_init(void) {
return 1;
}
#endif
static
PySocketModule_APIObject PySocketModuleAPI = {
&sock_type,
NULL
};
PyDoc_STRVAR(socket_doc,
"Implementation module for socket operations.\n\
\n\
See the socket module for documentation.");
PyMODINIT_FUNC
init_socket(void) {
PyObject *m, *has_ipv6;
if (!os_init())
return;
Py_TYPE(&sock_type) = &PyType_Type;
m = Py_InitModule3(PySocket_MODULE_NAME,
socket_methods,
socket_doc);
if (m == NULL)
return;
socket_error = PyErr_NewException("socket.error",
PyExc_IOError, NULL);
if (socket_error == NULL)
return;
PySocketModuleAPI.error = socket_error;
Py_INCREF(socket_error);
PyModule_AddObject(m, "error", socket_error);
socket_herror = PyErr_NewException("socket.herror",
socket_error, NULL);
if (socket_herror == NULL)
return;
Py_INCREF(socket_herror);
PyModule_AddObject(m, "herror", socket_herror);
socket_gaierror = PyErr_NewException("socket.gaierror", socket_error,
NULL);
if (socket_gaierror == NULL)
return;
Py_INCREF(socket_gaierror);
PyModule_AddObject(m, "gaierror", socket_gaierror);
socket_timeout = PyErr_NewException("socket.timeout",
socket_error, NULL);
if (socket_timeout == NULL)
return;
Py_INCREF(socket_timeout);
PyModule_AddObject(m, "timeout", socket_timeout);
Py_INCREF((PyObject *)&sock_type);
if (PyModule_AddObject(m, "SocketType",
(PyObject *)&sock_type) != 0)
return;
Py_INCREF((PyObject *)&sock_type);
if (PyModule_AddObject(m, "socket",
(PyObject *)&sock_type) != 0)
return;
#if defined(ENABLE_IPV6)
has_ipv6 = Py_True;
#else
has_ipv6 = Py_False;
#endif
Py_INCREF(has_ipv6);
PyModule_AddObject(m, "has_ipv6", has_ipv6);
if (PyModule_AddObject(m, PySocket_CAPI_NAME,
PyCObject_FromVoidPtr((void *)&PySocketModuleAPI, NULL)
) != 0)
return;
#if defined(AF_UNSPEC)
PyModule_AddIntConstant(m, "AF_UNSPEC", AF_UNSPEC);
#endif
PyModule_AddIntConstant(m, "AF_INET", AF_INET);
#if defined(AF_INET6)
PyModule_AddIntConstant(m, "AF_INET6", AF_INET6);
#endif
#if defined(AF_UNIX)
PyModule_AddIntConstant(m, "AF_UNIX", AF_UNIX);
#endif
#if defined(AF_AX25)
PyModule_AddIntConstant(m, "AF_AX25", AF_AX25);
#endif
#if defined(AF_IPX)
PyModule_AddIntConstant(m, "AF_IPX", AF_IPX);
#endif
#if defined(AF_APPLETALK)
PyModule_AddIntConstant(m, "AF_APPLETALK", AF_APPLETALK);
#endif
#if defined(AF_NETROM)
PyModule_AddIntConstant(m, "AF_NETROM", AF_NETROM);
#endif
#if defined(AF_BRIDGE)
PyModule_AddIntConstant(m, "AF_BRIDGE", AF_BRIDGE);
#endif
#if defined(AF_ATMPVC)
PyModule_AddIntConstant(m, "AF_ATMPVC", AF_ATMPVC);
#endif
#if defined(AF_AAL5)
PyModule_AddIntConstant(m, "AF_AAL5", AF_AAL5);
#endif
#if defined(AF_X25)
PyModule_AddIntConstant(m, "AF_X25", AF_X25);
#endif
#if defined(AF_INET6)
PyModule_AddIntConstant(m, "AF_INET6", AF_INET6);
#endif
#if defined(AF_ROSE)
PyModule_AddIntConstant(m, "AF_ROSE", AF_ROSE);
#endif
#if defined(AF_DECnet)
PyModule_AddIntConstant(m, "AF_DECnet", AF_DECnet);
#endif
#if defined(AF_NETBEUI)
PyModule_AddIntConstant(m, "AF_NETBEUI", AF_NETBEUI);
#endif
#if defined(AF_SECURITY)
PyModule_AddIntConstant(m, "AF_SECURITY", AF_SECURITY);
#endif
#if defined(AF_KEY)
PyModule_AddIntConstant(m, "AF_KEY", AF_KEY);
#endif
#if defined(AF_NETLINK)
PyModule_AddIntConstant(m, "AF_NETLINK", AF_NETLINK);
PyModule_AddIntConstant(m, "NETLINK_ROUTE", NETLINK_ROUTE);
#if defined(NETLINK_SKIP)
PyModule_AddIntConstant(m, "NETLINK_SKIP", NETLINK_SKIP);
#endif
#if defined(NETLINK_W1)
PyModule_AddIntConstant(m, "NETLINK_W1", NETLINK_W1);
#endif
PyModule_AddIntConstant(m, "NETLINK_USERSOCK", NETLINK_USERSOCK);
PyModule_AddIntConstant(m, "NETLINK_FIREWALL", NETLINK_FIREWALL);
#if defined(NETLINK_TCPDIAG)
PyModule_AddIntConstant(m, "NETLINK_TCPDIAG", NETLINK_TCPDIAG);
#endif
#if defined(NETLINK_NFLOG)
PyModule_AddIntConstant(m, "NETLINK_NFLOG", NETLINK_NFLOG);
#endif
#if defined(NETLINK_XFRM)
PyModule_AddIntConstant(m, "NETLINK_XFRM", NETLINK_XFRM);
#endif
#if defined(NETLINK_ARPD)
PyModule_AddIntConstant(m, "NETLINK_ARPD", NETLINK_ARPD);
#endif
#if defined(NETLINK_ROUTE6)
PyModule_AddIntConstant(m, "NETLINK_ROUTE6", NETLINK_ROUTE6);
#endif
PyModule_AddIntConstant(m, "NETLINK_IP6_FW", NETLINK_IP6_FW);
#if defined(NETLINK_DNRTMSG)
PyModule_AddIntConstant(m, "NETLINK_DNRTMSG", NETLINK_DNRTMSG);
#endif
#if defined(NETLINK_TAPBASE)
PyModule_AddIntConstant(m, "NETLINK_TAPBASE", NETLINK_TAPBASE);
#endif
#endif
#if defined(AF_ROUTE)
PyModule_AddIntConstant(m, "AF_ROUTE", AF_ROUTE);
#endif
#if defined(AF_ASH)
PyModule_AddIntConstant(m, "AF_ASH", AF_ASH);
#endif
#if defined(AF_ECONET)
PyModule_AddIntConstant(m, "AF_ECONET", AF_ECONET);
#endif
#if defined(AF_ATMSVC)
PyModule_AddIntConstant(m, "AF_ATMSVC", AF_ATMSVC);
#endif
#if defined(AF_SNA)
PyModule_AddIntConstant(m, "AF_SNA", AF_SNA);
#endif
#if defined(AF_IRDA)
PyModule_AddIntConstant(m, "AF_IRDA", AF_IRDA);
#endif
#if defined(AF_PPPOX)
PyModule_AddIntConstant(m, "AF_PPPOX", AF_PPPOX);
#endif
#if defined(AF_WANPIPE)
PyModule_AddIntConstant(m, "AF_WANPIPE", AF_WANPIPE);
#endif
#if defined(AF_LLC)
PyModule_AddIntConstant(m, "AF_LLC", AF_LLC);
#endif
#if defined(USE_BLUETOOTH)
PyModule_AddIntConstant(m, "AF_BLUETOOTH", AF_BLUETOOTH);
PyModule_AddIntConstant(m, "BTPROTO_L2CAP", BTPROTO_L2CAP);
PyModule_AddIntConstant(m, "BTPROTO_HCI", BTPROTO_HCI);
PyModule_AddIntConstant(m, "SOL_HCI", SOL_HCI);
PyModule_AddIntConstant(m, "HCI_FILTER", HCI_FILTER);
#if !defined(__FreeBSD__)
PyModule_AddIntConstant(m, "HCI_TIME_STAMP", HCI_TIME_STAMP);
PyModule_AddIntConstant(m, "HCI_DATA_DIR", HCI_DATA_DIR);
PyModule_AddIntConstant(m, "BTPROTO_SCO", BTPROTO_SCO);
#endif
PyModule_AddIntConstant(m, "BTPROTO_RFCOMM", BTPROTO_RFCOMM);
PyModule_AddStringConstant(m, "BDADDR_ANY", "00:00:00:00:00:00");
PyModule_AddStringConstant(m, "BDADDR_LOCAL", "00:00:00:FF:FF:FF");
#endif
#if defined(HAVE_NETPACKET_PACKET_H)
PyModule_AddIntConstant(m, "AF_PACKET", AF_PACKET);
PyModule_AddIntConstant(m, "PF_PACKET", PF_PACKET);
PyModule_AddIntConstant(m, "PACKET_HOST", PACKET_HOST);
PyModule_AddIntConstant(m, "PACKET_BROADCAST", PACKET_BROADCAST);
PyModule_AddIntConstant(m, "PACKET_MULTICAST", PACKET_MULTICAST);
PyModule_AddIntConstant(m, "PACKET_OTHERHOST", PACKET_OTHERHOST);
PyModule_AddIntConstant(m, "PACKET_OUTGOING", PACKET_OUTGOING);
PyModule_AddIntConstant(m, "PACKET_LOOPBACK", PACKET_LOOPBACK);
PyModule_AddIntConstant(m, "PACKET_FASTROUTE", PACKET_FASTROUTE);
#endif
#if defined(HAVE_LINUX_TIPC_H)
PyModule_AddIntConstant(m, "AF_TIPC", AF_TIPC);
PyModule_AddIntConstant(m, "TIPC_ADDR_NAMESEQ", TIPC_ADDR_NAMESEQ);
PyModule_AddIntConstant(m, "TIPC_ADDR_NAME", TIPC_ADDR_NAME);
PyModule_AddIntConstant(m, "TIPC_ADDR_ID", TIPC_ADDR_ID);
PyModule_AddIntConstant(m, "TIPC_ZONE_SCOPE", TIPC_ZONE_SCOPE);
PyModule_AddIntConstant(m, "TIPC_CLUSTER_SCOPE", TIPC_CLUSTER_SCOPE);
PyModule_AddIntConstant(m, "TIPC_NODE_SCOPE", TIPC_NODE_SCOPE);
PyModule_AddIntConstant(m, "SOL_TIPC", SOL_TIPC);
PyModule_AddIntConstant(m, "TIPC_IMPORTANCE", TIPC_IMPORTANCE);
PyModule_AddIntConstant(m, "TIPC_SRC_DROPPABLE", TIPC_SRC_DROPPABLE);
PyModule_AddIntConstant(m, "TIPC_DEST_DROPPABLE",
TIPC_DEST_DROPPABLE);
PyModule_AddIntConstant(m, "TIPC_CONN_TIMEOUT", TIPC_CONN_TIMEOUT);
PyModule_AddIntConstant(m, "TIPC_LOW_IMPORTANCE",
TIPC_LOW_IMPORTANCE);
PyModule_AddIntConstant(m, "TIPC_MEDIUM_IMPORTANCE",
TIPC_MEDIUM_IMPORTANCE);
PyModule_AddIntConstant(m, "TIPC_HIGH_IMPORTANCE",
TIPC_HIGH_IMPORTANCE);
PyModule_AddIntConstant(m, "TIPC_CRITICAL_IMPORTANCE",
TIPC_CRITICAL_IMPORTANCE);
PyModule_AddIntConstant(m, "TIPC_SUB_PORTS", TIPC_SUB_PORTS);
PyModule_AddIntConstant(m, "TIPC_SUB_SERVICE", TIPC_SUB_SERVICE);
#if defined(TIPC_SUB_CANCEL)
PyModule_AddIntConstant(m, "TIPC_SUB_CANCEL", TIPC_SUB_CANCEL);
#endif
PyModule_AddIntConstant(m, "TIPC_WAIT_FOREVER", TIPC_WAIT_FOREVER);
PyModule_AddIntConstant(m, "TIPC_PUBLISHED", TIPC_PUBLISHED);
PyModule_AddIntConstant(m, "TIPC_WITHDRAWN", TIPC_WITHDRAWN);
PyModule_AddIntConstant(m, "TIPC_SUBSCR_TIMEOUT", TIPC_SUBSCR_TIMEOUT);
PyModule_AddIntConstant(m, "TIPC_CFG_SRV", TIPC_CFG_SRV);
PyModule_AddIntConstant(m, "TIPC_TOP_SRV", TIPC_TOP_SRV);
#endif
PyModule_AddIntConstant(m, "SOCK_STREAM", SOCK_STREAM);
PyModule_AddIntConstant(m, "SOCK_DGRAM", SOCK_DGRAM);
#if !defined(__BEOS__)
PyModule_AddIntConstant(m, "SOCK_RAW", SOCK_RAW);
PyModule_AddIntConstant(m, "SOCK_SEQPACKET", SOCK_SEQPACKET);
#if defined(SOCK_RDM)
PyModule_AddIntConstant(m, "SOCK_RDM", SOCK_RDM);
#endif
#endif
#if defined(SO_DEBUG)
PyModule_AddIntConstant(m, "SO_DEBUG", SO_DEBUG);
#endif
#if defined(SO_ACCEPTCONN)
PyModule_AddIntConstant(m, "SO_ACCEPTCONN", SO_ACCEPTCONN);
#endif
#if defined(SO_REUSEADDR)
PyModule_AddIntConstant(m, "SO_REUSEADDR", SO_REUSEADDR);
#endif
#if defined(SO_EXCLUSIVEADDRUSE)
PyModule_AddIntConstant(m, "SO_EXCLUSIVEADDRUSE", SO_EXCLUSIVEADDRUSE);
#endif
#if defined(SO_KEEPALIVE)
PyModule_AddIntConstant(m, "SO_KEEPALIVE", SO_KEEPALIVE);
#endif
#if defined(SO_DONTROUTE)
PyModule_AddIntConstant(m, "SO_DONTROUTE", SO_DONTROUTE);
#endif
#if defined(SO_BROADCAST)
PyModule_AddIntConstant(m, "SO_BROADCAST", SO_BROADCAST);
#endif
#if defined(SO_USELOOPBACK)
PyModule_AddIntConstant(m, "SO_USELOOPBACK", SO_USELOOPBACK);
#endif
#if defined(SO_LINGER)
PyModule_AddIntConstant(m, "SO_LINGER", SO_LINGER);
#endif
#if defined(SO_OOBINLINE)
PyModule_AddIntConstant(m, "SO_OOBINLINE", SO_OOBINLINE);
#endif
#if defined(SO_REUSEPORT)
PyModule_AddIntConstant(m, "SO_REUSEPORT", SO_REUSEPORT);
#endif
#if defined(SO_SNDBUF)
PyModule_AddIntConstant(m, "SO_SNDBUF", SO_SNDBUF);
#endif
#if defined(SO_RCVBUF)
PyModule_AddIntConstant(m, "SO_RCVBUF", SO_RCVBUF);
#endif
#if defined(SO_SNDLOWAT)
PyModule_AddIntConstant(m, "SO_SNDLOWAT", SO_SNDLOWAT);
#endif
#if defined(SO_RCVLOWAT)
PyModule_AddIntConstant(m, "SO_RCVLOWAT", SO_RCVLOWAT);
#endif
#if defined(SO_SNDTIMEO)
PyModule_AddIntConstant(m, "SO_SNDTIMEO", SO_SNDTIMEO);
#endif
#if defined(SO_RCVTIMEO)
PyModule_AddIntConstant(m, "SO_RCVTIMEO", SO_RCVTIMEO);
#endif
#if defined(SO_ERROR)
PyModule_AddIntConstant(m, "SO_ERROR", SO_ERROR);
#endif
#if defined(SO_TYPE)
PyModule_AddIntConstant(m, "SO_TYPE", SO_TYPE);
#endif
#if defined(SOMAXCONN)
PyModule_AddIntConstant(m, "SOMAXCONN", SOMAXCONN);
#else
PyModule_AddIntConstant(m, "SOMAXCONN", 5);
#endif
#if defined(MSG_OOB)
PyModule_AddIntConstant(m, "MSG_OOB", MSG_OOB);
#endif
#if defined(MSG_PEEK)
PyModule_AddIntConstant(m, "MSG_PEEK", MSG_PEEK);
#endif
#if defined(MSG_DONTROUTE)
PyModule_AddIntConstant(m, "MSG_DONTROUTE", MSG_DONTROUTE);
#endif
#if defined(MSG_DONTWAIT)
PyModule_AddIntConstant(m, "MSG_DONTWAIT", MSG_DONTWAIT);
#endif
#if defined(MSG_EOR)
PyModule_AddIntConstant(m, "MSG_EOR", MSG_EOR);
#endif
#if defined(MSG_TRUNC)
PyModule_AddIntConstant(m, "MSG_TRUNC", MSG_TRUNC);
#endif
#if defined(MSG_CTRUNC)
PyModule_AddIntConstant(m, "MSG_CTRUNC", MSG_CTRUNC);
#endif
#if defined(MSG_WAITALL)
PyModule_AddIntConstant(m, "MSG_WAITALL", MSG_WAITALL);
#endif
#if defined(MSG_BTAG)
PyModule_AddIntConstant(m, "MSG_BTAG", MSG_BTAG);
#endif
#if defined(MSG_ETAG)
PyModule_AddIntConstant(m, "MSG_ETAG", MSG_ETAG);
#endif
#if defined(SOL_SOCKET)
PyModule_AddIntConstant(m, "SOL_SOCKET", SOL_SOCKET);
#endif
#if defined(SOL_IP)
PyModule_AddIntConstant(m, "SOL_IP", SOL_IP);
#else
PyModule_AddIntConstant(m, "SOL_IP", 0);
#endif
#if defined(SOL_IPX)
PyModule_AddIntConstant(m, "SOL_IPX", SOL_IPX);
#endif
#if defined(SOL_AX25)
PyModule_AddIntConstant(m, "SOL_AX25", SOL_AX25);
#endif
#if defined(SOL_ATALK)
PyModule_AddIntConstant(m, "SOL_ATALK", SOL_ATALK);
#endif
#if defined(SOL_NETROM)
PyModule_AddIntConstant(m, "SOL_NETROM", SOL_NETROM);
#endif
#if defined(SOL_ROSE)
PyModule_AddIntConstant(m, "SOL_ROSE", SOL_ROSE);
#endif
#if defined(SOL_TCP)
PyModule_AddIntConstant(m, "SOL_TCP", SOL_TCP);
#else
PyModule_AddIntConstant(m, "SOL_TCP", 6);
#endif
#if defined(SOL_UDP)
PyModule_AddIntConstant(m, "SOL_UDP", SOL_UDP);
#else
PyModule_AddIntConstant(m, "SOL_UDP", 17);
#endif
#if defined(IPPROTO_IP)
PyModule_AddIntConstant(m, "IPPROTO_IP", IPPROTO_IP);
#else
PyModule_AddIntConstant(m, "IPPROTO_IP", 0);
#endif
#if defined(IPPROTO_HOPOPTS)
PyModule_AddIntConstant(m, "IPPROTO_HOPOPTS", IPPROTO_HOPOPTS);
#endif
#if defined(IPPROTO_ICMP)
PyModule_AddIntConstant(m, "IPPROTO_ICMP", IPPROTO_ICMP);
#else
PyModule_AddIntConstant(m, "IPPROTO_ICMP", 1);
#endif
#if defined(IPPROTO_IGMP)
PyModule_AddIntConstant(m, "IPPROTO_IGMP", IPPROTO_IGMP);
#endif
#if defined(IPPROTO_GGP)
PyModule_AddIntConstant(m, "IPPROTO_GGP", IPPROTO_GGP);
#endif
#if defined(IPPROTO_IPV4)
PyModule_AddIntConstant(m, "IPPROTO_IPV4", IPPROTO_IPV4);
#endif
#if defined(IPPROTO_IPV6)
PyModule_AddIntConstant(m, "IPPROTO_IPV6", IPPROTO_IPV6);
#endif
#if defined(IPPROTO_IPIP)
PyModule_AddIntConstant(m, "IPPROTO_IPIP", IPPROTO_IPIP);
#endif
#if defined(IPPROTO_TCP)
PyModule_AddIntConstant(m, "IPPROTO_TCP", IPPROTO_TCP);
#else
PyModule_AddIntConstant(m, "IPPROTO_TCP", 6);
#endif
#if defined(IPPROTO_EGP)
PyModule_AddIntConstant(m, "IPPROTO_EGP", IPPROTO_EGP);
#endif
#if defined(IPPROTO_PUP)
PyModule_AddIntConstant(m, "IPPROTO_PUP", IPPROTO_PUP);
#endif
#if defined(IPPROTO_UDP)
PyModule_AddIntConstant(m, "IPPROTO_UDP", IPPROTO_UDP);
#else
PyModule_AddIntConstant(m, "IPPROTO_UDP", 17);
#endif
#if defined(IPPROTO_IDP)
PyModule_AddIntConstant(m, "IPPROTO_IDP", IPPROTO_IDP);
#endif
#if defined(IPPROTO_HELLO)
PyModule_AddIntConstant(m, "IPPROTO_HELLO", IPPROTO_HELLO);
#endif
#if defined(IPPROTO_ND)
PyModule_AddIntConstant(m, "IPPROTO_ND", IPPROTO_ND);
#endif
#if defined(IPPROTO_TP)
PyModule_AddIntConstant(m, "IPPROTO_TP", IPPROTO_TP);
#endif
#if defined(IPPROTO_IPV6)
PyModule_AddIntConstant(m, "IPPROTO_IPV6", IPPROTO_IPV6);
#endif
#if defined(IPPROTO_ROUTING)
PyModule_AddIntConstant(m, "IPPROTO_ROUTING", IPPROTO_ROUTING);
#endif
#if defined(IPPROTO_FRAGMENT)
PyModule_AddIntConstant(m, "IPPROTO_FRAGMENT", IPPROTO_FRAGMENT);
#endif
#if defined(IPPROTO_RSVP)
PyModule_AddIntConstant(m, "IPPROTO_RSVP", IPPROTO_RSVP);
#endif
#if defined(IPPROTO_GRE)
PyModule_AddIntConstant(m, "IPPROTO_GRE", IPPROTO_GRE);
#endif
#if defined(IPPROTO_ESP)
PyModule_AddIntConstant(m, "IPPROTO_ESP", IPPROTO_ESP);
#endif
#if defined(IPPROTO_AH)
PyModule_AddIntConstant(m, "IPPROTO_AH", IPPROTO_AH);
#endif
#if defined(IPPROTO_MOBILE)
PyModule_AddIntConstant(m, "IPPROTO_MOBILE", IPPROTO_MOBILE);
#endif
#if defined(IPPROTO_ICMPV6)
PyModule_AddIntConstant(m, "IPPROTO_ICMPV6", IPPROTO_ICMPV6);
#endif
#if defined(IPPROTO_NONE)
PyModule_AddIntConstant(m, "IPPROTO_NONE", IPPROTO_NONE);
#endif
#if defined(IPPROTO_DSTOPTS)
PyModule_AddIntConstant(m, "IPPROTO_DSTOPTS", IPPROTO_DSTOPTS);
#endif
#if defined(IPPROTO_XTP)
PyModule_AddIntConstant(m, "IPPROTO_XTP", IPPROTO_XTP);
#endif
#if defined(IPPROTO_EON)
PyModule_AddIntConstant(m, "IPPROTO_EON", IPPROTO_EON);
#endif
#if defined(IPPROTO_PIM)
PyModule_AddIntConstant(m, "IPPROTO_PIM", IPPROTO_PIM);
#endif
#if defined(IPPROTO_IPCOMP)
PyModule_AddIntConstant(m, "IPPROTO_IPCOMP", IPPROTO_IPCOMP);
#endif
#if defined(IPPROTO_VRRP)
PyModule_AddIntConstant(m, "IPPROTO_VRRP", IPPROTO_VRRP);
#endif
#if defined(IPPROTO_BIP)
PyModule_AddIntConstant(m, "IPPROTO_BIP", IPPROTO_BIP);
#endif
#if defined(IPPROTO_RAW)
PyModule_AddIntConstant(m, "IPPROTO_RAW", IPPROTO_RAW);
#else
PyModule_AddIntConstant(m, "IPPROTO_RAW", 255);
#endif
#if defined(IPPROTO_MAX)
PyModule_AddIntConstant(m, "IPPROTO_MAX", IPPROTO_MAX);
#endif
#if defined(IPPORT_RESERVED)
PyModule_AddIntConstant(m, "IPPORT_RESERVED", IPPORT_RESERVED);
#else
PyModule_AddIntConstant(m, "IPPORT_RESERVED", 1024);
#endif
#if defined(IPPORT_USERRESERVED)
PyModule_AddIntConstant(m, "IPPORT_USERRESERVED", IPPORT_USERRESERVED);
#else
PyModule_AddIntConstant(m, "IPPORT_USERRESERVED", 5000);
#endif
#if defined(INADDR_ANY)
PyModule_AddIntConstant(m, "INADDR_ANY", INADDR_ANY);
#else
PyModule_AddIntConstant(m, "INADDR_ANY", 0x00000000);
#endif
#if defined(INADDR_BROADCAST)
PyModule_AddIntConstant(m, "INADDR_BROADCAST", INADDR_BROADCAST);
#else
PyModule_AddIntConstant(m, "INADDR_BROADCAST", 0xffffffff);
#endif
#if defined(INADDR_LOOPBACK)
PyModule_AddIntConstant(m, "INADDR_LOOPBACK", INADDR_LOOPBACK);
#else
PyModule_AddIntConstant(m, "INADDR_LOOPBACK", 0x7F000001);
#endif
#if defined(INADDR_UNSPEC_GROUP)
PyModule_AddIntConstant(m, "INADDR_UNSPEC_GROUP", INADDR_UNSPEC_GROUP);
#else
PyModule_AddIntConstant(m, "INADDR_UNSPEC_GROUP", 0xe0000000);
#endif
#if defined(INADDR_ALLHOSTS_GROUP)
PyModule_AddIntConstant(m, "INADDR_ALLHOSTS_GROUP",
INADDR_ALLHOSTS_GROUP);
#else
PyModule_AddIntConstant(m, "INADDR_ALLHOSTS_GROUP", 0xe0000001);
#endif
#if defined(INADDR_MAX_LOCAL_GROUP)
PyModule_AddIntConstant(m, "INADDR_MAX_LOCAL_GROUP",
INADDR_MAX_LOCAL_GROUP);
#else
PyModule_AddIntConstant(m, "INADDR_MAX_LOCAL_GROUP", 0xe00000ff);
#endif
#if defined(INADDR_NONE)
PyModule_AddIntConstant(m, "INADDR_NONE", INADDR_NONE);
#else
PyModule_AddIntConstant(m, "INADDR_NONE", 0xffffffff);
#endif
#if defined(IP_OPTIONS)
PyModule_AddIntConstant(m, "IP_OPTIONS", IP_OPTIONS);
#endif
#if defined(IP_HDRINCL)
PyModule_AddIntConstant(m, "IP_HDRINCL", IP_HDRINCL);
#endif
#if defined(IP_TOS)
PyModule_AddIntConstant(m, "IP_TOS", IP_TOS);
#endif
#if defined(IP_TTL)
PyModule_AddIntConstant(m, "IP_TTL", IP_TTL);
#endif
#if defined(IP_RECVOPTS)
PyModule_AddIntConstant(m, "IP_RECVOPTS", IP_RECVOPTS);
#endif
#if defined(IP_RECVRETOPTS)
PyModule_AddIntConstant(m, "IP_RECVRETOPTS", IP_RECVRETOPTS);
#endif
#if defined(IP_RECVDSTADDR)
PyModule_AddIntConstant(m, "IP_RECVDSTADDR", IP_RECVDSTADDR);
#endif
#if defined(IP_RETOPTS)
PyModule_AddIntConstant(m, "IP_RETOPTS", IP_RETOPTS);
#endif
#if defined(IP_MULTICAST_IF)
PyModule_AddIntConstant(m, "IP_MULTICAST_IF", IP_MULTICAST_IF);
#endif
#if defined(IP_MULTICAST_TTL)
PyModule_AddIntConstant(m, "IP_MULTICAST_TTL", IP_MULTICAST_TTL);
#endif
#if defined(IP_MULTICAST_LOOP)
PyModule_AddIntConstant(m, "IP_MULTICAST_LOOP", IP_MULTICAST_LOOP);
#endif
#if defined(IP_ADD_MEMBERSHIP)
PyModule_AddIntConstant(m, "IP_ADD_MEMBERSHIP", IP_ADD_MEMBERSHIP);
#endif
#if defined(IP_DROP_MEMBERSHIP)
PyModule_AddIntConstant(m, "IP_DROP_MEMBERSHIP", IP_DROP_MEMBERSHIP);
#endif
#if defined(IP_DEFAULT_MULTICAST_TTL)
PyModule_AddIntConstant(m, "IP_DEFAULT_MULTICAST_TTL",
IP_DEFAULT_MULTICAST_TTL);
#endif
#if defined(IP_DEFAULT_MULTICAST_LOOP)
PyModule_AddIntConstant(m, "IP_DEFAULT_MULTICAST_LOOP",
IP_DEFAULT_MULTICAST_LOOP);
#endif
#if defined(IP_MAX_MEMBERSHIPS)
PyModule_AddIntConstant(m, "IP_MAX_MEMBERSHIPS", IP_MAX_MEMBERSHIPS);
#endif
#if defined(IPV6_JOIN_GROUP)
PyModule_AddIntConstant(m, "IPV6_JOIN_GROUP", IPV6_JOIN_GROUP);
#endif
#if defined(IPV6_LEAVE_GROUP)
PyModule_AddIntConstant(m, "IPV6_LEAVE_GROUP", IPV6_LEAVE_GROUP);
#endif
#if defined(IPV6_MULTICAST_HOPS)
PyModule_AddIntConstant(m, "IPV6_MULTICAST_HOPS", IPV6_MULTICAST_HOPS);
#endif
#if defined(IPV6_MULTICAST_IF)
PyModule_AddIntConstant(m, "IPV6_MULTICAST_IF", IPV6_MULTICAST_IF);
#endif
#if defined(IPV6_MULTICAST_LOOP)
PyModule_AddIntConstant(m, "IPV6_MULTICAST_LOOP", IPV6_MULTICAST_LOOP);
#endif
#if defined(IPV6_UNICAST_HOPS)
PyModule_AddIntConstant(m, "IPV6_UNICAST_HOPS", IPV6_UNICAST_HOPS);
#endif
#if defined(IPV6_V6ONLY)
PyModule_AddIntConstant(m, "IPV6_V6ONLY", IPV6_V6ONLY);
#endif
#if defined(IPV6_CHECKSUM)
PyModule_AddIntConstant(m, "IPV6_CHECKSUM", IPV6_CHECKSUM);
#endif
#if defined(IPV6_DONTFRAG)
PyModule_AddIntConstant(m, "IPV6_DONTFRAG", IPV6_DONTFRAG);
#endif
#if defined(IPV6_DSTOPTS)
PyModule_AddIntConstant(m, "IPV6_DSTOPTS", IPV6_DSTOPTS);
#endif
#if defined(IPV6_HOPLIMIT)
PyModule_AddIntConstant(m, "IPV6_HOPLIMIT", IPV6_HOPLIMIT);
#endif
#if defined(IPV6_HOPOPTS)
PyModule_AddIntConstant(m, "IPV6_HOPOPTS", IPV6_HOPOPTS);
#endif
#if defined(IPV6_NEXTHOP)
PyModule_AddIntConstant(m, "IPV6_NEXTHOP", IPV6_NEXTHOP);
#endif
#if defined(IPV6_PATHMTU)
PyModule_AddIntConstant(m, "IPV6_PATHMTU", IPV6_PATHMTU);
#endif
#if defined(IPV6_PKTINFO)
PyModule_AddIntConstant(m, "IPV6_PKTINFO", IPV6_PKTINFO);
#endif
#if defined(IPV6_RECVDSTOPTS)
PyModule_AddIntConstant(m, "IPV6_RECVDSTOPTS", IPV6_RECVDSTOPTS);
#endif
#if defined(IPV6_RECVHOPLIMIT)
PyModule_AddIntConstant(m, "IPV6_RECVHOPLIMIT", IPV6_RECVHOPLIMIT);
#endif
#if defined(IPV6_RECVHOPOPTS)
PyModule_AddIntConstant(m, "IPV6_RECVHOPOPTS", IPV6_RECVHOPOPTS);
#endif
#if defined(IPV6_RECVPKTINFO)
PyModule_AddIntConstant(m, "IPV6_RECVPKTINFO", IPV6_RECVPKTINFO);
#endif
#if defined(IPV6_RECVRTHDR)
PyModule_AddIntConstant(m, "IPV6_RECVRTHDR", IPV6_RECVRTHDR);
#endif
#if defined(IPV6_RECVTCLASS)
PyModule_AddIntConstant(m, "IPV6_RECVTCLASS", IPV6_RECVTCLASS);
#endif
#if defined(IPV6_RTHDR)
PyModule_AddIntConstant(m, "IPV6_RTHDR", IPV6_RTHDR);
#endif
#if defined(IPV6_RTHDRDSTOPTS)
PyModule_AddIntConstant(m, "IPV6_RTHDRDSTOPTS", IPV6_RTHDRDSTOPTS);
#endif
#if defined(IPV6_RTHDR_TYPE_0)
PyModule_AddIntConstant(m, "IPV6_RTHDR_TYPE_0", IPV6_RTHDR_TYPE_0);
#endif
#if defined(IPV6_RECVPATHMTU)
PyModule_AddIntConstant(m, "IPV6_RECVPATHMTU", IPV6_RECVPATHMTU);
#endif
#if defined(IPV6_TCLASS)
PyModule_AddIntConstant(m, "IPV6_TCLASS", IPV6_TCLASS);
#endif
#if defined(IPV6_USE_MIN_MTU)
PyModule_AddIntConstant(m, "IPV6_USE_MIN_MTU", IPV6_USE_MIN_MTU);
#endif
#if defined(TCP_NODELAY)
PyModule_AddIntConstant(m, "TCP_NODELAY", TCP_NODELAY);
#endif
#if defined(TCP_MAXSEG)
PyModule_AddIntConstant(m, "TCP_MAXSEG", TCP_MAXSEG);
#endif
#if defined(TCP_CORK)
PyModule_AddIntConstant(m, "TCP_CORK", TCP_CORK);
#endif
#if defined(TCP_KEEPIDLE)
PyModule_AddIntConstant(m, "TCP_KEEPIDLE", TCP_KEEPIDLE);
#endif
#if defined(TCP_KEEPINTVL)
PyModule_AddIntConstant(m, "TCP_KEEPINTVL", TCP_KEEPINTVL);
#endif
#if defined(TCP_KEEPCNT)
PyModule_AddIntConstant(m, "TCP_KEEPCNT", TCP_KEEPCNT);
#endif
#if defined(TCP_SYNCNT)
PyModule_AddIntConstant(m, "TCP_SYNCNT", TCP_SYNCNT);
#endif
#if defined(TCP_LINGER2)
PyModule_AddIntConstant(m, "TCP_LINGER2", TCP_LINGER2);
#endif
#if defined(TCP_DEFER_ACCEPT)
PyModule_AddIntConstant(m, "TCP_DEFER_ACCEPT", TCP_DEFER_ACCEPT);
#endif
#if defined(TCP_WINDOW_CLAMP)
PyModule_AddIntConstant(m, "TCP_WINDOW_CLAMP", TCP_WINDOW_CLAMP);
#endif
#if defined(TCP_INFO)
PyModule_AddIntConstant(m, "TCP_INFO", TCP_INFO);
#endif
#if defined(TCP_QUICKACK)
PyModule_AddIntConstant(m, "TCP_QUICKACK", TCP_QUICKACK);
#endif
#if defined(IPX_TYPE)
PyModule_AddIntConstant(m, "IPX_TYPE", IPX_TYPE);
#endif
#if defined(EAI_ADDRFAMILY)
PyModule_AddIntConstant(m, "EAI_ADDRFAMILY", EAI_ADDRFAMILY);
#endif
#if defined(EAI_AGAIN)
PyModule_AddIntConstant(m, "EAI_AGAIN", EAI_AGAIN);
#endif
#if defined(EAI_BADFLAGS)
PyModule_AddIntConstant(m, "EAI_BADFLAGS", EAI_BADFLAGS);
#endif
#if defined(EAI_FAIL)
PyModule_AddIntConstant(m, "EAI_FAIL", EAI_FAIL);
#endif
#if defined(EAI_FAMILY)
PyModule_AddIntConstant(m, "EAI_FAMILY", EAI_FAMILY);
#endif
#if defined(EAI_MEMORY)
PyModule_AddIntConstant(m, "EAI_MEMORY", EAI_MEMORY);
#endif
#if defined(EAI_NODATA)
PyModule_AddIntConstant(m, "EAI_NODATA", EAI_NODATA);
#endif
#if defined(EAI_NONAME)
PyModule_AddIntConstant(m, "EAI_NONAME", EAI_NONAME);
#endif
#if defined(EAI_OVERFLOW)
PyModule_AddIntConstant(m, "EAI_OVERFLOW", EAI_OVERFLOW);
#endif
#if defined(EAI_SERVICE)
PyModule_AddIntConstant(m, "EAI_SERVICE", EAI_SERVICE);
#endif
#if defined(EAI_SOCKTYPE)
PyModule_AddIntConstant(m, "EAI_SOCKTYPE", EAI_SOCKTYPE);
#endif
#if defined(EAI_SYSTEM)
PyModule_AddIntConstant(m, "EAI_SYSTEM", EAI_SYSTEM);
#endif
#if defined(EAI_BADHINTS)
PyModule_AddIntConstant(m, "EAI_BADHINTS", EAI_BADHINTS);
#endif
#if defined(EAI_PROTOCOL)
PyModule_AddIntConstant(m, "EAI_PROTOCOL", EAI_PROTOCOL);
#endif
#if defined(EAI_MAX)
PyModule_AddIntConstant(m, "EAI_MAX", EAI_MAX);
#endif
#if defined(AI_PASSIVE)
PyModule_AddIntConstant(m, "AI_PASSIVE", AI_PASSIVE);
#endif
#if defined(AI_CANONNAME)
PyModule_AddIntConstant(m, "AI_CANONNAME", AI_CANONNAME);
#endif
#if defined(AI_NUMERICHOST)
PyModule_AddIntConstant(m, "AI_NUMERICHOST", AI_NUMERICHOST);
#endif
#if defined(AI_NUMERICSERV)
PyModule_AddIntConstant(m, "AI_NUMERICSERV", AI_NUMERICSERV);
#endif
#if defined(AI_MASK)
PyModule_AddIntConstant(m, "AI_MASK", AI_MASK);
#endif
#if defined(AI_ALL)
PyModule_AddIntConstant(m, "AI_ALL", AI_ALL);
#endif
#if defined(AI_V4MAPPED_CFG)
PyModule_AddIntConstant(m, "AI_V4MAPPED_CFG", AI_V4MAPPED_CFG);
#endif
#if defined(AI_ADDRCONFIG)
PyModule_AddIntConstant(m, "AI_ADDRCONFIG", AI_ADDRCONFIG);
#endif
#if defined(AI_V4MAPPED)
PyModule_AddIntConstant(m, "AI_V4MAPPED", AI_V4MAPPED);
#endif
#if defined(AI_DEFAULT)
PyModule_AddIntConstant(m, "AI_DEFAULT", AI_DEFAULT);
#endif
#if defined(NI_MAXHOST)
PyModule_AddIntConstant(m, "NI_MAXHOST", NI_MAXHOST);
#endif
#if defined(NI_MAXSERV)
PyModule_AddIntConstant(m, "NI_MAXSERV", NI_MAXSERV);
#endif
#if defined(NI_NOFQDN)
PyModule_AddIntConstant(m, "NI_NOFQDN", NI_NOFQDN);
#endif
#if defined(NI_NUMERICHOST)
PyModule_AddIntConstant(m, "NI_NUMERICHOST", NI_NUMERICHOST);
#endif
#if defined(NI_NAMEREQD)
PyModule_AddIntConstant(m, "NI_NAMEREQD", NI_NAMEREQD);
#endif
#if defined(NI_NUMERICSERV)
PyModule_AddIntConstant(m, "NI_NUMERICSERV", NI_NUMERICSERV);
#endif
#if defined(NI_DGRAM)
PyModule_AddIntConstant(m, "NI_DGRAM", NI_DGRAM);
#endif
#if defined(SHUT_RD)
PyModule_AddIntConstant(m, "SHUT_RD", SHUT_RD);
#elif defined(SD_RECEIVE)
PyModule_AddIntConstant(m, "SHUT_RD", SD_RECEIVE);
#else
PyModule_AddIntConstant(m, "SHUT_RD", 0);
#endif
#if defined(SHUT_WR)
PyModule_AddIntConstant(m, "SHUT_WR", SHUT_WR);
#elif defined(SD_SEND)
PyModule_AddIntConstant(m, "SHUT_WR", SD_SEND);
#else
PyModule_AddIntConstant(m, "SHUT_WR", 1);
#endif
#if defined(SHUT_RDWR)
PyModule_AddIntConstant(m, "SHUT_RDWR", SHUT_RDWR);
#elif defined(SD_BOTH)
PyModule_AddIntConstant(m, "SHUT_RDWR", SD_BOTH);
#else
PyModule_AddIntConstant(m, "SHUT_RDWR", 2);
#endif
#if defined(SIO_RCVALL)
{
PyObject *tmp;
tmp = PyLong_FromUnsignedLong(SIO_RCVALL);
if (tmp == NULL)
return;
PyModule_AddObject(m, "SIO_RCVALL", tmp);
}
PyModule_AddIntConstant(m, "RCVALL_OFF", RCVALL_OFF);
PyModule_AddIntConstant(m, "RCVALL_ON", RCVALL_ON);
PyModule_AddIntConstant(m, "RCVALL_SOCKETLEVELONLY", RCVALL_SOCKETLEVELONLY);
#if defined(RCVALL_IPLEVEL)
PyModule_AddIntConstant(m, "RCVALL_IPLEVEL", RCVALL_IPLEVEL);
#endif
#if defined(RCVALL_MAX)
PyModule_AddIntConstant(m, "RCVALL_MAX", RCVALL_MAX);
#endif
#endif
#if defined(USE_GETHOSTBYNAME_LOCK) || defined(USE_GETADDRINFO_LOCK)
netdb_lock = PyThread_allocate_lock();
#endif
}
#if !defined(HAVE_INET_PTON)
#if !defined(NTDDI_VERSION) || (NTDDI_VERSION < NTDDI_LONGHORN)
int
inet_pton(int af, const char *src, void *dst) {
if (af == AF_INET) {
long packed_addr;
packed_addr = inet_addr(src);
if (packed_addr == INADDR_NONE)
return 0;
memcpy(dst, &packed_addr, 4);
return 1;
}
return -1;
}
const char *
inet_ntop(int af, const void *src, char *dst, socklen_t size) {
if (af == AF_INET) {
struct in_addr packed_addr;
if (size < 16)
return NULL;
memcpy(&packed_addr, src, sizeof(packed_addr));
return strncpy(dst, inet_ntoa(packed_addr), size);
}
return NULL;
}
#endif
#endif