/*                System dependencies in the W3 library
                                   SYSTEM DEPENDENCIES

   System-system differences for TCP include files and macros.  This
   file includes for each system the files necessary for network and
   file I/O.  It should be used in conjunction with HTUtils.h to help
   ensure portability across as many platforms and flavors of platforms
   as possible.

  AUTHORS

  TBL                Tim Berners-Lee, W3 project, CERN, <timbl@info.cern.ch>
  EvA                     Eelco van Asperen <evas@cs.few.eur.nl>
  MA                      Marc Andreessen NCSA
  AT                      Aleksandar Totic <atotic@ncsa.uiuc.edu>
  SCW                     Susan C. Weber <sweber@kyle.eitech.com>

  HISTORY:
  22 Feb 91               Written (TBL) as part of the WWW library.
  16 Jan 92               PC code from EvA
  22 Apr 93               Merged diffs bits from xmosaic release
  29 Apr 93               Windows/NT code from SCW
  20 May 94		  A.Harper Add support for VMS CMU TCP/IP transport
   3 Oct 94		  A.Harper Add support for VMS SOCKETSHR/NETLIB
  15 Jul 95               S. Bjorndahl Gnu C for VMS Globaldef/ref support

*/

#ifndef TCP_H
#define TCP_H

/*

Default values

   These values may be reset and altered by system-specific sections
   later on.  there are also a bunch of defaults at the end .

 */
/* Default values of those: */
#define NETCLOSE close      /* Routine to close a TCP-IP socket         */
#define NETREAD  HTDoRead       /* Routine to read from a TCP-IP socket     */
#define NETWRITE write      /* Routine to write to a TCP-IP socket      */
#define SOCKET_READ read    /* normal socket read routine */
#define IOCTL ioctl	    /* normal ioctl routine for sockets */
#define SOCKET_ERRNO errno	    /* normal socket errno */

/* Unless stated otherwise, */
#define SELECT                  /* Can handle >1 channel.               */
#define GOT_SYSTEM              /* Can call shell with string           */

#ifdef UNIX
#define GOT_PIPE
#endif /* UNIX */

#define INVSOC (-1)             /* Unix invalid socket */
		/* NB: newer libwww has something different for Windows */

#ifndef VMS

#include <sys/types.h>

#if defined(__BORLANDC__)
#define DECL_ERRNO
#endif

#if defined(__DJGPP__) || defined(__BORLANDC__)
#undef HAVE_DIRENT_H
#define HAVE_DIRENT_H
#undef HAVE_SYS_FILIO_H
#endif /* DJGPP or __BORLANDC__ */

#if defined(_MSC_VER)
#undef HAVE_DIRENT_H
#define HAVE_DIRENT_H
#undef HAVE_SYS_FILIO_H
#endif /* _MSC_VER */

#ifdef HAVE_DIRENT_H
# include <dirent.h>
# define D_NAMLEN(dirent) strlen((dirent)->d_name)
# define STRUCT_DIRENT struct dirent
#else
# define D_NAMLEN(dirent) (dirent)->d_namlen
# define STRUCT_DIRENT struct direct
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif /* HAVE_DIRENT_H */
#endif /* !VMS */

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if defined(_AIX) && !defined(AIX)
#define AIX
#endif /* _AIX */

#ifdef __CYGWIN__
#define _WINDOWS_NSL
#define WIN_EX
#else
#ifdef WIN_EX
#define HAVE_FTIME 1
#define HAVE_SYS_TIMEB_H 1
#endif
#endif /* __CYGWIN__ */

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif
#endif

#ifdef HAVE_STRING_H
#include <string.h>             /* For bzero etc */
#endif /* HAVE_STRING_H */

/*

  MACROS FOR CONVERTING CHARACTERS

 */
#ifndef TOASCII
#ifdef EBCDIC  /* S/390 -- gil -- 1327 */

extern       char un_IBM1047[];
extern unsigned char IBM1047[];
/* For debugging
#include <assert.h>
#define   TOASCII(c) (assert((c)>=0 && (c)<256), un_IBM1047[c])
*/ /* for production */
#define   TOASCII(c) (un_IBM1047[c])

#define FROMASCII(c) (IBM1047[c])

#else  /* EBCDIC */

#if '0' != 48
 error Host character set is not ASCII.
#endif

#define TOASCII(c) (c)
#define FROMASCII(c) (c)

