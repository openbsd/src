// sys/socket.h

// djl
// Provide UNIX compatibility

#ifndef  _INC_SYS_SOCKET
#define  _INC_SYS_SOCKET

#ifdef __cplusplus
extern "C" {
#endif

#ifndef  _WINDOWS_
#define  _WINDOWS_

#define  FAR
#define  PASCAL     __stdcall
#define  WINAPI     __stdcall

#undef WORD
typedef  int        BOOL;
typedef  unsigned short WORD;
typedef  void*      HANDLE;
typedef  void*      HWND;
typedef  int (FAR WINAPI *FARPROC)();

typedef unsigned long       DWORD;
typedef void *PVOID;

#define IN
#define OUT

typedef struct _OVERLAPPED {
    DWORD   Internal;
    DWORD   InternalHigh;
    DWORD   Offset;
    DWORD   OffsetHigh;
    HANDLE  hEvent;
} OVERLAPPED, *LPOVERLAPPED;

#endif //_WINDOWS_
#include <winsock.h>

#define  ENOTSOCK	WSAENOTSOCK
#undef   HOST_NOT_FOUND


SOCKET win32_accept (SOCKET s, struct sockaddr *addr, int *addrlen);
int win32_bind (SOCKET s, const struct sockaddr *addr, int namelen);
int win32_closesocket (SOCKET s);
int win32_connect (SOCKET s, const struct sockaddr *name, int namelen);
int win32_ioctlsocket (SOCKET s, long cmd, u_long *argp);
int win32_getpeername (SOCKET s, struct sockaddr *name, int * namelen);
int win32_getsockname (SOCKET s, struct sockaddr *name, int * namelen);
int win32_getsockopt (SOCKET s, int level, int optname, char * optval, int *optlen);
u_long win32_htonl (u_long hostlong);
u_short win32_htons (u_short hostshort);
unsigned long win32_inet_addr (const char * cp);
char * win32_inet_ntoa (struct in_addr in);
int win32_listen (SOCKET s, int backlog);
u_long win32_ntohl (u_long netlong);
u_short win32_ntohs (u_short netshort);
int win32_recv (SOCKET s, char * buf, int len, int flags);
int win32_recvfrom (SOCKET s, char * buf, int len, int flags,
                         struct sockaddr *from, int * fromlen);
int win32_select (int nfds, int *readfds, int *writefds, int *exceptfds, const struct timeval *timeout);
int win32_send (SOCKET s, const char * buf, int len, int flags);
int win32_sendto (SOCKET s, const char * buf, int len, int flags,
                       const struct sockaddr *to, int tolen);
int win32_setsockopt (SOCKET s, int level, int optname,
                           const char * optval, int optlen);
SOCKET win32_socket (int af, int type, int protocol);
int win32_shutdown (SOCKET s, int how);

/* Database function prototypes */

struct hostent * win32_gethostbyaddr(const char * addr, int len, int type);
struct hostent * win32_gethostbyname(const char * name);
int win32_gethostname (char * name, int namelen);
struct servent * win32_getservbyport(int port, const char * proto);
struct servent * win32_getservbyname(const char * name, const char * proto);
struct protoent * win32_getprotobynumber(int proto);
struct protoent * win32_getprotobyname(const char * name);
struct protoent *win32_getprotoent(void);
struct servent *win32_getservent(void);
void win32_sethostent(int stayopen);
void win32_setnetent(int stayopen);
struct netent * win32_getnetent(void);
struct netent * win32_getnetbyname(char *name);
struct netent * win32_getnetbyaddr(long net, int type);
void win32_setprotoent(int stayopen);
void win32_setservent(int stayopen);
void win32_endhostent(void);
void win32_endnetent(void);
void win32_endprotoent(void);
void win32_endservent(void);

//
// direct to our version
//
#define htonl		win32_htonl
#define htons		win32_htons
#define ntohl		win32_ntohl
#define ntohs		win32_ntohs
#define inet_addr	win32_inet_addr
#define inet_ntoa	win32_inet_ntoa

#define socket		win32_socket
#define bind		win32_bind
#define listen		win32_listen
#define accept		win32_accept
#define connect		win32_connect
#define send		win32_send
#define sendto		win32_sendto
#define recv		win32_recv
#define recvfrom	win32_recvfrom
#define shutdown	win32_shutdown
#define ioctlsocket	win32_ioctlsocket
#define setsockopt	win32_setsockopt
#define getsockopt	win32_getsockopt
#define getpeername	win32_getpeername
#define getsockname	win32_getsockname
#define gethostname	win32_gethostname
#define gethostbyname	win32_gethostbyname
#define gethostbyaddr	win32_gethostbyaddr
#define getprotobyname	win32_getprotobyname
#define getprotobynumber win32_getprotobynumber
#define getservbyname	win32_getservbyname
#define getservbyport	win32_getservbyport
#define select		win32_select
#define endhostent	win32_endhostent
#define endnetent	win32_endnetent
#define endprotoent	win32_endprotoent
#define endservent	win32_endservent
#define getnetent	win32_getnetent
#define getnetbyname	win32_getnetbyname
#define getnetbyaddr	win32_getnetbyaddr
#define getprotoent	win32_getprotoent
#define getservent	win32_getservent
#define sethostent	win32_sethostent
#define setnetent	win32_setnetent
#define setprotoent	win32_setprotoent
#define setservent	win32_setservent

#ifdef __cplusplus
}
#endif

#endif	// _INC_SYS_SOCKET
