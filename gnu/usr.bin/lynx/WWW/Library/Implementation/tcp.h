/*                System dependencies in the W3 library
                                   SYSTEM DEPENDENCIES
                                             
   System-system differences for TCP include files and macros. This
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

#ifndef HTUTILS_H
#include "HTUtils.h"
#endif /* !HTUTILS_H */

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

#ifdef unix
#define GOT_PIPE
#endif /* unix */

typedef struct sockaddr_in SockA;  /* See netinet/in.h */


#ifndef STDIO_H
#include <stdio.h>
#define STDIO_H
#endif /* !STDIO_H */

#ifndef VMS
#include <sys/types.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define D_NAMLEN(dirent) strlen((dirent)->d_name)
# define STRUCT_DIRENT struct dirent
#else
# define D_NAMLEN(dirent) (dirent)->d_namlen
# define STRUCT_DIRENT struct direct
# define direct dirent	/* FIXME */
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif /* HAVE_DIRENT_H */
#endif /* !VMS */

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef _AIX
#define AIX
#endif /* _AIX */
#ifdef AIX
#define unix
#endif /* AIX */

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif
#endif

#if HAVE_STRING_H
#include <string.h>             /* For bzero etc */
#endif /* HAVE_STRING_H */

/*

  M ACROS FOR CONVERTING CHARACTERS

 */
#ifndef TOASCII
#define TOASCII(c) (c)
#define FROMASCII(c) (c)
#endif /* !TOASCII */


/*
IBM-PC running Windows NT

	These parameters providede by  Susan C. Weber <sweber@kyle.eitech.com>.
*/

#ifdef _WINDOWS
#include "fcntl.h"                      /* For HTFile.c */
#include "sys\types.h"                  /* For HTFile.c */
#include "sys\stat.h"                   /* For HTFile.c */
#undef NETREAD
#undef NETWRITE
#undef NETCLOSE
#undef IOCTL
#define NETREAD(s,b,l)  recv((s),(b),(l),0)
#define NETWRITE(s,b,l) send((s),(b),(l),0)
#define NETCLOSE(s)     closesocket(s)
#define IOCTL				ioctlsocket
#include <io.h>
#include <string.h>
#include <process.h>
#include <time.h>
#include <errno.h>
#include <direct.h>
#include <stdio.h>
#include <winsock.h>
typedef struct sockaddr_in SockA;  /* See netinet/in.h */
#define EINPROGRESS          (WSABASEERR+36)
#define EALREADY             (WSABASEERR+37)
#define EISCONN              (WSABASEERR+56)
#define EINTR                (WSABASEERR+4)
#define EAGAIN               (WSABASEERR+1002)
#define ENOTCONN             (WSABASEERR+57)
#define ECONNRESET           (WSABASEERR+54)
#define EINVAL                22
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
   close sockets. In these cases the socket number is a VMS channel
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

#ifndef STDIO_H
#include <stdio.h>
#define STDIO_H
#endif /* !STDIO_H */

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
extern int bzero();
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
#define __TIME_T 1
#endif /* __TYPE */
#ifdef __TIME_LOADED
#define __TIME 1  /* to avoid double definitions in in.h */
#endif /* __TIME_LOADED */
#include "multinet_root:[multinet.include.sys]time.h"
#include "multinet_root:[multinet.include.sys]socket.h"
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
#include "types.h"  /* for socket.h */
#include "socket.h"
#include "dn"
#include "dnetdb"
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

#endif /* VMS */

/*
 * On non-VMS machines and for DECC on VMS, the GLOBALDEF and GLOBALREF
 * storage types default to normal C storage types.
 */
#ifndef GLOBALREF
#define GLOBALDEF
#define GLOBALREF extern
#endif /* !GLOBALREF */

#ifdef DJGPP
#undef SELECT
#define TCP_INCLUDES_DONE
#define NO_IOCTL
#include <errno.h>
#include <sys/types.h>
#include <socket.h>

#undef NETWRITE
#define NETWRITE write_s
#undef NETREAD
#define NETREAD read_s
#undef NETCLOSE
#define NETCLOSE close_s
#endif

/*
SCO ODT unix version
 */
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif /* HAVE_SYS_FILIO_H */

/*
MIPS unix
 */
/* Mips hack (bsd4.3/sysV mixture...) */
#ifdef mips
extern int errno;
#endif /* mips */

/*
Regular BSD unix versions
=========================
   These are a default unix where not already defined specifically.
 */
#ifndef INCLUDES_DONE
#include <sys/types.h>
/* #include <streams/streams.h>                 not ultrix */
#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include <errno.h>          /* independent */
#ifdef SCO
#include <sys/timeb.h>
#include <time.h>
#endif /* SCO */
#if defined(AIX) || defined(SVR4)
#include <time.h>
#endif /* AIX || SVR4 */
#include <sys/time.h>       /* independent */
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/file.h>       /* For open() etc */
#if defined(NeXT) || defined(sony_news)
#ifndef mode_t
typedef unsigned short mode_t;
#endif /* !mode_t */
#ifndef pid_t
typedef int pid_t;
#endif /* !pid_t */
#ifndef S_ISREG
#define S_ISREG(m) (((m) & 0170000) == 0100000)
#endif /* S_ISREG */
#ifndef WEXITSTATUS
#ifdef sony_news
#define WEXITSTATUS(s) WIFEXITED(s)
#else
#define WEXITSTATUS(s) (((s).w_status >> 8) & 0377)
#endif /* sony_news */
#endif /* !WEXITSTATUS */
#ifndef WTERMSIG
#ifdef sony_news
#define WTERMSIG(s) (s).w_termsig
#else
#define WTERMSIG(s) (((s).w_status >> 8) & 0177)
#endif /* sony_news */
#endif /* !WTERMSIG */
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
#if defined(UNIX) && !defined(unix)
#define unix
#endif /* UNIX && !unix */

#ifdef HAVE_CONFIG_H

# ifdef HAVE_LIMITS_H
#  include <limits.h>
# endif /* HAVE_LIMITS_H */
# if !defined(MAXINT) && defined(INT_MAX)
#  define MAXINT INT_MAX
# endif /* !MAXINT && INT_MAX */

#else

/* FIXME: remove after completing configure-script */
#ifdef unix                    /* if this is to compile on a UNIX machine */
#define HAVE_READDIR 1    /* if directory reading functions are available */
#ifdef USE_DIRENT             /* sys v version */
#include <dirent.h>
#define direct dirent
#else
#include <sys/dir.h>
#endif /* USE_DIRENT */
#if defined(sun) && defined(__svr4__)
#include <sys/fcntl.h>
#include <limits.h>
#else
#if defined(__hpux) || defined(LINUX) || defined (__FreeBSD__) 
#include <limits.h>
#endif /* __hpux || LINUX || __FreeBSD__ */
#endif /* sun && __svr4__ */
#if !defined(MAXINT) && defined(INT_MAX)
#define MAXINT INT_MAX
#endif /* !MAXINT && INT_MAX */
#endif /* unix */

#ifndef VM
#ifndef VMS
#ifndef THINK_C
#define DECL_SYS_ERRLIST 1
#endif /* !THINK_C */
#endif /* !VMS */
#endif /* !VM */

#endif /* !HAVE_CONFIG_H */

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
#ifndef __hpux /* this may or may not be good -marc */
#include <arpa/inet.h>      /* Must be after netinet/in.h */
#endif /* !__hpux */
#include <netdb.h>
#endif  /* TCP includes */

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


#endif /* TCP_H */
