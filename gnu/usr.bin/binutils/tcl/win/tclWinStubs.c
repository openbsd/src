/* 
 * tclWinStubs.c --
 *
 *	This file contains wrapper functions for the WinSock API
 *	interfaces so we can indirect through a function table.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinStubs.c 1.2 96/02/01 14:35:05
 */

#include	"tclInt.h"
#include	"tclPort.h"

/*
 * The following variable indicates whether the socket library has been
 * initialized.
 */

static int initialized = 0;

/*
 * The following structure contains pointer to all of the WinSock API entry
 * points used by Tcl.  It is initialized if the winsock.dll is properly loaded
 * by WSAStartup.
 */

static struct {
    SOCKET (PASCAL FAR *accept)(SOCKET s, struct sockaddr FAR *addr, int FAR *addrlen);
    int (PASCAL FAR *bind)(SOCKET s, const struct sockaddr FAR *addr, int namelen);
    int (PASCAL FAR *closesocket)(SOCKET s);
    int (PASCAL FAR *connect)(SOCKET s, const struct sockaddr FAR *name, int namelen);
    int (PASCAL FAR *ioctlsocket)(SOCKET s, long cmd, u_long FAR *argp);
    int (PASCAL FAR *getsockopt)(SOCKET s, int level, int optname, char FAR * optval, int FAR *optlen);
    u_short (PASCAL FAR *htons)(u_short hostshort);
    unsigned long (PASCAL FAR *inet_addr)(const char FAR * cp);
    char FAR * (PASCAL FAR *inet_ntoa)(struct in_addr in);
    int (PASCAL FAR *listen)(SOCKET s, int backlog);
    u_short (PASCAL FAR *ntohs)(u_short netshort);
    int (PASCAL FAR *recv)(SOCKET s, char FAR * buf, int len, int flags);
    int (PASCAL FAR *send)(SOCKET s, const char FAR * buf, int len, int flags);
    int (PASCAL FAR *setsockopt)(SOCKET s, int level, int optname, const char FAR * optval, int optlen);
    SOCKET (PASCAL FAR *socket)(int af, int type, int protocol);
    struct hostent FAR * (PASCAL FAR *gethostbyname)(const char FAR * name);
    int (PASCAL FAR *gethostname)(char FAR * name, int namelen);
    struct servent FAR * (PASCAL FAR *getservbyname)(const char FAR * name, const char FAR * proto);
    int (PASCAL FAR *WSAStartup)(WORD wVersionRequired, LPWSADATA lpWSAData);
    int (PASCAL FAR *WSACleanup)(void);
    int (PASCAL FAR *WSAGetLastError)(void);
    int (PASCAL FAR *WSAAsyncSelect)(SOCKET s, HWND hWnd, u_int wMsg, long lEvent);
} winSock;


/*
 *----------------------------------------------------------------------
 *
 * accept, et al. --
 *
 *	These functions are wrappers that let us bind the WinSock
 *	API dynamically so we can run on systems that don't have
 *	the wsock32.dll.
 *
 * Results:
 *	As defined for each function.
 *
 * Side effects:
 *	As defined for each function.
 *
 *----------------------------------------------------------------------
 */

SOCKET PASCAL FAR
accept(SOCKET s, struct sockaddr FAR *addr, int FAR *addrlen)
{
    return (*winSock.accept)(s, addr, addrlen);
}

int PASCAL FAR
bind(SOCKET s, const struct sockaddr FAR *addr, int namelen)
{
    return (*winSock.bind)(s, addr, namelen);
}

int PASCAL FAR
closesocket(SOCKET s) {
    return (*winSock.closesocket)(s);
}

int PASCAL FAR
connect(SOCKET s, const struct sockaddr FAR *name, int namelen)
{
    return (*winSock.connect)(s, name, namelen);
}

int PASCAL FAR
ioctlsocket(SOCKET s, long cmd, u_long FAR *argp) {
    return (*winSock.ioctlsocket)(s, cmd, argp);
}

int PASCAL FAR
getsockopt(SOCKET s, int level, int optname, char FAR * optval,
	int FAR *optlen)
{
    return (*winSock.getsockopt)(s, level, optname, optval, optlen);
}

u_short PASCAL FAR
htons(u_short hostshort) {
    return (*winSock.htons)(hostshort);
}

unsigned long PASCAL FAR
inet_addr(const char FAR * cp) {
    return (*winSock.inet_addr)(cp);
}

char FAR * PASCAL FAR
inet_ntoa(struct in_addr in) {
    return (*winSock.inet_ntoa)(in);
}

int PASCAL FAR
listen(SOCKET s, int backlog)
{
    return (*winSock.listen)(s, backlog);
}

u_short PASCAL FAR
ntohs(u_short netshort)
{
    return (*winSock.ntohs)(netshort);
}

int PASCAL FAR
recv(SOCKET s, char FAR * buf, int len, int flags)
{
    return (*winSock.recv)(s, buf, len, flags);
}

int PASCAL FAR
send(SOCKET s, const char FAR * buf, int len, int flags)
{
    return (*winSock.send)(s, buf, len, flags);
}

