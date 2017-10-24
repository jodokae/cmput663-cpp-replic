#if 0
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <string.h>
#include <stddef.h>
#include "addrinfo.h"
#endif
#define SUCCESS 0
#define YES 1
#define NO 0
static struct gni_afd {
int a_af;
int a_addrlen;
int a_socklen;
int a_off;
} gni_afdl [] = {
#if defined(ENABLE_IPV6)
{
PF_INET6, sizeof(struct in6_addr), sizeof(struct sockaddr_in6),
offsetof(struct sockaddr_in6, sin6_addr)
},
#endif
{
PF_INET, sizeof(struct in_addr), sizeof(struct sockaddr_in),
offsetof(struct sockaddr_in, sin_addr)
},
{0, 0, 0},
};
struct gni_sockinet {
u_char si_len;
u_char si_family;
u_short si_port;
};
#define ENI_NOSOCKET 0
#define ENI_NOSERVNAME 1
#define ENI_NOHOSTNAME 2
#define ENI_MEMORY 3
#define ENI_SYSTEM 4
#define ENI_FAMILY 5
#define ENI_SALEN 6
int getnameinfo Py_PROTO((const struct sockaddr *, size_t, char *, size_t,
char *, size_t, int));
int
getnameinfo(sa, salen, host, hostlen, serv, servlen, flags)
const struct sockaddr *sa;
size_t salen;
char *host;
size_t hostlen;
char *serv;
size_t servlen;
int flags;
{
struct gni_afd *gni_afd;
struct servent *sp;
struct hostent *hp;
u_short port;
int family, len, i;
char *addr, *p;
u_long v4a;
#if defined(ENABLE_IPV6)
u_char pfx;
#endif
int h_error;
char numserv[512];
char numaddr[512];
if (sa == NULL)
return ENI_NOSOCKET;
#if defined(HAVE_SOCKADDR_SA_LEN)
len = sa->sa_len;
if (len != salen) return ENI_SALEN;
#else
len = salen;
#endif
family = sa->sa_family;
for (i = 0; gni_afdl[i].a_af; i++)
if (gni_afdl[i].a_af == family) {
gni_afd = &gni_afdl[i];
goto found;
}
return ENI_FAMILY;
found:
if (len != gni_afd->a_socklen) return ENI_SALEN;
port = ((struct gni_sockinet *)sa)->si_port;
addr = (char *)sa + gni_afd->a_off;
if (serv == NULL || servlen == 0) {
} else if (flags & NI_NUMERICSERV) {
sprintf(numserv, "%d", ntohs(port));
if (strlen(numserv) > servlen)
return ENI_MEMORY;
strcpy(serv, numserv);
} else {
sp = getservbyport(port, (flags & NI_DGRAM) ? "udp" : "tcp");
if (sp) {
if (strlen(sp->s_name) > servlen)
return ENI_MEMORY;
strcpy(serv, sp->s_name);
} else
return ENI_NOSERVNAME;
}
switch (sa->sa_family) {
case AF_INET:
v4a = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
if (IN_MULTICAST(v4a) || IN_EXPERIMENTAL(v4a))
flags |= NI_NUMERICHOST;
v4a >>= IN_CLASSA_NSHIFT;
if (v4a == 0 || v4a == IN_LOOPBACKNET)
flags |= NI_NUMERICHOST;
break;
#if defined(ENABLE_IPV6)
case AF_INET6:
pfx = ((struct sockaddr_in6 *)sa)->sin6_addr.s6_addr8[0];
if (pfx == 0 || pfx == 0xfe || pfx == 0xff)
flags |= NI_NUMERICHOST;
break;
#endif
}
if (host == NULL || hostlen == 0) {
} else if (flags & NI_NUMERICHOST) {
if (inet_ntop(gni_afd->a_af, addr, numaddr, sizeof(numaddr))
== NULL)
return ENI_SYSTEM;
if (strlen(numaddr) > hostlen)
return ENI_MEMORY;
strcpy(host, numaddr);
} else {
#if defined(ENABLE_IPV6)
hp = getipnodebyaddr(addr, gni_afd->a_addrlen, gni_afd->a_af, &h_error);
#else
hp = gethostbyaddr(addr, gni_afd->a_addrlen, gni_afd->a_af);
h_error = h_errno;
#endif
if (hp) {
if (flags & NI_NOFQDN) {
p = strchr(hp->h_name, '.');
if (p) *p = '\0';
}
if (strlen(hp->h_name) > hostlen) {
#if defined(ENABLE_IPV6)
freehostent(hp);
#endif
return ENI_MEMORY;
}
strcpy(host, hp->h_name);
#if defined(ENABLE_IPV6)
freehostent(hp);
#endif
} else {
if (flags & NI_NAMEREQD)
return ENI_NOHOSTNAME;
if (inet_ntop(gni_afd->a_af, addr, numaddr, sizeof(numaddr))
== NULL)
return ENI_NOHOSTNAME;
if (strlen(numaddr) > hostlen)
return ENI_MEMORY;
strcpy(host, numaddr);
}
}
return SUCCESS;
}