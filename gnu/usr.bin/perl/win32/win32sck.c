/* NTSock.C
 *
 * (c) 1995 Microsoft Corporation. All rights reserved. 
 * 		Developed by hip communications inc., http://info.hip.com/info/
 * Portions (c) 1993 Intergraph Corporation. All rights reserved.
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#include <windows.h>
#define WIN32_LEAN_AND_MEAN
#include "EXTERN.h"
#include "perl.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>

#define CROAK croak

#ifdef USE_SOCKETS_AS_HANDLES
/* thanks to Beverly Brown	(beverly@datacube.com) */

#define OPEN_SOCKET(x)	_open_osfhandle(x,O_RDWR|O_BINARY)
#define TO_SOCKET(x)	_get_osfhandle(x)

#else

#	define OPEN_SOCKET(x)	(x)
#	define TO_SOCKET(x)	(x)

#endif	/* USE_SOCKETS_AS_HANDLES */

static struct servent* win32_savecopyservent(struct servent*d,
                                             struct servent*s,
                                             const char *proto);
#define SOCKETAPI PASCAL 

typedef SOCKET (SOCKETAPI *LPSOCKACCEPT)(SOCKET, struct sockaddr *, int *);
typedef int (SOCKETAPI *LPSOCKBIND)(SOCKET, const struct sockaddr *, int);
typedef int (SOCKETAPI *LPSOCKCLOSESOCKET)(SOCKET);
typedef int (SOCKETAPI *LPSOCKCONNECT)(SOCKET, const struct sockaddr *, int);
typedef int (SOCKETAPI *LPSOCKIOCTLSOCKET)(SOCKET, long, u_long *);
typedef int (SOCKETAPI *LPSOCKGETPEERNAME)(SOCKET, struct sockaddr *, int *);
typedef int (SOCKETAPI *LPSOCKGETSOCKNAME)(SOCKET, struct sockaddr *, int *);
typedef int (SOCKETAPI *LPSOCKGETSOCKOPT)(SOCKET, int, int, char *, int *);
typedef u_long (SOCKETAPI *LPSOCKHTONL)(u_long);
typedef u_short (SOCKETAPI *LPSOCKHTONS)(u_short);
typedef int (SOCKETAPI *LPSOCKLISTEN)(SOCKET, int);
typedef u_long (SOCKETAPI *LPSOCKNTOHL)(u_long);
typedef u_short (SOCKETAPI *LPSOCKNTOHS)(u_short);
typedef int (SOCKETAPI *LPSOCKRECV)(SOCKET, char *, int, int);
typedef int (SOCKETAPI *LPSOCKRECVFROM)(SOCKET, char *, int, int, struct sockaddr *, int *);
typedef int (SOCKETAPI *LPSOCKSELECT)(int, fd_set *, fd_set *, fd_set *, const struct timeval *);
typedef int (SOCKETAPI *LPSOCKSEND)(SOCKET, const char *, int, int);
typedef int (SOCKETAPI *LPSOCKSENDTO)(SOCKET, const char *, int, int, const struct sockaddr *, int);
typedef int (SOCKETAPI *LPSOCKSETSOCKOPT)(SOCKET, int, int, const char *, int);
typedef int (SOCKETAPI *LPSOCKSHUTDOWN)(SOCKET, int);
typedef SOCKET (SOCKETAPI *LPSOCKSOCKET)(int, int, int);
typedef char FAR *(SOCKETAPI *LPSOCKINETNTOA)(struct in_addr in);
typedef unsigned long (SOCKETAPI *LPSOCKINETADDR)(const char FAR * cp);


/* Database function prototypes */
typedef struct hostent *(SOCKETAPI *LPSOCKGETHOSTBYADDR)(const char *, int, int);
typedef struct hostent *(SOCKETAPI *LPSOCKGETHOSTBYNAME)(const char *);
typedef int (SOCKETAPI *LPSOCKGETHOSTNAME)(char *, int);
typedef struct servent *(SOCKETAPI *LPSOCKGETSERVBYPORT)(int, const char *);
typedef struct servent *(SOCKETAPI *LPSOCKGETSERVBYNAME)(const char *, const char *);
typedef struct protoent *(SOCKETAPI *LPSOCKGETPROTOBYNUMBER)(int);
typedef struct protoent *(SOCKETAPI *LPSOCKGETPROTOBYNAME)(const char *);