int PASCAL FAR
setsockopt(SOCKET s, int level, int optname, const char FAR * optval,
	int optlen)
{
    return (*winSock.setsockopt)(s, level, optname, optval, optlen);
}

SOCKET PASCAL FAR
socket(int af, int type, int protocol)
{
    return (*winSock.socket)(af, type, protocol);
}

struct hostent FAR * PASCAL FAR
gethostbyname(const char FAR * name)
{
    return (*winSock.gethostbyname)(name);
}

int PASCAL FAR
gethostname(char FAR * name, int namelen)
{
    return (*winSock.gethostname)(name, namelen);
}

struct servent FAR * PASCAL FAR
getservbyname(const char FAR * name, const char FAR * proto)
{
    return (*winSock.getservbyname)(name, proto);
}

/* Microsoft Windows Extension function prototypes */

int PASCAL FAR
WSACleanup(void) {
    return (*winSock.WSACleanup)();
}

int PASCAL FAR
WSAGetLastError(void) {
    return (*winSock.WSAGetLastError)();
}

int PASCAL FAR
WSAAsyncSelect(SOCKET s, HWND hWnd, u_int wMsg, long lEvent) {
    return (*winSock.WSAAsyncSelect)(s, hWnd, wMsg, lEvent);
}


int PASCAL FAR
WSAStartup(WORD wVersionRequired, LPWSADATA lpWSAData) {
    HINSTANCE handle;
    if (!initialized) {
	handle = TclWinLoadLibrary("wsock32.dll");
	if (handle == NULL) {
	    return WSAVERNOTSUPPORTED;
	}
	winSock.accept = (SOCKET (PASCAL FAR *)(SOCKET s, struct sockaddr FAR *addr, int FAR *addrlen)) GetProcAddress(handle, "accept");
	winSock.bind = (int (PASCAL FAR *)(SOCKET s, const struct sockaddr FAR *addr, int namelen)) GetProcAddress(handle, "bind");
	winSock.closesocket = (int (PASCAL FAR *)(SOCKET s)) GetProcAddress(handle, "closesocket");
	winSock.connect = (int (PASCAL FAR *)(SOCKET s, const struct sockaddr FAR *name, int namelen)) GetProcAddress(handle, "connect");
	winSock.ioctlsocket = (int (PASCAL FAR *)(SOCKET s, long cmd, u_long FAR *argp)) GetProcAddress(handle, "ioctlsocket");
	winSock.getsockopt = (int (PASCAL FAR *)(SOCKET s, int level, int optname, char FAR * optval, int FAR *optlen)) GetProcAddress(handle, "getsockopt");
	winSock.htons = (u_short (PASCAL FAR *)(u_short hostshort)) GetProcAddress(handle, "htons");
	winSock.inet_addr = (unsigned long (PASCAL FAR *)(const char FAR * cp)) GetProcAddress(handle, "inet_addr");
	winSock.inet_ntoa = (char FAR * (PASCAL FAR *)(struct in_addr in)) GetProcAddress(handle, "inet_ntoa");
	winSock.listen = (int (PASCAL FAR *)(SOCKET s, int backlog)) GetProcAddress(handle, "listen");
	winSock.ntohs = (u_short (PASCAL FAR *)(u_short netshort)) GetProcAddress(handle, "ntohs");
	winSock.recv = (int (PASCAL FAR *)(SOCKET s, char FAR * buf, int len, int flags)) GetProcAddress(handle, "recv");
	winSock.send = (int (PASCAL FAR *)(SOCKET s, const char FAR * buf, int len, int flags)) GetProcAddress(handle, "send");
	winSock.setsockopt = (int (PASCAL FAR *)(SOCKET s, int level, int optname, const char FAR * optval, int optlen)) GetProcAddress(handle, "setsockopt");
	winSock.socket = (SOCKET (PASCAL FAR *)(int af, int type, int protocol)) GetProcAddress(handle, "socket");
	winSock.gethostbyname = (struct hostent FAR * (PASCAL FAR *)(const char FAR * name)) GetProcAddress(handle, "gethostbyname");
	winSock.gethostname = (int (PASCAL FAR *)(char FAR * name, int namelen)) GetProcAddress(handle, "gethostname");
	winSock.getservbyname = (struct servent FAR * (PASCAL FAR *)(const char FAR * name, const char FAR * proto)) GetProcAddress(handle, "getservbyname");
	winSock.WSAStartup = (int (PASCAL FAR *)(WORD wVersionRequired, LPWSADATA lpWSAData)) GetProcAddress(handle, "WSAStartup");
	winSock.WSACleanup = (int (PASCAL FAR *)(void)) GetProcAddress(handle, "WSACleanup");
	winSock.WSAGetLastError = (int (PASCAL FAR *)(void)) GetProcAddress(handle, "WSAGetLastError");
	winSock.WSAAsyncSelect = (int (PASCAL FAR *)(SOCKET s, HWND hWnd, u_int wMsg, long lEvent)) GetProcAddress(handle, "WSAAsyncSelect");
	initialized = 1;
    }
    return (*winSock.WSAStartup)(wVersionRequired, lpWSAData);
}