#endif /* EBCDIC */
#endif /* !TOASCII */

/* convert a char to an unsigned, needed if we have signed characters for ctype.h */
#define UCH(ch) ((unsigned char)(ch))

/*
IBM-PC running Windows NT

	These parameters providede by  Susan C. Weber <sweber@kyle.eitech.com>.
*/

#ifdef _WINDOWS
#define _WINDOWS_NSL
#include <fcntl.h>                      /* For HTFile.c */
#include <sys\types.h>                  /* For HTFile.c */
#include <sys\stat.h>                   /* For HTFile.c */
#undef NETREAD
#undef NETWRITE
#undef NETCLOSE
#undef IOCTL
extern int ws_netread(int fd, char *buf, int len);
#define NETREAD(s,b,l)  ws_netread((s),(b),(l))	/* 1997/11/06 (Thu) */
#define NETWRITE(s,b,l) send((s),(b),(l),0)
#define NETCLOSE(s)     closesocket(s)
#define IOCTL				ioctlsocket
#include <io.h>
#include <string.h>
#include <process.h>
#include <time.h>
#include <errno.h>
#include <direct.h>

#ifdef USE_WINSOCK2_H
#include <winsock2.h>		/* normally included in windows.h */

#undef EINPROGRESS
#undef EALREADY
#undef EISCONN
#undef EINTR
#undef EAGAIN
#undef ENOTCONN
#undef ECONNRESET
#undef ETIMEDOUT

#define EINPROGRESS  WSAEINPROGRESS
#define EALREADY     WSAEALREADY
#define EISCONN      WSAEISCONN
#define EINTR        WSAEINTR
/* fine EAGAIN       WSAEAGAIN */
#define ENOTCONN     WSAENOTCONN
#define ECONNRESET   WSAECONNRESET
#define ETIMEDOUT    WSAETIMEDOUT

#else /* USE_WINSOCK_H */

#include <winsock.h>
typedef struct sockaddr_in SockA;  /* See netinet/in.h */

#if defined(_MSC_VER) || defined(__MINGW32__)
#undef EINTR
#undef EAGAIN
#endif /* _MSC_VER */

#define EINPROGRESS          (WSABASEERR+36)
#define EALREADY             (WSABASEERR+37)
#define EISCONN              (WSABASEERR+56)
#define EINTR                (WSABASEERR+4)
#define EAGAIN               (WSABASEERR+1002)
#define ENOTCONN             (WSABASEERR+57)
#define ECONNRESET           (WSABASEERR+54)
#define ETIMEDOUT             WSAETIMEDOUT	/* 1997/11/10 (Mon) */

#undef  SOCKET_ERRNO	/* 1997/10/19 (Sun) 18:01:46 */
#define SOCKET_ERRNO          WSAGetLastError()

#endif	/* USE_WINSOCK_H */

#define INCLUDES_DONE
#define TCP_INCLUDES_DONE
#endif  /* WINDOWS */



/*

VAX/VMS

   Under VMS, there are many versions of TCP-IP. Define one if you do
   not use Digital's UCX product:

  UCX                     DEC's "Ultrix connection" (default)
  CMU_TCP                 Available via FTP from sacusr.mp.usbr.gov
  SOCKETSHR		  Eckhart Meyer's interface to NETLIB
  WIN_TCP                 From Wollongong, now GEC software.
  MULTINET                From SRI, became TGV, then Cisco.
  DECNET                  Cern's TCP socket emulation over DECnet

   The last three do not interfere with the
   unix i/o library, and so they need special calls to read, write and
   close sockets.  In these cases the socket number is a VMS channel
   number, so we make the @@@ HORRIBLE @@@ assumption that a channel
   number will be greater than 10 but a unix file descriptor less than
   10.  It works.

 */
#ifdef VMS

#ifdef UCX
#undef IOCTL
#define IOCTL HTioctl
#endif /* UCX */

#ifdef WIN_TCP
#undef SOCKET_READ
#undef NETWRITE
#undef NETCLOSE
#define SOCKET_READ(s,b,l)  ((s)>10 ? netread((s),(b),(l)) : read((s),(b),(l)))
#define NETWRITE(s,b,l) ((s)>10 ? netwrite((s),(b),(l)) : write((s),(b),(l)))
#define NETCLOSE(s)     ((s)>10 ? netclose(s) : close(s))
#undef IOCTL
#define IOCTL(a,b,c) -1 /* disables ioctl function	      */
#define NO_IOCTL	/* flag to check if ioctl is disabled */
#endif /* WIN_TCP */