/* Microsoft Windows Extension function prototypes */
typedef int (SOCKETAPI *LPSOCKWSASTARTUP)(unsigned short, LPWSADATA);
typedef int (SOCKETAPI *LPSOCKWSACLEANUP)(void);
typedef int (SOCKETAPI *LPSOCKWSAGETLASTERROR)(void);
typedef int (SOCKETAPI *LPWSAFDIsSet)(SOCKET, fd_set *);

static HINSTANCE hWinSockDll = 0;
/* extern CRITICAL_SECTION csSock; */

static LPSOCKACCEPT paccept = 0;
static LPSOCKBIND pbind = 0;
static LPSOCKCLOSESOCKET pclosesocket = 0;
static LPSOCKCONNECT pconnect = 0;
static LPSOCKIOCTLSOCKET pioctlsocket = 0;
static LPSOCKGETPEERNAME pgetpeername = 0;
static LPSOCKGETSOCKNAME pgetsockname = 0;
static LPSOCKGETSOCKOPT pgetsockopt = 0;
static LPSOCKHTONL phtonl = 0;
static LPSOCKHTONS phtons = 0;
static LPSOCKLISTEN plisten = 0;
static LPSOCKNTOHL pntohl = 0;
static LPSOCKNTOHS pntohs = 0;
static LPSOCKRECV precv = 0;
static LPSOCKRECVFROM precvfrom = 0;
static LPSOCKSELECT pselect = 0;
static LPSOCKSEND psend = 0;
static LPSOCKSENDTO psendto = 0;
static LPSOCKSETSOCKOPT psetsockopt = 0;
static LPSOCKSHUTDOWN pshutdown = 0;
static LPSOCKSOCKET psocket = 0;
static LPSOCKGETHOSTBYADDR pgethostbyaddr = 0;
static LPSOCKGETHOSTBYNAME pgethostbyname = 0;
static LPSOCKGETHOSTNAME pgethostname = 0;
static LPSOCKGETSERVBYPORT pgetservbyport = 0;
static LPSOCKGETSERVBYNAME pgetservbyname = 0;
static LPSOCKGETPROTOBYNUMBER pgetprotobynumber = 0;
static LPSOCKGETPROTOBYNAME pgetprotobyname = 0;
static LPSOCKWSASTARTUP pWSAStartup = 0;
static LPSOCKWSACLEANUP pWSACleanup = 0;
static LPSOCKWSAGETLASTERROR pWSAGetLastError = 0;
static LPWSAFDIsSet pWSAFDIsSet = 0;
static LPSOCKINETNTOA pinet_ntoa = 0;
static LPSOCKINETADDR pinet_addr = 0;

__declspec(thread) struct servent myservent;


void *
GetAddress(HINSTANCE hInstance, char *lpFunctionName)
{
    FARPROC proc = GetProcAddress(hInstance, lpFunctionName);
    if(proc == 0)
	CROAK("Unable to get address of %s in WSock32.dll", lpFunctionName);
    return proc;
}

