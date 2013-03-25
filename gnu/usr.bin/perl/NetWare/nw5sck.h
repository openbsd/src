
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME     :  nw5sck.h
 * DESCRIPTION  :  Socket related functions.
 * Author       :  SGP
 * Date         :  January 2001.
 * Date Modified:  June 26th 2001.
 *
 */



#ifndef  _INC_NW_SOCKET
#define  _INC_NW_SOCKET


#include <sys/socket.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef u_int           SOCKET;

struct nwsockent local_context;

# undef gethostbyname
# undef gethostbyaddr

# undef endhostent
# undef endnetent
# undef endprotoent
# undef endservent
# undef gethostent
# undef getprotoent
# undef getnetbyaddr
# undef getnetbyname
# undef gethostbyaddr
# undef getprotobyname
# undef getservbyname
# undef getservbyport
# undef getservent
# undef sethostent
# undef setnetent
# undef setprotoent
# undef setservent

# define gethostbyname(name)   NetDBgethostbyname(&local_context,name)
# define gethostbyaddr(a,l,t)  NetDBgethostbyaddr(&local_context,a,l,t)

# define endhostent()          NetDBendhostent(&local_context)
# define endnetent()           NWendnetent(&local_context)
# define endprotoent()         NWendprotoent(&local_context)
# define endservent()          NWendservent(&local_context)
# define gethostent()          NetDBgethostent(&local_context,NULL)
# define getprotoent()         NWgetprotoent(&local_context)
# define gethostbyaddr(a,l,t)  NetDBgethostbyaddr(&local_context,a,l,t)
# define getnetbyaddr(net,typ) NWgetnetbyaddr(&local_context,net,typ)
# define getnetbyname(name)    NWgetnetbyname(&local_context,name)
# define getprotobyname(name)  NWgetprotobyname(&local_context,name)
# define getservbyname(n,p)    NWgetservbyname(&local_context,n,p)
# define getservbyport(n,p)    NWgetservbyport(&local_context,n,p)
# define getservent()          NWgetservent(&local_context)
# define sethostent()          NWsethostent(&local_context, stayopen)
# define setnetent()           NWsetnetent(&local_context, stayopen)
# define setprotoent()         NWsetprotoent(&local_context, stayopen)
# define setservent()          NWsetservent(&local_context, stayopen)

u_long nw_htonl(u_long hostlong);
u_short nw_htons(u_short hostshort);
u_long nw_ntohl(u_long netlong);
u_short nw_ntohs(u_short netshort);

SOCKET nw_accept(SOCKET s, struct sockaddr *addr, int *addrlen);
int nw_bind(SOCKET s, const struct sockaddr *addr, int addrlen);
int nw_connect(SOCKET s, const struct sockaddr *addr, int addrlen);

struct hostent * nw_gethostbyname(const char * name);
struct hostent * nw_gethostbyaddr(const char *addr, int len, int type);
int nw_gethostname(char *name, int len);
struct netent * nw_getnetbyaddr(long net, int type);
struct netent *nw_getnetbyname(char *name);
int nw_getpeername(SOCKET s, struct sockaddr *addr, int *addrlen);
struct protoent * nw_getprotobyname(const char *name);
struct protoent * nw_getprotobynumber(int num);
struct servent * nw_getservbyname(const char *name, const char *proto);
struct servent * nw_getservbyport(int port, const char *proto);
struct servent * nw_getservent(void);
void nw_sethostent(int stayopen);
void nw_setnetent(int stayopen);
void nw_setprotoent(int stayopen);
void nw_setservent(int stayopen);
int nw_setsockopt(SOCKET s, int level, int optname, const char* optval, int optlen);

int nw_getsockname(SOCKET s, struct sockaddr *addr, int *addrlen);
int nw_getsockopt(SOCKET s, int level, int optname, char *optval, int *optlen);

unsigned long nw_inet_addr(const char *cp);
char * nw_inet_ntoa(struct in_addr in);

void nw_endhostent();
void nw_endnetent();
void nw_endprotoent();
void nw_endservent();
struct hostent *nw_gethostent();
struct netent *nw_getnetent();
struct protoent * nw_getprotoent();

SOCKET nw_socket(int af, int type, int protocol);
int nw_listen(SOCKET s, int backlog);
int nw_send(SOCKET s, const char *buf, int len, int flags);
int nw_recv(SOCKET s, char *buf, int len, int flags);
int nw_sendto(SOCKET s, const char *buf, int len, int flags,const struct sockaddr *to, int tolen);
int nw_recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);
int nw_select(int nfds, fd_set* rd, fd_set* wr, fd_set* ex, const struct timeval* timeout);
int nw_shutdown(SOCKET s, int how);
#ifdef __cplusplus
}
#endif


#endif	// _INC_NW_SOCKET