#ifdef CMU_TCP
#undef SOCKET_READ
#undef NETREAD
#undef NETWRITE
#undef NETCLOSE
#define SOCKET_READ(s,b,l) (cmu_get_sdc((s)) != 0 ? cmu_read((s),(b),(l)) : read((s),(b),(l)))
#define NETREAD(s,b,l) (cmu_get_sdc((s)) != 0 ? HTDoRead((s),(b),(l)) : read((s),(b),(l)))
#define NETWRITE(s,b,l) (cmu_get_sdc((s)) != 0 ? cmu_write((s),(b),(l)) : write((s),(b),(l)))
#define NETCLOSE(s) (cmu_get_sdc((s)) != 0 ? cmu_close((s)) : close((s)))
#endif /* CMU_TCP */

#ifdef MULTINET
#undef NETCLOSE
#undef SOCKET_READ
#undef NETWRITE
#undef IOCTL
#undef SOCKET_ERRNO
/*
**  Delete these socket_foo() prototypes as MultiNet adds them
**  to it's socket library headers.  Compiler warnings due to
**  the absence of arguments in the generic prototypes here will
**  include the names of those which can be deleted. - FM
*/
extern int socket_read();
extern int socket_write();
extern int socket_close();
extern int socket_ioctl();

#define SOCKET_READ(s,b,l)  ((s)>10 ? socket_read((s),(b),(l)) : \
				read((s),(b),(l)))
#define NETWRITE(s,b,l) ((s)>10 ? socket_write((s),(b),(l)) : \
                                write((s),(b),(l)))
#define NETCLOSE(s)     ((s)>10 ? socket_close(s) : close(s))
#define IOCTL socket_ioctl
#define SOCKET_ERRNO socket_errno
#endif /* MULTINET */

#ifdef SOCKETSHR_TCP
#undef SOCKET_READ
#undef NETREAD
#undef NETWRITE
#undef NETCLOSE
#undef IOCTL
#define SOCKET_READ(s,b,l) (si_get_sdc((s)) != 0 ? si_read((s),(b),(l)) : \
                                read((s),(b),(l)))
#define NETREAD(s,b,l) (si_get_sdc((s)) != 0 ? HTDoRead((s),(b),(l)) : \
                                read((s),(b),(l)))
#define NETWRITE(s,b,l) (si_get_sdc((s)) != 0 ? si_write((s),(b),(l)) : \
                                write((s),(b),(l)))
#define NETCLOSE(s) (si_get_sdc((s)) != 0 ? si_close((s)) : close((s)))
#define IOCTL si_ioctl
#endif /* SOCKETSHR_TCP */

#include <string.h>

#include <file.h>
#include <stat.h>
#include <unixio.h>
#include <unixlib.h>

#define INCLUDES_DONE

#ifdef MULTINET  /* Include from standard Multinet directories */
/*
**  Delete any of these multinet_foo() and associated prototypes
**  as MultiNet adds them to its socket library headers.  You'll
**  get compiler warnings about them, due the absence of arguments
**  in the generic prototyping here, and the warnings will include
**  the names of the functions whose prototype entries can be
**  deleted here. - FM
*/
extern int multinet_accept();
extern int multinet_bind();
extern int multinet_connect();
extern int multinet_gethostname();
extern int multinet_getsockname();
extern unsigned short multinet_htons(unsigned short __val);
extern unsigned short multinet_ntohs(unsigned short __val);
extern int multinet_listen();
extern int multinet_select();
extern int multinet_socket();
extern char *vms_errno_string();

