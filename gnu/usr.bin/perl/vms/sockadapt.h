/*  sockadapt.h
 *
 *  Authors: Charles Bailey  bailey@newman.upenn.edu
 *           David Denholm  denholm@conmat.phys.soton.ac.uk
 *  Last Revised:  4-Mar-1997
 *
 *  This file should include any other header files and procide any
 *  declarations, typedefs, and prototypes needed by perl for TCP/IP
 *  operations.
 *
 *  This version is set up for perl5 with socketshr 0.9D TCP/IP support.
 */

#ifndef __SOCKADAPT_INCLUDED
#define __SOCKADAPT_INCLUDED 1

#if defined(DECCRTL_SOCKETS)
    /* Use builtin socket interface in DECCRTL and
     * UCX emulation in whatever TCP/IP stack is present.
     * Provide prototypes for missing routines; stubs are
     * in sockadapt.c.
     */
#  include <socket.h>
#  include <inet.h>
#  include <in.h>
#  include <netdb.h>
#if ((__VMS_VER >= 70000000) && (__DECC_VER >= 50200000)) || (__CRTL_VER >= 70000000)
#else
   void sethostent(int);
   void endhostent(void);
   void setnetent(int);
   void endnetent(void);
   void setprotoent(int);
   void endprotoent(void);
   void setservent(int);
   void endservent(void);
#endif
#  if defined(__DECC) && defined(__DECC_VER) && (__DECC_VER >= 50200000) && !defined(Sock_size_t)
#    define Sock_size_t unsigned int
#  endif

#else
    /* Pull in SOCKETSHR's header, and set up structures for
     * gcc, whose basic header file set doesn't include the
     * TCP/IP stuff.
     */


#ifdef __GNU_CC__

/* we may not have netdb.h etc, so lets just do this here  - div */
/* no harm doing this for all .c files - needed only by pp_sys.c */

struct	hostent {
    char	*h_name;	/* official name of host */
    char	**h_aliases;	/* alias list */
    int	h_addrtype;	/* host address type */
    int	h_length;	/* length of address */
    char	**h_addr_list;	/* address */
};
#ifdef h_addr
#   undef h_addr
#endif
#define h_addr h_addr_list[0]

struct	protoent {
    char	*p_name;	/* official protocol name */
    char	**p_aliases;	/* alias list */
    int	p_proto;	/* protocol # */
};

struct	servent {
    char	*s_name;	/* official service name */
    char	**s_aliases;	/* alias list */
    int	s_port;		/* port # */
    char	*s_proto;	/* protocol to use */
};

struct	in_addr {
    unsigned long s_addr;
};

struct	sockaddr {
    unsigned short	sa_family;		/* address family */
    char	sa_data[14];		/* up to 14 bytes of direct address */
};

/*
 * Socket address, internet style.
 */
struct sockaddr_in {
	short	sin_family;
	unsigned short	sin_port;
	struct	in_addr sin_addr;
	char	sin_zero[8];
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct netent {
	char *n_name;
	char **n_aliases;
	int n_addrtype;
	long n_net;
};

/* Since socketshr.h won't declare function prototypes unless it thinks
 * the system headers have already been included, we convince it that
 * this is the case.
 */

#ifndef AF_INET
#  define AF_INET 2
#endif
#ifndef IPPROTO_TCP
#  define IPPROTO_TCP 6
#endif
#ifndef __INET_LOADED
#  define __INET_LOADED
#endif
#ifndef __NETDB_LOADED
#  define __NETDB_LOADED
#endif

/* Finally, we provide prototypes for routines not supported by SocketShr,
 * so that the stubs in sockadapt.c won't cause complaints about
 * undeclared routines.
 */

struct netent *getnetbyaddr( long net, int type);
struct netent *getnetbyname( char *name);
struct netent *getnetent();
void setnetent(int);
void endnetent();

#else /* !__GNU_CC__ */

/* DECC and VAXC have socket headers in the system set; they're for UCX, but
 * we'll assume that the actual calling sequence is identical across the
 * various TCP/IP stacks; these routines are pretty standard.
 */
#include <socket.h>
#include <in.h>
#include <inet.h>

/* SocketShr doesn't support these routines, but the DECC RTL contains
 * stubs with these names, designed to be used with the UCX socket
 * library.  We avoid linker collisions by substituting new names.
 */
#define getnetbyaddr no_getnetbyaddr
#define getnetbyname no_getnetbyname
#define getnetent    no_getnetent
#define setnetent    no_setnetent
#define endnetent    no_endnetent

#include <netdb.h>
#endif

/* We don't have these two in the system headers. */
void setnetent(int);
void endnetent();

#include <socketshr.h>
/* socketshr.h from SocketShr 0.9D doesn't alias fileno; its comments say
 * that the CRTL version works OK.  This isn't the case, at least with
 * VAXC, so we use the SocketShr version.
 * N.B. This means that sockadapt.h must be included *after* stdio.h.
 *      This is presently the case for Perl.
 */
#ifdef fileno
#  undef fileno
#endif
#define fileno si_fileno
int si_fileno(FILE *);


/* Catch erroneous results for UDP sockets -- see sockadapt.c */
#ifdef getpeername
#  undef getpeername
#endif
#define getpeername my_getpeername
int my_getpeername (int, struct sockaddr *, int *);

#endif /* SOCKETSHR stuff */
#endif /* include guard */
