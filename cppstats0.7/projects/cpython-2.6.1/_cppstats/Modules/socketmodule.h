#if !defined(MS_WINDOWS)
#if defined(__VMS)
#include <socket.h>
#else
#include <sys/socket.h>
#endif
#include <netinet/in.h>
#if !(defined(__BEOS__) || defined(__CYGWIN__) || (defined(PYOS_OS2) && defined(PYCC_VACPP)))
#include <netinet/tcp.h>
#endif
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(SIO_GET_MULTICAST_FILTER)
#include <MSTcpIP.h>
#define HAVE_ADDRINFO
#define HAVE_SOCKADDR_STORAGE
#define HAVE_GETADDRINFO
#define HAVE_GETNAMEINFO
#define ENABLE_IPV6
#else
typedef int socklen_t;
#endif
#endif
#if defined(HAVE_SYS_UN_H)
#include <sys/un.h>
#else
#undef AF_UNIX
#endif
#if defined(HAVE_LINUX_NETLINK_H)
#if defined(HAVE_ASM_TYPES_H)
#include <asm/types.h>
#endif
#include <linux/netlink.h>
#else
#undef AF_NETLINK
#endif
#if defined(HAVE_BLUETOOTH_BLUETOOTH_H)
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sco.h>
#include <bluetooth/hci.h>
#endif
#if defined(HAVE_BLUETOOTH_H)
#include <bluetooth.h>
#endif
#if defined(HAVE_NETPACKET_PACKET_H)
#include <sys/ioctl.h>
#include <net/if.h>
#include <netpacket/packet.h>
#endif
#if defined(HAVE_LINUX_TIPC_H)
#include <linux/tipc.h>
#endif
#if !defined(Py__SOCKET_H)
#define Py__SOCKET_H
#if defined(__cplusplus)
extern "C" {
#endif
#define PySocket_MODULE_NAME "_socket"
#define PySocket_CAPI_NAME "CAPI"
#if defined(MS_WINDOWS)
typedef SOCKET SOCKET_T;
#if defined(MS_WIN64)
#define SIZEOF_SOCKET_T 8
#else
#define SIZEOF_SOCKET_T 4
#endif
#else
typedef int SOCKET_T;
#define SIZEOF_SOCKET_T SIZEOF_INT
#endif
typedef union sock_addr {
struct sockaddr_in in;
#if defined(AF_UNIX)
struct sockaddr_un un;
#endif
#if defined(AF_NETLINK)
struct sockaddr_nl nl;
#endif
#if defined(ENABLE_IPV6)
struct sockaddr_in6 in6;
struct sockaddr_storage storage;
#endif
#if defined(HAVE_BLUETOOTH_BLUETOOTH_H)
struct sockaddr_l2 bt_l2;
struct sockaddr_rc bt_rc;
struct sockaddr_sco bt_sco;
struct sockaddr_hci bt_hci;
#endif
#if defined(HAVE_NETPACKET_PACKET_H)
struct sockaddr_ll ll;
#endif
} sock_addr_t;
typedef struct {
PyObject_HEAD
SOCKET_T sock_fd;
int sock_family;
int sock_type;
int sock_proto;
PyObject *(*errorhandler)(void);
double sock_timeout;
} PySocketSockObject;
typedef struct {
PyTypeObject *Sock_Type;
PyObject *error;
} PySocketModule_APIObject;
#if !defined(PySocket_BUILDING_SOCKET)
static
PySocketModule_APIObject PySocketModule;
#if !defined(DPRINTF)
#define DPRINTF if (0) printf
#endif
static
int PySocketModule_ImportModuleAndAPI(void) {
PyObject *mod = 0, *v = 0;
char *apimodule = PySocket_MODULE_NAME;
char *apiname = PySocket_CAPI_NAME;
void *api;
DPRINTF("Importing the %s C API...\n", apimodule);
mod = PyImport_ImportModuleNoBlock(apimodule);
if (mod == NULL)
goto onError;
DPRINTF(" %s package found\n", apimodule);
v = PyObject_GetAttrString(mod, apiname);
if (v == NULL)
goto onError;
Py_DECREF(mod);
DPRINTF(" API object %s found\n", apiname);
api = PyCObject_AsVoidPtr(v);
if (api == NULL)
goto onError;
Py_DECREF(v);
memcpy(&PySocketModule, api, sizeof(PySocketModule));
DPRINTF(" API object loaded and initialized.\n");
return 0;
onError:
DPRINTF(" not found.\n");
Py_XDECREF(mod);
Py_XDECREF(v);
return -1;
}
#endif
#if defined(__cplusplus)
}
#endif
#endif