void
LoadWinSock(void)
{
/*  EnterCriticalSection(&csSock); */
    if(hWinSockDll == NULL) {
	HINSTANCE hLib = LoadLibrary("WSock32.DLL");
	if(hLib == NULL)
	    CROAK("Could not load WSock32.dll\n");

	paccept = (LPSOCKACCEPT)GetAddress(hLib, "accept");
	pbind = (LPSOCKBIND)GetAddress(hLib, "bind");
	pclosesocket = (LPSOCKCLOSESOCKET)GetAddress(hLib, "closesocket");
	pconnect = (LPSOCKCONNECT)GetAddress(hLib, "connect");
	pioctlsocket = (LPSOCKIOCTLSOCKET)GetAddress(hLib, "ioctlsocket");
	pgetpeername = (LPSOCKGETPEERNAME)GetAddress(hLib, "getpeername");
	pgetsockname = (LPSOCKGETSOCKNAME)GetAddress(hLib, "getsockname");
	pgetsockopt = (LPSOCKGETSOCKOPT)GetAddress(hLib, "getsockopt");
	phtonl = (LPSOCKHTONL)GetAddress(hLib, "htonl");
	phtons = (LPSOCKHTONS)GetAddress(hLib, "htons");
	plisten = (LPSOCKLISTEN)GetAddress(hLib, "listen");
	pntohl = (LPSOCKNTOHL)GetAddress(hLib, "ntohl");
	pntohs = (LPSOCKNTOHS)GetAddress(hLib, "ntohs");
	precv = (LPSOCKRECV)GetAddress(hLib, "recv");
	precvfrom = (LPSOCKRECVFROM)GetAddress(hLib, "recvfrom");
	pselect = (LPSOCKSELECT)GetAddress(hLib, "select");
	psend = (LPSOCKSEND)GetAddress(hLib, "send");
	psendto = (LPSOCKSENDTO)GetAddress(hLib, "sendto");
	psetsockopt = (LPSOCKSETSOCKOPT)GetAddress(hLib, "setsockopt");
	pshutdown = (LPSOCKSHUTDOWN)GetAddress(hLib, "shutdown");
	psocket = (LPSOCKSOCKET)GetAddress(hLib, "socket");
	pgethostbyaddr = (LPSOCKGETHOSTBYADDR)GetAddress(hLib, "gethostbyaddr");
	pgethostbyname = (LPSOCKGETHOSTBYNAME)GetAddress(hLib, "gethostbyname");
	pgethostname = (LPSOCKGETHOSTNAME)GetAddress(hLib, "gethostname");
	pgetservbyport = (LPSOCKGETSERVBYPORT)GetAddress(hLib, "getservbyport");
	pgetservbyname = (LPSOCKGETSERVBYNAME)GetAddress(hLib, "getservbyname");
	pgetprotobynumber = (LPSOCKGETPROTOBYNUMBER)GetAddress(hLib, "getprotobynumber");
	pgetprotobyname = (LPSOCKGETPROTOBYNAME)GetAddress(hLib, "getprotobyname");
	pWSAStartup = (LPSOCKWSASTARTUP)GetAddress(hLib, "WSAStartup");
	pWSACleanup = (LPSOCKWSACLEANUP)GetAddress(hLib, "WSACleanup");
	pWSAGetLastError = (LPSOCKWSAGETLASTERROR)GetAddress(hLib, "WSAGetLastError");
	pWSAFDIsSet = (LPWSAFDIsSet)GetAddress(hLib, "__WSAFDIsSet");
	pinet_addr = (LPSOCKINETADDR)GetAddress(hLib,"inet_addr");
	pinet_ntoa = (LPSOCKINETNTOA)GetAddress(hLib,"inet_ntoa");

	hWinSockDll = hLib;
    }
/*  LeaveCriticalSection(&csSock); */
}

void
EndSockets(void)
{
    if(hWinSockDll != NULL) {
	pWSACleanup();
	FreeLibrary(hWinSockDll);
    }
    hWinSockDll = NULL;
}

void
StartSockets(void) 
{
    unsigned short version;
    WSADATA retdata;
    int ret;
    int iSockOpt = SO_SYNCHRONOUS_NONALERT;

    LoadWinSock();
    /*
     * initalize the winsock interface and insure that it is
     * cleaned up at exit.
     */
    version = 0x101;
    if(ret = pWSAStartup(version, &retdata))
	CROAK("Unable to locate winsock library!\n");
    if(retdata.wVersion != version)
	CROAK("Could not find version 1.1 of winsock dll\n");

    /* atexit((void (*)(void)) EndSockets); */

#ifdef USE_SOCKETS_AS_HANDLES
    /*
     * Enable the use of sockets as filehandles
     */
    psetsockopt(INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE,
		(char *)&iSockOpt, sizeof(iSockOpt));
#endif	/* USE_SOCKETS_AS_HANDLES */
}