#ifndef __SOCKET_TYPEDEFS
#define __SOCKET_TYPEDEFS 1
#endif /* !__SOCKET_TYPEDEFS */
#include <time.h>
#include <types.h>
#ifdef __TIME_T
#undef  __TYPES
#define __TYPES 1
#define __TYPES_LOADED 1
#endif /* __TIME_T */
#ifdef __SOCKET_TYPEDEFS
#undef __SOCKET_TYPEDEFS
#endif /* __SOCKET_TYPEDEFS */
#include "multinet_root:[multinet.include.sys]types.h"
#ifndef __SOCKET_TYPEDEFS
#define __SOCKET_TYPEDEFS 1
#endif /* !__SOCKET_TYPEDEFS */
#include "multinet_root:[multinet.include]errno.h"
#ifdef __TYPES
#undef  __TIME_T
#define __TIME_T 1
#endif /* __TYPE */
#ifdef __TIME_LOADED
#undef  __TIME
#define __TIME 1  /* to avoid double definitions in in.h */
#endif /* __TIME_LOADED */
#include "multinet_root:[multinet.include.sys]time.h"
#define MULTINET_NO_PROTOTYPES	/* DECC is compatible-but-different */
#include "multinet_root:[multinet.include.sys]socket.h"
#undef MULTINET_NO_PROTOTYPES
#include "multinet_root:[multinet.include.netinet]in.h"
#include "multinet_root:[multinet.include.arpa]inet.h"
#include "multinet_root:[multinet.include]netdb.h"
#include "multinet_root:[multinet.include.sys]ioctl.h"
#define TCP_INCLUDES_DONE
/*
**  Uncomment this if you get compiler messages
**  about struct timeval having no linkage. - FM
*/
/*#define NO_TIMEVAL*/
#ifdef NO_TIMEVAL
struct timeval {
    long tv_sec;		/* seconds since Jan. 1, 1970 */
    long tv_usec;		/* microseconds */
};
#endif /* NO_TIMEVAL */
#endif /* MULTINET */


#ifdef DECNET
#include <types.h>
#include <errno.h>
#include <time.h>
#include <types.h>  /* for socket.h */
#include <socket.h>
#include <dn>
#include <dnetdb>
/* #include "vms.h" */
#define TCP_INCLUDES_DONE
#endif /* DECNET */


#ifdef UCX
#include <types.h>
#include <errno.h>
#include <time.h>
#include <socket.h>
#include <in.h>
#include <inet.h>
#if defined(TCPWARE) && !defined(__DECC)
#include "tcpware_include:netdb.h"
#include "tcpware_include:ucx$inetdef.h"
#else
#include <netdb.h>
#include <ucx$inetdef.h>
#endif /* TCPWARE */
#define TCP_INCLUDES_DONE
#endif /* UCX */


#ifdef CMU_TCP
#include <types.h>
#include <errno.h>
#include "cmuip_root:[syslib]time.h"
#include "cmuip_root:[syslib]socket.h"
#include <in.h>
#include <inet.h>
#include <netdb.h>
#include "cmuip_root:[syslib]ioctl.h"
#define TCP_INCLUDES_DONE
#endif /* CMU_TCP */


#ifdef SOCKETSHR_TCP
#include <types.h>
#include <errno.h>
#include <time.h>
#include <socket.h>
#include <in.h>
#include <inet.h>
#include <netdb.h>
#include "socketshr_library:socketshr.h"
#include "socketshr_library:ioctl.h"
#define TCP_INCLUDES_DONE
#endif /* SOCKETSHR_TCP */

#ifdef WIN_TCP
#include <types.h>
#include <errno.h>
#include <time.h>
#include <socket.h>
#include <in.h>
#include <inet.h>
#include <netdb.h>
#ifndef NO_IOCTL
#include <ioctl.h>
#endif /* !NO_IOCTL */
#define TCP_INCLUDES_DONE
#endif /* WIN_TCP */

#ifndef TCP_INCLUDES_DONE
#include <types.h>
#include <errno.h>
#include <time.h>
#ifdef VMS_SOCKET_HEADERS
/*
**  Not all versions of VMS have the full set of headers
**  for socket library functions, because the TCP/IP
**  packages were layered products.  If we want these
**  specifically, instead of those for the above packages,
**  the module should be compiled with VMS_SOCKET_HEADERS
**  defined instead of layered product definitions, above.
**  If the module is not using socket library functions,
**  none of the definitions need be used, and we include
**  only the above three headers. - FM
*/
#include <socket.h>
#include <in.h>
#include <inet.h>
#include <netdb.h>
#include <ioctl.h>
#endif /* VMS_SOCKET_HEADERS */
#define TCP_INCLUDES_DONE
#endif /* !TCP_INCLUDES_DONE */

/*

   On VMS machines, the linker needs to be told to put global data sections into
 a data
   segment using these storage classes. (MarkDonszelmann)

 */
