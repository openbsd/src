/* win32sck.c
 *
 * (c) 1995 Microsoft Corporation. All rights reserved. 
 * 		Developed by hip communications inc.
 * Portions (c) 1993 Intergraph Corporation. All rights reserved.
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#define WIN32IO_IS_STDIO
#define WIN32SCK_IS_STDSCK
#define WIN32_LEAN_AND_MEAN
#define PERLIO_NOT_STDIO 0
#ifdef __GNUC__
#define Win32_Winsock
#endif
#include <windows.h>
#include <ws2spi.h>

#include "EXTERN.h"
#include "perl.h"

#include "Win32iop.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <io.h>

/* thanks to Beverly Brown	(beverly@datacube.com) */
#define OPEN_SOCKET(x)	win32_open_osfhandle(x,O_RDWR|O_BINARY)
#define TO_SOCKET(x)	_get_osfhandle(x)

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
    unsigned short version;
    WSADATA retdata;
    int ret;

    /*
     * initalize the winsock interface and insure that it is
     * cleaned up at exit.
     */
    version = 0x2;
    if(ret = WSAStartup(version, &retdata))
	Perl_croak_nocontext("Unable to locate winsock library!\n");
    if(retdata.wVersion != version)
	Perl_croak_nocontext("Could not find version 2.0 of winsock dll\n");

    /* atexit((void (*)(void)) EndSockets); */
    wsock_started = 1;
}

/* in no sockets Win32 builds, this fowards to replacements in util.c, dTHX
 * is required
 */
u_long
win32_htonl(u_long hostlong)
{
#ifdef MYSWAP
    dTHX;
#else
    StartSockets();
#endif
    return htonl(hostlong);
}

u_short
win32_htons(u_short hostshort)
{
#ifdef MYSWAP
    dTHX;
#else
    StartSockets();
#endif
    return htons(hostshort);
}

u_long
win32_ntohl(u_long netlong)
{
#ifdef MYSWAP
    dTHX;
#else
    StartSockets();
#endif
    return ntohl(netlong);
}

u_short
win32_ntohs(u_short netshort)
{
#ifdef MYSWAP
    dTHX;
#else
    StartSockets();
#endif
    return ntohs(netshort);
}



SOCKET
win32_accept(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    SOCKET r;

    SOCKET_TEST((r = accept(TO_SOCKET(s), addr, addrlen)), INVALID_SOCKET);
    return OPEN_SOCKET(r);
}

int
win32_bind(SOCKET s, const struct sockaddr *addr, int addrlen)
{
    int r;

    SOCKET_TEST_ERROR(r = bind(TO_SOCKET(s), addr, addrlen));
    return r;
}

int
win32_connect(SOCKET s, const struct sockaddr *addr, int addrlen)
{
    int r;

    SOCKET_TEST_ERROR(r = connect(TO_SOCKET(s), addr, addrlen));
    return r;
}


int
win32_getpeername(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    int r;

    SOCKET_TEST_ERROR(r = getpeername(TO_SOCKET(s), addr, addrlen));
    return r;
}

int
win32_getsockname(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    int r;

    SOCKET_TEST_ERROR(r = getsockname(TO_SOCKET(s), addr, addrlen));
    return r;
}

int
win32_getsockopt(SOCKET s, int level, int optname, char *optval, int *optlen)
{
    int r;

    SOCKET_TEST_ERROR(r = getsockopt(TO_SOCKET(s), level, optname, optval, optlen));
    return r;
}

int
win32_ioctlsocket(SOCKET s, long cmd, u_long *argp)
{
    int r;

    SOCKET_TEST_ERROR(r = ioctlsocket(TO_SOCKET(s), cmd, argp));
    return r;
}

int
win32_listen(SOCKET s, int backlog)
{
    int r;

    SOCKET_TEST_ERROR(r = listen(TO_SOCKET(s), backlog));
    return r;
}

int
win32_recv(SOCKET s, char *buf, int len, int flags)
{
    int r;

    SOCKET_TEST_ERROR(r = recv(TO_SOCKET(s), buf, len, flags));
    return r;
}

int
win32_recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
    int r;
    int frombufsize = *fromlen;

    SOCKET_TEST_ERROR(r = recvfrom(TO_SOCKET(s), buf, len, flags, from, fromlen));
    /* Winsock's recvfrom() only returns a valid 'from' when the socket
     * is connectionless.  Perl expects a valid 'from' for all types
     * of sockets, so go the extra mile.
     */
    if (r != SOCKET_ERROR && frombufsize == *fromlen)
	(void)win32_getpeername(s, from, fromlen);
    return r;
}