#ifndef USE_SOCKETS_AS_HANDLES
FILE *
myfdopen(int fd, char *mode)
{
    FILE *fp;
    char sockbuf[256];
    int optlen = sizeof(sockbuf);
    int retval;

    if (hWinSockDll == 0)
	return(fdopen(fd, mode));

    retval = pgetsockopt((SOCKET)fd, SOL_SOCKET, SO_TYPE, sockbuf, &optlen);
    if(retval == SOCKET_ERROR && pWSAGetLastError() == WSAENOTSOCK) {
	return(fdopen(fd, mode));
    }

    /*
     * If we get here, then fd is actually a socket.
     */
    Newz(1310, fp, 1, FILE);
    if(fp == NULL) {
	errno = ENOMEM;
	return NULL;
    }

    fp->_file = fd;
    if(*mode == 'r')
	fp->_flag = _IOREAD;
    else
	fp->_flag = _IOWRT;
   
    return fp;
}
#endif	/* USE_SOCKETS_AS_HANDLES */


u_long
win32_htonl(u_long hostlong)
{
    if(hWinSockDll == 0)
	StartSockets();

    return phtonl(hostlong);
}

u_short
win32_htons(u_short hostshort)
{
    if(hWinSockDll == 0)
	StartSockets();

    return phtons(hostshort);
}

u_long
win32_ntohl(u_long netlong)
{
    if(hWinSockDll == 0)
	StartSockets();

    return pntohl(netlong);
}

u_short
win32_ntohs(u_short netshort)
{
    if(hWinSockDll == 0)
	StartSockets();

    return pntohs(netshort);
}


#define SOCKET_TEST(x, y)	if(hWinSockDll == 0) StartSockets();\
				if((x) == (y)) errno = pWSAGetLastError()

#define SOCKET_TEST_ERROR(x) SOCKET_TEST(x, SOCKET_ERROR)

SOCKET
win32_accept(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    SOCKET r;

    SOCKET_TEST((r = paccept(TO_SOCKET(s), addr, addrlen)), INVALID_SOCKET);
    return OPEN_SOCKET(r);
}

int
win32_bind(SOCKET s, const struct sockaddr *addr, int addrlen)
{
    int r;

    SOCKET_TEST_ERROR(r = pbind(TO_SOCKET(s), addr, addrlen));
    return r;
}

int
win32_connect(SOCKET s, const struct sockaddr *addr, int addrlen)
{
    int r;

    SOCKET_TEST_ERROR(r = pconnect(TO_SOCKET(s), addr, addrlen));
    return r;
}


int
win32_getpeername(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    int r;

    SOCKET_TEST_ERROR(r = pgetpeername(TO_SOCKET(s), addr, addrlen));
    return r;
}

int
win32_getsockname(SOCKET s, struct sockaddr *addr, int *addrlen)
{
    int r;

    SOCKET_TEST_ERROR(r = pgetsockname(TO_SOCKET(s), addr, addrlen));
    return r;
}

int
win32_getsockopt(SOCKET s, int level, int optname, char *optval, int *optlen)
{
    int r;

    SOCKET_TEST_ERROR(r = pgetsockopt(TO_SOCKET(s), level, optname, optval, optlen));
    return r;
}

int
win32_ioctlsocket(SOCKET s, long cmd, u_long *argp)
{
    int r;

    SOCKET_TEST_ERROR(r = pioctlsocket(TO_SOCKET(s), cmd, argp));
    return r;
}

int
win32_listen(SOCKET s, int backlog)
{
    int r;

    SOCKET_TEST_ERROR(r = plisten(TO_SOCKET(s), backlog));
    return r;
}

int
win32_recv(SOCKET s, char *buf, int len, int flags)
{
    int r;

    SOCKET_TEST_ERROR(r = precv(TO_SOCKET(s), buf, len, flags));
    return r;
}

int
win32_recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
    int r;

    SOCKET_TEST_ERROR(r = precvfrom(TO_SOCKET(s), buf, len, flags, from, fromlen));
    return r;
}