#if defined(VAXC) && !defined(__DECC)
#define GLOBALDEF globaldef
#define GLOBALREF globalref
#else
#ifdef __GNUC__		/* this added by Sterling Bjorndahl */
#define GLOBALREF_IS_MACRO 1
#define GLOBALDEF_IS_MACRO 1
#include <gnu_hacks.h>	/* defines GLOBALREF and GLOBALDEF for GNUC on VMS */
#endif  /* __GNUC__ */
#endif /* VAXC && !DECC */

#include <perror.h>
#ifndef errno
extern int errno;
#endif /* !errno */

#endif /* VMS */

/*
 * On non-VMS machines and for DECC on VMS, the GLOBALDEF and GLOBALREF
 * storage types default to normal C storage types.
 */
#ifndef GLOBALREF
#define GLOBALDEF
#define GLOBALREF extern
#endif /* !GLOBALREF */

#ifdef __DJGPP__
#undef SELECT
#define TCP_INCLUDES_DONE
#define NO_IOCTL
#define DECL_ERRNO
#include <errno.h>
#include <sys/types.h>
#include <socket.h>
#include <io.h>
#ifdef WATT32
#include <arpa/inet.h>
#include <tcp.h>
#ifdef word
#undef word
#endif /* word */
#define select select_s
#endif /* WATT32 */

#undef NETWRITE
#define NETWRITE write_s
#undef NETREAD
#define NETREAD read_s
#undef NETCLOSE
#define NETCLOSE close_s
#ifndef WATT32
#define getsockname getsockname_s
#endif /* !WATT32 */
#ifdef HAVE_GETTEXT
#define gettext gettext__
#endif
#endif /* DJGPP */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif /* HAVE_SYS_FILIO_H */

#if !defined(HAVE_LSTAT) && !defined(lstat)
#define lstat(path,block) stat(path,block)
#endif

#if defined(DECL_ERRNO) && !defined(errno)
extern int errno;
#endif /* DECL_ERRNO */

/*
Regular BSD unix versions
=========================
   These are a default unix where not already defined specifically.
 */
#ifndef INCLUDES_DONE
#include <sys/types.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include <errno.h>          /* independent */
#ifdef __MVS__  /* S/390 -- gil -- 1361 */
#include <time.h>
#endif /* __MVS__ */
#ifdef SCO
#include <sys/timeb.h>
#include <time.h>
#endif /* SCO */
#if defined(AIX) || defined(SVR4)
#include <time.h>
#endif /* AIX || SVR4 */
#include <sys/time.h>       /* independent */
#include <sys/stat.h>
#ifndef __MVS__  /* S/390 -- gil -- 1373 */
#include <sys/param.h>
#endif /* __MVS__ */
#include <sys/file.h>       /* For open() etc */

#if defined(NeXT) || defined(sony_news)
#ifndef mode_t
typedef unsigned short mode_t;
#endif /* !mode_t */

#ifndef pid_t
typedef int pid_t;
#endif /* !pid_t */

#endif /* NeXT || sony_news */

#define INCLUDES_DONE
#endif  /* Normal includes */

/* FIXME: this should be autoconf'd */
/* Interactive UNIX for i386 and i486 -- Thanks to jeffrey@itm.itm.org */
#ifdef ISC
#include <net/errno.h>
#include <sys/types.h>
#include <sys/tty.h>
#include <sys/sioctl.h>
#include <sys/bsdtypes.h>
#ifndef MERGE
#define MERGE
#include <sys/pty.h>
#undef MERGE
#else
#include <sys/pty.h>
#endif /* !MERGE */
#ifndef USE_DIRENT
#define USE_DIRENT     /* sys V style directory open */
#endif /* USE_DIRENT */
#include <sys/dirent.h>
#endif /* ISC */

/*	Directory reading stuff - BSD or SYS V
*/
#ifdef HAVE_CONFIG_H

# ifdef HAVE_LIMITS_H
#  include <limits.h>
# endif /* HAVE_LIMITS_H */
# if !defined(MAXINT) && defined(INT_MAX)
#  define MAXINT INT_MAX
# endif /* !MAXINT && INT_MAX */

#else

#if !(defined(VM) || defined(VMS) || defined(THINK_C) || defined(PCNFS) || defined(__MINGW32__))
#define DECL_SYS_ERRLIST 1
#endif

#endif /* !HAVE_CONFIG_H */

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#define N_(s) (s)

#ifndef HAVE_GETTEXT
#define gettext(s) s
#endif

/*
Defaults
========
  INCLUDE FILES FOR TCP
 */
