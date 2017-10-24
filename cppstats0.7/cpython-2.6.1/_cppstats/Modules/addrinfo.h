#if !defined(HAVE_GETADDRINFO)
#if defined(EAI_ADDRFAMILY)
#undef EAI_ADDRFAMILY
#undef EAI_AGAIN
#undef EAI_BADFLAGS
#undef EAI_FAIL
#undef EAI_FAMILY
#undef EAI_MEMORY
#undef EAI_NODATA
#undef EAI_NONAME
#undef EAI_SERVICE
#undef EAI_SOCKTYPE
#undef EAI_SYSTEM
#undef EAI_BADHINTS
#undef EAI_PROTOCOL
#undef EAI_MAX
#undef getaddrinfo
#define getaddrinfo fake_getaddrinfo
#endif
#define EAI_ADDRFAMILY 1
#define EAI_AGAIN 2
#define EAI_BADFLAGS 3
#define EAI_FAIL 4
#define EAI_FAMILY 5
#define EAI_MEMORY 6
#define EAI_NODATA 7
#define EAI_NONAME 8
#define EAI_SERVICE 9
#define EAI_SOCKTYPE 10
#define EAI_SYSTEM 11
#define EAI_BADHINTS 12
#define EAI_PROTOCOL 13
#define EAI_MAX 14
#if defined(AI_PASSIVE)
#undef AI_PASSIVE
#undef AI_CANONNAME
#undef AI_NUMERICHOST
#undef AI_MASK
#undef AI_ALL
#undef AI_V4MAPPED_CFG
#undef AI_ADDRCONFIG
#undef AI_V4MAPPED
#undef AI_DEFAULT
#endif
#define AI_PASSIVE 0x00000001
#define AI_CANONNAME 0x00000002
#define AI_NUMERICHOST 0x00000004
#define AI_MASK (AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST)
#define AI_ALL 0x00000100
#define AI_V4MAPPED_CFG 0x00000200
#define AI_ADDRCONFIG 0x00000400
#define AI_V4MAPPED 0x00000800
#define AI_DEFAULT (AI_V4MAPPED_CFG | AI_ADDRCONFIG)
#endif
#if !defined(HAVE_GETNAMEINFO)
#if !defined(NI_MAXHOST)
#define NI_MAXHOST 1025
#define NI_MAXSERV 32
#endif
#if !defined(NI_NOFQDN)
#define NI_NOFQDN 0x00000001
#define NI_NUMERICHOST 0x00000002
#define NI_NAMEREQD 0x00000004
#define NI_NUMERICSERV 0x00000008
#define NI_DGRAM 0x00000010
#endif
#endif
#if !defined(HAVE_ADDRINFO)
struct addrinfo {
int ai_flags;
int ai_family;
int ai_socktype;
int ai_protocol;
size_t ai_addrlen;
char *ai_canonname;
struct sockaddr *ai_addr;
struct addrinfo *ai_next;
};
#endif
#if !defined(HAVE_SOCKADDR_STORAGE)
#define _SS_MAXSIZE 128
#if defined(HAVE_LONG_LONG)
#define _SS_ALIGNSIZE (sizeof(PY_LONG_LONG))
#else
#define _SS_ALIGNSIZE (sizeof(double))
#endif
#define _SS_PAD1SIZE (_SS_ALIGNSIZE - sizeof(u_char) * 2)
#define _SS_PAD2SIZE (_SS_MAXSIZE - sizeof(u_char) * 2 - _SS_PAD1SIZE - _SS_ALIGNSIZE)
struct sockaddr_storage {
#if defined(HAVE_SOCKADDR_SA_LEN)
unsigned char ss_len;
unsigned char ss_family;
#else
unsigned short ss_family;
#endif
char __ss_pad1[_SS_PAD1SIZE];
#if defined(HAVE_LONG_LONG)
PY_LONG_LONG __ss_align;
#else
double __ss_align;
#endif
char __ss_pad2[_SS_PAD2SIZE];
};
#endif
#if defined(__cplusplus)
extern "C" {
#endif
extern void freehostent Py_PROTO((struct hostent *));
#if defined(__cplusplus)
}
#endif