/* select contributed by Vincent R. Slyngstad (vrs@ibeam.intel.com) */
int
win32_select(int nfds, int* rd, int* wr, int* ex, const struct timeval* timeout)
{
    long r;
    int dummy = 0;
    int i, fd, bit, offset;
    FD_SET nrd, nwr, nex,*prd,*pwr,*pex;

    if (!rd)
	rd = &dummy, prd = NULL;
    else
	prd = &nrd;
    if (!wr)
	wr = &dummy, pwr = NULL;
    else
	pwr = &nwr;
    if (!ex)
	ex = &dummy, pex = NULL;
    else
	pex = &nex;

    FD_ZERO(&nrd);
    FD_ZERO(&nwr);
    FD_ZERO(&nex);
    for (i = 0; i < nfds; i++) {
	fd = TO_SOCKET(i);
	bit = 1L<<(i % (sizeof(int)*8));
	offset = i / (sizeof(int)*8);
	if (rd[offset] & bit)
	    FD_SET(fd, &nrd);
	if (wr[offset] & bit)
	    FD_SET(fd, &nwr);
	if (ex[offset] & bit)
	    FD_SET(fd, &nex);
    }

    SOCKET_TEST_ERROR(r = pselect(nfds, prd, pwr, pex, timeout));

    for (i = 0; i < nfds; i++) {
	fd = TO_SOCKET(i);
	bit = 1L<<(i % (sizeof(int)*8));
	offset = i / (sizeof(int)*8);
	if (rd[offset] & bit) {
	    if (!pWSAFDIsSet(fd, &nrd))
		rd[offset] &= ~bit;
	}
	if (wr[offset] & bit) {
	    if (!pWSAFDIsSet(fd, &nwr))
		wr[offset] &= ~bit;
	}
	if (ex[offset] & bit) {
	    if (!pWSAFDIsSet(fd, &nex))
		ex[offset] &= ~bit;
	}
    }
    return r;
}

int
win32_send(SOCKET s, const char *buf, int len, int flags)
{
    int r;

    SOCKET_TEST_ERROR(r = psend(TO_SOCKET(s), buf, len, flags));
    return r;
}

int
win32_sendto(SOCKET s, const char *buf, int len, int flags,
	     const struct sockaddr *to, int tolen)
{
    int r;

    SOCKET_TEST_ERROR(r = psendto(TO_SOCKET(s), buf, len, flags, to, tolen));
    return r;
}

int
win32_setsockopt(SOCKET s, int level, int optname, const char *optval, int optlen)
{
    int r;

    SOCKET_TEST_ERROR(r = psetsockopt(TO_SOCKET(s), level, optname, optval, optlen));
    return r;
}
    
int
win32_shutdown(SOCKET s, int how)
{
    int r;

    SOCKET_TEST_ERROR(r = pshutdown(TO_SOCKET(s), how));
    return r;
}

SOCKET
win32_socket(int af, int type, int protocol)
{
    SOCKET s;

#ifndef USE_SOCKETS_AS_HANDLES
    SOCKET_TEST(s = psocket(af, type, protocol), INVALID_SOCKET);
#else
    if(hWinSockDll == 0)
	StartSockets();

    if((s = psocket(af, type, protocol)) == INVALID_SOCKET)
	errno = pWSAGetLastError();
    else
	s = OPEN_SOCKET(s);
#endif	/* USE_SOCKETS_AS_HANDLES */

    return s;
}

#undef fclose
int
my_fclose (FILE *pf)
{
	int osf, retval;
	if (hWinSockDll == 0)		/* No WinSockDLL? */
		return(fclose(pf));	/* Then not a socket. */
	osf = TO_SOCKET(fileno(pf));	/* Get it now before it's gone! */
	retval = fclose(pf);		/* Must fclose() before closesocket() */
	if (osf != -1
	    && pclosesocket(osf) == SOCKET_ERROR
	    && WSAGetLastError() != WSAENOTSOCK)
		retval = EOF;
	return retval;
}

