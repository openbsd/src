/* Time-stamp: <01/08/01 21:01:12 keuchel@w2k> */

/* wincesck.c
 *
 * (c) 1995 Microsoft Corporation. All rights reserved.
 * 		Developed by hip communications inc.
 * Portions (c) 1993 Intergraph Corporation. All rights reserved.
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

/* The socket calls use fd functions from celib... */

#define WIN32IO_IS_STDIO
#define WIN32SCK_IS_STDSCK
#define WIN32_LEAN_AND_MEAN

#ifdef __GNUC__
#define Win32_Winsock
#endif

#include <windows.h>

#define wince_private
#include "errno.h"

#include "EXTERN.h"
#include "perl.h"

#include "Win32iop.h"
#include <sys/socket.h>

#ifndef UNDER_CE
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <io.h>
#endif

#ifdef UNDER_CE

XCE_EXPORT struct servent *xcegetservbyname(const char *sname, const char *sproto);
XCE_EXPORT struct servent * xcegetservbyport(int aport, const char *sproto);
XCE_EXPORT struct protoent *xcegetprotobyname(const char *name);
XCE_EXPORT struct protoent *xcegetprotobynumber(int number);

#define getservbyname xcegetservbyname
#define getservbyport xcegetservbyport
#define getprotobyname xcegetprotobyname
#define getprotobynumber xcegetprotobynumber

/* uses fdtab... */
#include "cesocket2.h"

#endif

#define TO_SOCKET(X) (X)

#define StartSockets() \
    STMT_START {					\
	if (!wsock_started)				\
	    start_sockets();				\
    } STMT_END

#define SOCKET_TEST(x, y) \
    STMT_START {					\
	StartSockets();					\
	if((x) == (y))					\
	    errno = WSAGetLastError();			\
    } STMT_END

#define SOCKET_TEST_ERROR(x) SOCKET_TEST(x, SOCKET_ERROR)

static struct servent* win32_savecopyservent(struct servent*d,
                                             struct servent*s,
                                             const char *proto);

static int wsock_started = 0;

EXTERN_C void
EndSockets(void)
{
    if (wsock_started)
	WSACleanup();
}

void
start_sockets(void)
{
    dTHX;
    unsigned short version;
    WSADATA retdata;
    int ret;

    /*
     * initalize the winsock interface and insure that it is
     * cleaned up at exit.
     */
    version = 0x101;
    if(ret = WSAStartup(version, &retdata))
	Perl_croak_nocontext("Unable to locate winsock library!\n");
    if(retdata.wVersion != version)
	Perl_croak_nocontext("Could not find version 1.1 of winsock dll\n");

    /* atexit((void (*)(void)) EndSockets); */
    wsock_started = 1;
}

u_long
win32_htonl(u_long hostlong)
{
    StartSockets();
    return htonl(hostlong);
}

u_short
win32_htons(u_short hostshort)
{
    StartSockets();
    return htons(hostshort);
}

u_long
win32_ntohl(u_long netlong)
{
    StartSockets();
    return ntohl(netlong);
}

u_short
win32_ntohs(u_short netshort)
{
    StartSockets();
    return ntohs(netshort);
}

SOCKET
win32_socket(int af, int type, int protocol)
{
  StartSockets();
  return xcesocket(af, type, protocol);
}

SOCKET
win32_accept(SOCKET s, struct sockaddr *addr, int *addrlen)
{
  StartSockets();
  return xceaccept(s, addr, addrlen);
}

int
win32_bind(SOCKET s, const struct sockaddr *addr, int addrlen)
{
  StartSockets();
  return xcebind(s, addr, addrlen);
}

int
win32_connect(SOCKET s, const struct sockaddr *addr, int addrlen)
{
  StartSockets();
  return xceconnect(s, addr, addrlen);
}


int
win32_getpeername(SOCKET s, struct sockaddr *addr, int *addrlen)
{
  StartSockets();
  return xcegetpeername(s, addr, addrlen);
}

int
win32_getsockname(SOCKET s, struct sockaddr *addr, int *addrlen)
{
  StartSockets();
  return xcegetsockname(s, addr, addrlen);
}

int
win32_getsockopt(SOCKET s, int level, int optname, char *optval, int *optlen)
{
  StartSockets();
  return xcegetsockopt(s, level, optname, optval, optlen);
}

int
win32_ioctlsocket(SOCKET s, long cmd, u_long *argp)
{
  StartSockets();
  return xceioctlsocket(s, cmd, argp);
}

int
win32_listen(SOCKET s, int backlog)
{
  StartSockets();
  return xcelisten(s, backlog);
}

int
win32_recv(SOCKET s, char *buf, int len, int flags)
{
  StartSockets();
  return xcerecv(s, buf, len, flags);
}

int
win32_recvfrom(SOCKET s, char *buf, int len, int flags,
	       struct sockaddr *from, int *fromlen)
{
  StartSockets();
  return xcerecvfrom(s, buf, len, flags, from, fromlen);
}

int
win32_select(int nfds, Perl_fd_set* rd, Perl_fd_set* wr,
	     Perl_fd_set* ex, const struct timeval* timeout)
{
  StartSockets();
  /* select not yet fixed */
  errno = ENOSYS;
  return -1;
}

int
win32_send(SOCKET s, const char *buf, int len, int flags)
{
  StartSockets();
  return xcesend(s, buf, len, flags);
}

int
win32_sendto(SOCKET s, const char *buf, int len, int flags,
	     const struct sockaddr *to, int tolen)
{
  StartSockets();
  return xcesendto(s, buf, len, flags, to, tolen);
}