/* select contributed by Vincent R. Slyngstad (vrs@ibeam.intel.com) */
int
win32_select(int nfds, Perl_fd_set* rd, Perl_fd_set* wr, Perl_fd_set* ex, const struct timeval* timeout)
{
    int r;
    int i, fd, save_errno = errno;
    FD_SET nrd, nwr, nex;
    bool just_sleep = TRUE;

    StartSockets();

    FD_ZERO(&nrd);
    FD_ZERO(&nwr);
    FD_ZERO(&nex);
    for (i = 0; i < nfds; i++) {
	if (rd && PERL_FD_ISSET(i,rd)) {
	    fd = TO_SOCKET(i);
	    FD_SET((unsigned)fd, &nrd);
            just_sleep = FALSE;
	}
	if (wr && PERL_FD_ISSET(i,wr)) {
	    fd = TO_SOCKET(i);
	    FD_SET((unsigned)fd, &nwr);
            just_sleep = FALSE;
	}
	if (ex && PERL_FD_ISSET(i,ex)) {
	    fd = TO_SOCKET(i);
	    FD_SET((unsigned)fd, &nex);
            just_sleep = FALSE;
	}
    }

    /* winsock seems incapable of dealing with all three fd_sets being empty,
     * so do the (millisecond) sleep as a special case
     */
    if (just_sleep) {
	if (timeout)
	    Sleep(timeout->tv_sec  * 1000 +
		  timeout->tv_usec / 1000);	/* do the best we can */
	else
	    Sleep(UINT_MAX);
	return 0;
    }

    errno = save_errno;
    SOCKET_TEST_ERROR(r = select(nfds, &nrd, &nwr, &nex, timeout));
    save_errno = errno;

    for (i = 0; i < nfds; i++) {
	if (rd && PERL_FD_ISSET(i,rd)) {
	    fd = TO_SOCKET(i);
	    if (!FD_ISSET(fd, &nrd))
		PERL_FD_CLR(i,rd);
	}
	if (wr && PERL_FD_ISSET(i,wr)) {
	    fd = TO_SOCKET(i);
	    if (!FD_ISSET(fd, &nwr))
		PERL_FD_CLR(i,wr);
	}
	if (ex && PERL_FD_ISSET(i,ex)) {
	    fd = TO_SOCKET(i);
	    if (!FD_ISSET(fd, &nex))
		PERL_FD_CLR(i,ex);
	}
    }
    errno = save_errno;
    return r;
}

int
win32_send(SOCKET s, const char *buf, int len, int flags)
{
    int r;

    SOCKET_TEST_ERROR(r = send(TO_SOCKET(s), buf, len, flags));
    return r;
}

int
win32_sendto(SOCKET s, const char *buf, int len, int flags,
	     const struct sockaddr *to, int tolen)
{
    int r;

    SOCKET_TEST_ERROR(r = sendto(TO_SOCKET(s), buf, len, flags, to, tolen));
    return r;
}

int
win32_setsockopt(SOCKET s, int level, int optname, const char *optval, int optlen)
{
    int r;

    SOCKET_TEST_ERROR(r = setsockopt(TO_SOCKET(s), level, optname, optval, optlen));
    return r;
}
    
int
win32_shutdown(SOCKET s, int how)
{
    int r;

    SOCKET_TEST_ERROR(r = shutdown(TO_SOCKET(s), how));
    return r;
}

int
win32_closesocket(SOCKET s)
{
    int r;

    SOCKET_TEST_ERROR(r = closesocket(TO_SOCKET(s)));
    return r;
}

void
convert_proto_info_w2a(WSAPROTOCOL_INFOW *in, WSAPROTOCOL_INFOA *out)
{
    Copy(in, out, 1, WSAPROTOCOL_INFOA);
    wcstombs(out->szProtocol, in->szProtocol, sizeof(out->szProtocol));
}

SOCKET
open_ifs_socket(int af, int type, int protocol)
{
    dTHX;
    char *s;
    unsigned long proto_buffers_len = 0;
    int error_code;
    SOCKET out = INVALID_SOCKET;

    if ((s = PerlEnv_getenv("PERL_ALLOW_NON_IFS_LSP")) && atoi(s))
        return WSASocket(af, type, protocol, NULL, 0, 0);

    if (WSCEnumProtocols(NULL, NULL, &proto_buffers_len, &error_code) == SOCKET_ERROR
        && error_code == WSAENOBUFS)
    {
	WSAPROTOCOL_INFOW *proto_buffers;
        int protocols_available = 0;       
 
        Newx(proto_buffers, proto_buffers_len / sizeof(WSAPROTOCOL_INFOW),
            WSAPROTOCOL_INFOW);

        if ((protocols_available = WSCEnumProtocols(NULL, proto_buffers, 
            &proto_buffers_len, &error_code)) != SOCKET_ERROR)
        {
            int i;
            for (i = 0; i < protocols_available; i++)
            {
                WSAPROTOCOL_INFOA proto_info;

                if ((af != AF_UNSPEC && af != proto_buffers[i].iAddressFamily)
                    || (type != proto_buffers[i].iSocketType)
                    || (protocol != 0 && proto_buffers[i].iProtocol != 0 &&
                        protocol != proto_buffers[i].iProtocol))
                    continue;

                if ((proto_buffers[i].dwServiceFlags1 & XP1_IFS_HANDLES) == 0)
                    continue;

                convert_proto_info_w2a(&(proto_buffers[i]), &proto_info);

                out = WSASocket(af, type, protocol, &proto_info, 0, 0);
                break;
            }
        }

        Safefree(proto_buffers);
    }

    return out;
}