struct hostent *
win32_gethostbyaddr(const char *addr, int len, int type)
{
    struct hostent *r;

    SOCKET_TEST(r = pgethostbyaddr(addr, len, type), NULL);
    return r;
}

struct hostent *
win32_gethostbyname(const char *name)
{
    struct hostent *r;

    SOCKET_TEST(r = pgethostbyname(name), NULL);
    return r;
}

int
win32_gethostname(char *name, int len)
{
    int r;

    SOCKET_TEST_ERROR(r = pgethostname(name, len));
    return r;
}

struct protoent *
win32_getprotobyname(const char *name)
{
    struct protoent *r;

    SOCKET_TEST(r = pgetprotobyname(name), NULL);
    return r;
}

struct protoent *
win32_getprotobynumber(int num)
{
    struct protoent *r;

    SOCKET_TEST(r = pgetprotobynumber(num), NULL);
    return r;
}

struct servent *
win32_getservbyname(const char *name, const char *proto)
{
    struct servent *r;
   
    SOCKET_TEST(r = pgetservbyname(name, proto), NULL);
    if (r) {
	r = win32_savecopyservent(&myservent, r, proto);
    }
    return r;
}

struct servent *
win32_getservbyport(int port, const char *proto)
{
    struct servent *r;

    SOCKET_TEST(r = pgetservbyport(port, proto), NULL);
    if (r) {
	r = win32_savecopyservent(&myservent, r, proto);
    }
    return r;
}

char FAR *
win32_inet_ntoa(struct in_addr in)
{
    if(hWinSockDll == 0)
	StartSockets();

    return pinet_ntoa(in);
}

unsigned long
win32_inet_addr(const char FAR *cp)
{
    if(hWinSockDll == 0)
	StartSockets();

    return pinet_addr(cp);

}

/*
 * Networking stubs
 */
#undef CROAK 
#define CROAK croak

void
win32_endhostent() 
{
    CROAK("endhostent not implemented!\n");
}

void
win32_endnetent()
{
    CROAK("endnetent not implemented!\n");
}

void
win32_endprotoent()
{
    CROAK("endprotoent not implemented!\n");
}

void
win32_endservent()
{
    CROAK("endservent not implemented!\n");
}


struct netent *
win32_getnetent(void) 
{
    CROAK("getnetent not implemented!\n");
    return (struct netent *) NULL;
}

struct netent *
win32_getnetbyname(char *name) 
{
    CROAK("getnetbyname not implemented!\n");
    return (struct netent *)NULL;
}

struct netent *
win32_getnetbyaddr(long net, int type) 
{
    CROAK("getnetbyaddr not implemented!\n");
    return (struct netent *)NULL;
}

struct protoent *
win32_getprotoent(void) 
{
    CROAK("getprotoent not implemented!\n");
    return (struct protoent *) NULL;
}

struct servent *
win32_getservent(void) 
{
    CROAK("getservent not implemented!\n");
    return (struct servent *) NULL;
}

void
win32_sethostent(int stayopen)
{
    CROAK("sethostent not implemented!\n");
}


void
win32_setnetent(int stayopen)
{
    CROAK("setnetent not implemented!\n");
}


void
win32_setprotoent(int stayopen)
{
    CROAK("setprotoent not implemented!\n");
}


void
win32_setservent(int stayopen)
{
    CROAK("setservent not implemented!\n");
}

#define WIN32IO_IS_STDIO
#include <io.h>
#include "win32iop.h"

static struct servent*
win32_savecopyservent(struct servent*d, struct servent*s, const char *proto)
{
    d->s_name = s->s_name;
    d->s_aliases = s->s_aliases;
    d->s_port = s->s_port;
#ifndef __BORLANDC__	/* Buggy on Win95 and WinNT-with-Borland-WSOCK */
    if (!IsWin95() && s->s_proto && strlen(s->s_proto))
	d->s_proto = s->s_proto;
    else
#endif
	if (proto && strlen(proto))
	d->s_proto = (char *)proto;
    else
	d->s_proto = "tcp";
   
    return d;
}