int
win32_setsockopt(SOCKET s, int level, int optname,
		 const char *optval, int optlen)
{
  StartSockets();
  return xcesetsockopt(s, level, optname, optval, optlen);
}

int
win32_shutdown(SOCKET s, int how)
{
  StartSockets();
  return xceshutdown(s, how);
}

int
win32_closesocket(SOCKET s)
{
  StartSockets();
  return xceclosesocket(s);
}

struct hostent *
win32_gethostbyaddr(const char *addr, int len, int type)
{
  struct hostent *r;

  SOCKET_TEST(r = gethostbyaddr(addr, len, type), NULL);
  return r;
}

struct hostent *
win32_gethostbyname(const char *name)
{
  struct hostent *r;

  SOCKET_TEST(r = gethostbyname(name), NULL);
  return r;
}

int
win32_gethostname(char *name, int len)
{
  int r;

  SOCKET_TEST_ERROR(r = gethostname(name, len));
  return r;
}

struct protoent *
win32_getprotobyname(const char *name)
{
    struct protoent *r;

    SOCKET_TEST(r = getprotobyname(name), NULL);
    return r;
}

struct protoent *
win32_getprotobynumber(int num)
{
    struct protoent *r;

    SOCKET_TEST(r = getprotobynumber(num), NULL);
    return r;
}

struct servent *
win32_getservbyname(const char *name, const char *proto)
{
    dTHX;
    struct servent *r;

    SOCKET_TEST(r = getservbyname(name, proto), NULL);
    if (r) {
	r = win32_savecopyservent(&w32_servent, r, proto);
    }
    return r;
}

struct servent *
win32_getservbyport(int port, const char *proto)
{
    dTHX;
    struct servent *r;

    SOCKET_TEST(r = getservbyport(port, proto), NULL);
    if (r) {
	r = win32_savecopyservent(&w32_servent, r, proto);
    }
    return r;
}

int
win32_ioctl(int i, unsigned int u, char *data)
{
    dTHX;
    u_long u_long_arg; 
    int retval;
    
    if (!wsock_started) {
	Perl_croak_nocontext("ioctl implemented only on sockets");
	/* NOTREACHED */
    }

    /* mauke says using memcpy avoids alignment issues */
    memcpy(&u_long_arg, data, sizeof u_long_arg); 
    retval = ioctlsocket(TO_SOCKET(i), (long)u, &u_long_arg);
    memcpy(data, &u_long_arg, sizeof u_long_arg);
    
    if (retval == SOCKET_ERROR) {
	if (WSAGetLastError() == WSAENOTSOCK) {
	    Perl_croak_nocontext("ioctl implemented only on sockets");
	    /* NOTREACHED */
	}
	errno = WSAGetLastError();
    }
    return retval;
}

char FAR *
win32_inet_ntoa(struct in_addr in)
{
    StartSockets();
    return inet_ntoa(in);
}

unsigned long
win32_inet_addr(const char FAR *cp)
{
    StartSockets();
    return inet_addr(cp);
}

/*
 * Networking stubs
 */

void
win32_endhostent()
{
    dTHX;
    Perl_croak_nocontext("endhostent not implemented!\n");
}

void
win32_endnetent()
{
    dTHX;
    Perl_croak_nocontext("endnetent not implemented!\n");
}

void
win32_endprotoent()
{
    dTHX;
    Perl_croak_nocontext("endprotoent not implemented!\n");
}

void
win32_endservent()
{
    dTHX;
    Perl_croak_nocontext("endservent not implemented!\n");
}


struct netent *
win32_getnetent(void)
{
    dTHX;
    Perl_croak_nocontext("getnetent not implemented!\n");
    return (struct netent *) NULL;
}

struct netent *
win32_getnetbyname(char *name)
{
    dTHX;
    Perl_croak_nocontext("getnetbyname not implemented!\n");
    return (struct netent *)NULL;
}

struct netent *
win32_getnetbyaddr(long net, int type)
{
    dTHX;
    Perl_croak_nocontext("getnetbyaddr not implemented!\n");
    return (struct netent *)NULL;
}

struct protoent *
win32_getprotoent(void)
{
    dTHX;
    Perl_croak_nocontext("getprotoent not implemented!\n");
    return (struct protoent *) NULL;
}

struct servent *
win32_getservent(void)
{
    dTHX;
    Perl_croak_nocontext("getservent not implemented!\n");
    return (struct servent *) NULL;
}

void
win32_sethostent(int stayopen)
{
    dTHX;
    Perl_croak_nocontext("sethostent not implemented!\n");
}


void
win32_setnetent(int stayopen)
{
    dTHX;
    Perl_croak_nocontext("setnetent not implemented!\n");
}


void
win32_setprotoent(int stayopen)
{
    dTHX;
    Perl_croak_nocontext("setprotoent not implemented!\n");
}


void
win32_setservent(int stayopen)
{
    dTHX;
    Perl_croak_nocontext("setservent not implemented!\n");
}

static struct servent*
win32_savecopyservent(struct servent*d, struct servent*s, const char *proto)
{
    d->s_name = s->s_name;
    d->s_aliases = s->s_aliases;
    d->s_port = s->s_port;
    if (!IsWin95() && s->s_proto && strlen(s->s_proto))
	d->s_proto = s->s_proto;
    else
    if (proto && strlen(proto))
	d->s_proto = (char *)proto;
    else
	d->s_proto = "tcp";

    return d;
}