SOCKET
win32_socket(int af, int type, int protocol)
{
    SOCKET s;

    StartSockets();

    if((s = open_ifs_socket(af, type, protocol)) == INVALID_SOCKET)
	errno = WSAGetLastError();
    else
	s = OPEN_SOCKET(s);

    return s;
}

/*
 * close RTL fd while respecting sockets
 * added as temporary measure until PerlIO has real
 * Win32 native layer
 *   -- BKS, 11-11-2000
*/

int my_close(int fd)
{
    int osf;
    if (!wsock_started)		/* No WinSock? */
	return(close(fd));	/* Then not a socket. */
    osf = TO_SOCKET(fd);/* Get it now before it's gone! */
    if (osf != -1) {
	int err;
	err = closesocket(osf);
	if (err == 0) {
	    (void)close(fd);	/* handle already closed, ignore error */
	    return 0;
	}
	else if (err == SOCKET_ERROR) {
	    err = WSAGetLastError();
	    if (err != WSAENOTSOCK) {
		(void)close(fd);
		errno = err;
		return EOF;
	    }
	}
    }
    return close(fd);
}

#undef fclose
int
my_fclose (FILE *pf)
{
    int osf;
    if (!wsock_started)		/* No WinSock? */
	return(fclose(pf));	/* Then not a socket. */
    osf = TO_SOCKET(win32_fileno(pf));/* Get it now before it's gone! */
    if (osf != -1) {
	int err;
	win32_fflush(pf);
	err = closesocket(osf);
	if (err == 0) {
	    (void)fclose(pf);	/* handle already closed, ignore error */
	    return 0;
	}
	else if (err == SOCKET_ERROR) {
	    err = WSAGetLastError();
	    if (err != WSAENOTSOCK) {
		(void)fclose(pf);
		errno = err;
		return EOF;
	    }
	}
    }
    return fclose(pf);
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
    dTHXa(NULL);    
    struct servent *r;

    SOCKET_TEST(r = getservbyname(name, proto), NULL);
    if (r) {
        aTHXa(PERL_GET_THX);
	r = win32_savecopyservent(&w32_servent, r, proto);
    }
    return r;
}

struct servent *
win32_getservbyport(int port, const char *proto)
{
    dTHXa(NULL); 
    struct servent *r;

    SOCKET_TEST(r = getservbyport(port, proto), NULL);
    if (r) {
        aTHXa(PERL_GET_THX);
	r = win32_savecopyservent(&w32_servent, r, proto);
    }
    return r;
}

int
win32_ioctl(int i, unsigned int u, char *data)
{
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
    win32_croak_not_implemented("endhostent");
}

void
win32_endnetent()
{
    win32_croak_not_implemented("endnetent");
}

void
win32_endprotoent()
{
    win32_croak_not_implemented("endprotoent");
}

void
win32_endservent()
{
    win32_croak_not_implemented("endservent");
}


struct netent *
win32_getnetent(void) 
{
    win32_croak_not_implemented("getnetent");
    return (struct netent *) NULL;
}

struct netent *
win32_getnetbyname(char *name) 
{
    win32_croak_not_implemented("getnetbyname");
    return (struct netent *)NULL;
}

struct netent *
win32_getnetbyaddr(long net, int type) 
{
    win32_croak_not_implemented("getnetbyaddr");
    return (struct netent *)NULL;
}

struct protoent *
win32_getprotoent(void) 
{
    win32_croak_not_implemented("getprotoent");
    return (struct protoent *) NULL;
}

struct servent *
win32_getservent(void) 
{
    win32_croak_not_implemented("getservent");
    return (struct servent *) NULL;
}

void
win32_sethostent(int stayopen)
{
    win32_croak_not_implemented("sethostent");
}


void
win32_setnetent(int stayopen)
{
    win32_croak_not_implemented("setnetent");
}


void
win32_setprotoent(int stayopen)
{
    win32_croak_not_implemented("setprotoent");
}


void
win32_setservent(int stayopen)
{
    win32_croak_not_implemented("setservent");
}

static struct servent*
win32_savecopyservent(struct servent*d, struct servent*s, const char *proto)
{
    d->s_name = s->s_name;
    d->s_aliases = s->s_aliases;
    d->s_port = s->s_port;
    if (s->s_proto && strlen(s->s_proto))
	d->s_proto = s->s_proto;
    else
    if (proto && strlen(proto))
	d->s_proto = (char *)proto;
    else
	d->s_proto = "tcp";
   
    return d;
}