#ifndef TCP_INCLUDES_DONE
#ifndef NO_IOCTL
#include <sys/ioctl.h> /* EJB */
#endif /* !NO_IOCTL */
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>      /* Must be after netinet/in.h */
#endif
#include <netdb.h>
#endif  /* TCP includes */

typedef unsigned short PortNumber;

#ifndef S_ISLNK
#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#endif /* S_ISLNK */

#ifndef S_ISDIR
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#endif /* S_ISDIR */

#ifndef S_ISREG
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#endif /* S_ISREG */

#ifndef S_ISUID
#define S_ISUID  0004000
#endif
#ifndef S_ISGID
#define S_ISGID  0002000
#endif
#ifndef S_ISVTX
#define S_ISVTX  0001000
#endif

#ifndef S_IRWXU
#define S_IRWXU 00700
#endif

#ifndef S_IRUSR
#define S_IRUSR 00400
#endif
#ifndef S_IWUSR
#define S_IWUSR 00200
#endif
#ifndef S_IXUSR
#define S_IXUSR 00100
#endif

#ifndef S_IRWXG
#define S_IRWXG 00070
#endif

#ifndef S_IRGRP
#define S_IRGRP 00040
#endif
#ifndef S_IWGRP
#define S_IWGRP 00020
#endif
#ifndef S_IXGRP
#define S_IXGRP 00010
#endif

#ifndef S_IRWXO
#define S_IRWXO 00007
#endif

#ifndef S_IROTH
#define S_IROTH 00004
#endif
#ifndef S_IWOTH
#define S_IWOTH 00002
#endif
#ifndef S_IXOTH
#define S_IXOTH 00001
#endif

/*

ROUGH ESTIMATE OF MAX PATH LENGTH

*/
#ifndef HT_MAX_PATH
#ifdef MAXPATHLEN
#define HT_MAX_PATH MAXPATHLEN
#else
#ifdef PATH_MAX
#define HT_MAX_PATH PATH_MAX
#else
#define HT_MAX_PATH 1024                        /* Any better ideas? */
#endif
#endif
#endif /* HT_MAX_PATH */

#if HT_MAX_PATH < 256
#undef HT_MAX_PATH
#define HT_MAX_PATH 256
#endif

/*
  MACROS FOR MANIPULATING MASKS FOR SELECT()
 */
#ifdef SELECT
#ifndef FD_SET
typedef unsigned int fd_set;
#define FD_SET(fd,pmask) (*(pmask)) |=  (1<<(fd))
#define FD_CLR(fd,pmask) (*(pmask)) &= ~(1<<(fd))
#define FD_ZERO(pmask)   (*(pmask))=0
#define FD_ISSET(fd,pmask) (*(pmask) & (1<<(fd)))
#endif  /* !FD_SET */
#endif  /* SELECT */

/*
 * Macro for setting errno - only define this if you really can do it.
 */
#if defined(CAN_SET_ERRNO) || (!defined(errno) && (!defined(VMS) || defined(UCX)))
#define set_errno(value) errno = value
#else
#define set_errno(value) /* we do not know how */
#endif

/* IPv6 support */
#if defined(HAVE_GETADDRINFO) && defined(HAVE_GAI_STRERROR) && defined(ENABLE_IPV6)
#	define INET6
#endif /* HAVE_GETADDRINFO && HAVE_GAI_STRERROR && ENABLE_IPV6 */

#if !defined(__MINGW32__)
#ifdef INET6
typedef struct sockaddr_storage SockA;  /* See netinet/in.h */
#else
typedef struct sockaddr_in SockA;  /* See netinet/in.h */
#endif /* INET6 */
#endif

#ifdef INET6
#ifdef SIN6_LEN
#define SOCKADDR_LEN(soc_address) ((struct sockaddr *)&soc_address)->sa_len
#else
#ifndef SA_LEN
#define SA_LEN(x) (((x)->sa_family == AF_INET6)?sizeof(struct sockaddr_in6): \
       (((x)->sa_family == AF_INET)?sizeof(struct sockaddr_in):sizeof(struct sockaddr)))
#endif
#define SOCKADDR_LEN(soc_address) SA_LEN((struct sockaddr *)&soc_address)
#endif /* SIN6_LEN */
#else
#define SOCKADDR_LEN(soc_address) sizeof(soc_address)
#endif /* INET6 */

#endif /* TCP_H */
