/*			Generic Communication Code		HTTCP.c
**			==========================
**
**	This code is in common between client and server sides.
**
**	16 Jan 92  TBL	Fix strtol() undefined on CMU Mach.
**	25 Jun 92  JFG	Added DECNET option through TCP socket emulation.
**	13 Sep 93  MD	Added correct return of vmserrorno for HTInetStatus.
**			Added decoding of vms error message for MULTINET.
**	7-DEC-1993 Bjorn S. Nilsson, ALEPH, CERN, VMS UCX ioctl() changes
**			(done of Mosaic)
**	19 Feb 94  Danny Mayer	Added Bjorn Fixes to Lynx version
**	 7 Mar 94  Danny Mayer	Added Fix UCX version for full domain name
**	20 May 94  Andy Harper	Added support for CMU TCP/IP transport
**	17 Nov 94  Andy Harper	Added support for SOCKETSHR transport
**	16 Jul 95  S. Bjorndahl added kluge to deal with LIBCMU bug
*/

#include "HTUtils.h"
#include "tcp.h"		/* Defines SHORT_NAMES if necessary */
#include "HTAccess.h"
#include "HTParse.h"
#include "HTAlert.h"
#include "HTTCP.h"

#ifdef NSL_FORK
#include <signal.h>
#include <sys/wait.h>
#endif /* NSL_FORK */

#define FREE(x) if (x) {free(x); x = NULL;}

extern int HTCheckForInterrupt NOPARAMS;

#ifdef SVR4_BSDSELECT
PUBLIC int BSDselect PARAMS((
	int		 nfds,
	fd_set *	 readfds,
	fd_set *	 writefds,
	fd_set *	 exceptfds,
	struct timeval * timeout));
#ifdef select
#undef select
#endif /* select */
#define select BSDselect
#ifdef SOCKS
#ifdef Rselect
#undef Rselect
#endif /* Rselect */
#define Rselect BSDselect
#endif /* SOCKS */
#endif /* SVR4_BSDSELECT */

#include "LYLeaks.h"

#ifdef SHORT_NAMES
#define HTInetStatus		HTInStat
#define HTInetString		HTInStri
#define HTParseInet		HTPaInet
#endif /* SHORT_NAMES */

#ifndef FD_SETSIZE
#if defined(UCX) || defined(SOCKETSHR_TCP) || defined(CMU_TCP)
#define FD_SETSIZE 32
#else
#define FD_SETSIZE 256
#endif /* Limit # sockets to 32 for UCX, BSN - also SOCKETSHR and CMU, AH */
#endif /* FD_SETSIZE */

/*
**  Module-Wide variables
*/
PRIVATE char *hostname = NULL;		/* The name of this host */

/*
**  PUBLIC VARIABLES
*/
#ifdef SOCKS
extern BOOLEAN socks_flag;
PUBLIC unsigned long socks_bind_remoteAddr; /* for long Rbind */
#endif /* SOCKS */

/* PUBLIC SockA HTHostAddress; */	/* The internet address of the host */
					/* Valid after call to HTHostName() */

/*	Encode INET status (as in sys/errno.h)			  inet_status()
**	------------------
**
**  On entry,
**	where		gives a description of what caused the error
**	global errno	gives the error number in the Unix way.
**
**  On return,
**	returns 	a negative status in the Unix way.
*/
#ifndef PCNFS

#ifdef VMS
#include <perror.h>
#ifndef errno
extern int errno;
#endif /* !errno */
#endif /* VMS */

#ifndef VM
#ifndef VMS
#ifndef THINK_C

#ifdef DECL_SYS_ERRLIST
extern char *sys_errlist[];		/* see man perror on cernvax */
extern int sys_nerr;
#endif /* DECL_SYS_ERRLIST */

#endif /* !THINK_C */
#endif /* !VMS */
#endif /* !VM */

#endif	/* !PCNFS */

#if defined(VMS) && defined(UCX)
/*
**  A routine to mimic the ioctl function for UCX.
**  Bjorn S. Nilsson, 25-Nov-1993. Based on an example in the UCX manual.
*/
#include <iodef.h>
#define IOC_OUT (int)0x40000000
extern int vaxc$get_sdc(), sys$qiow();

PUBLIC int HTioctl ARGS3(
	int,		d,
	int,		request,
	int *,		argp)
{
    int sdc, status;
    unsigned short fun, iosb[4];
    char *p5, *p6;
    struct comm {
	int command;
	char *addr;
    } ioctl_comm;
    struct it2 {
	unsigned short len;
	unsigned short opt;
	struct comm *addr;
    } ioctl_desc;

    if ((sdc = vaxc$get_sdc (d)) == 0) {
	errno = EBADF;
	return -1;
    }
    ioctl_desc.opt  = UCX$C_IOCTL;
    ioctl_desc.len  = sizeof(struct comm);
    ioctl_desc.addr = &ioctl_comm;
    if (request & IOC_OUT) {
	fun = IO$_SENSEMODE;
	p5 = 0;
	p6 = (char *)&ioctl_desc;
    } else {
	fun = IO$_SETMODE;
	p5 = (char *)&ioctl_desc;
	p6 = 0;
    }
    ioctl_comm.command = request;
    ioctl_comm.addr = (char *)argp;
    status = sys$qiow (0, sdc, fun, iosb, 0, 0, 0, 0, 0, 0, p5, p6);
    if (!(status & 01)) {
	errno = status;
	return -1;
    }
    if (!(iosb[0] & 01)) {
	errno = iosb[0];
	return -1;
    }
    return 0;
}
#endif /* VMS && UCX */

/*	Report Internet Error
**	---------------------
*/
PUBLIC int HTInetStatus ARGS1(
	char *, 	where)
{
#ifdef VMS
#ifdef MULTINET
    SOCKET_ERRNO = vmserrno;
#endif /* MULTINET */
#endif /* VMS */

    CTRACE(tfp,
	"TCP: Error %d in `SOCKET_ERRNO' after call to %s() failed.\n\t%s\n",
	   SOCKET_ERRNO,  where,
	   /* third arg is transport/platform specific */
#ifdef VM
	   "(Error number not translated)");	/* What Is the VM equiv? */
#define ER_NO_TRANS_DONE
#endif /* VM */

#ifdef VMS
#ifdef MULTINET
	   vms_errno_string());
#else
	   ((SOCKET_ERRNO > 0 && SOCKET_ERRNO <= 65) ?
	    strerror(SOCKET_ERRNO) : "(Error number not translated)"));
#endif /* MULTINET */
#define ER_NO_TRANS_DONE
#endif /* VMS */

#ifdef HAVE_STRERROR
	   strerror(SOCKET_ERRNO));
#define ER_NO_TRANS_DONE
#endif /* HAVE_STRERROR */

#ifndef ER_NO_TRANS_DONE
	   (SOCKET_ERRNO < sys_nerr ?
	    sys_errlist[SOCKET_ERRNO] : "Unknown error" ));
#endif /* !ER_NO_TRANS_DONE */

#ifdef VMS
#ifndef MULTINET
    CTRACE(tfp,
	   "         Unix error number (SOCKET_ERRNO) = %ld dec\n",
	   SOCKET_ERRNO);
    CTRACE(tfp,
	   "         VMS error (vaxc$errno)    = %lx hex\n",
	   vaxc$errno);
#endif /* MULTINET */
#endif /* VMS */

#ifdef VMS
    /*
    **	uerrno and errno happen to be zero if vmserrno <> 0
    */
#ifdef MULTINET
    return -vmserrno;
#else
    return -vaxc$errno;
#endif /* MULTINET */
#else
    return -SOCKET_ERRNO;
#endif /* VMS */
}

/*	Parse a cardinal value				       parse_cardinal()
**	----------------------
**
** On entry,
**	*pp	    points to first character to be interpreted, terminated by
**		    non 0:9 character.
**	*pstatus    points to status already valid
**	maxvalue    gives the largest allowable value.
**
** On exit,
**	*pp	    points to first unread character
**	*pstatus    points to status updated iff bad
*/
PUBLIC unsigned int HTCardinal ARGS3(
	int *,		pstatus,
	char **,	pp,
	unsigned int,	max_value)
{
    unsigned int n;
    if ((**pp<'0') || (**pp>'9')) {	    /* Null string is error */
	*pstatus = -3;	/* No number where one expected */
	return 0;
    }

    n = 0;
    while ((**pp >= '0') && (**pp <= '9'))
	n = n*10 + *((*pp)++) - '0';

    if (n > max_value) {
	*pstatus = -4;	/* Cardinal outside range */
	return 0;
    }

    return n;
}

#ifndef DECNET	/* Function only used below for a trace message */
/*	Produce a string for an Internet address
**	----------------------------------------
**
**  On exit,
**	returns a pointer to a static string which must be copied if
**		it is to be kept.
*/
PUBLIC CONST char * HTInetString ARGS1(
	SockA*, 	soc_in)
{
    static char string[16];
    sprintf(string, "%d.%d.%d.%d",
	    (int)*((unsigned char *)(&soc_in->sin_addr)+0),
	    (int)*((unsigned char *)(&soc_in->sin_addr)+1),
	    (int)*((unsigned char *)(&soc_in->sin_addr)+2),
	    (int)*((unsigned char *)(&soc_in->sin_addr)+3));
    return string;
}
#endif /* !DECNET */

/*	Parse a network node address and port
**	-------------------------------------
**
**  On entry,
**	str	points to a string with a node name or number,
**		with optional trailing colon and port number.
**	soc_in	points to the binary internet or decnet address field.
**
**  On exit,
**	*soc_in is filled in. If no port is specified in str, that
**		field is left unchanged in *soc_in.
*/
PUBLIC int HTParseInet ARGS2(
	SockA *,	soc_in,
	CONST char *,	str)
{
    char *port;
    char *host = NULL;
    int dotcount_ip = 0;	/* for dotted decimal IP addr */
    struct hostent  *phost;	/* Pointer to host - See netdb.h */

    if (!str) {
	if (TRACE) {
	    fprintf(stderr, "HTParseInet: Can't parse `NULL'.\n");
	}
	return -1;
    }
    if (HTCheckForInterrupt()) {
	if (TRACE) {
	    fprintf (stderr, "HTParseInet: INTERRUPTED for '%s'.\n", str);
	}
	return -1;
    }
    StrAllocCopy(host, str);	/* Make a copy we can mutilate */

    /*
    **	Parse port number if present.
    */
    if ((port = strchr(host, ':')) != NULL) {
	*port++ = 0;		/* Chop off port */
	if (port[0] >= '0' && port[0] <= '9') {
#ifdef unix
	    soc_in->sin_port = htons(atol(port));
#else /* VMS: */
#ifdef DECNET
	    soc_in->sdn_objnum = (unsigned char)(strtol(port, (char**)0, 10));
#else
	    soc_in->sin_port = htons((unsigned short)strtol(port,(char**)0,10));
#endif /* Decnet */
#endif /* Unix vs. VMS */
#ifdef SUPPRESS 	/* 1. crashes!?!.  2. Not recommended */
	} else {
	    struct servent * serv = getservbyname(port, (char*)0);
	    if (serv) {
		soc_in->sin_port = serv->s_port;
	    } else if (TRACE) {
		fprintf(stderr, "TCP: Unknown service %s\n", port);
	    }
#endif /* SUPPRESS */
	}
    }

#ifdef DECNET
    /*
    **	Read Decnet node name. @@ Should know about DECnet addresses, but
    **	it's probably worth waiting until the Phase transition from IV to V.
    */
    soc_in->sdn_nam.n_len = min(DN_MAXNAML, strlen(host));  /* <=6 in phase 4 */
    strncpy(soc_in->sdn_nam.n_name, host, soc_in->sdn_nam.n_len + 1);
    if (TRACE) {
	fprintf(stderr,
		"DECnet: Parsed address as object number %d on host %.6s...\n",
		soc_in->sdn_objnum, host);
    }
#else  /* parse Internet host: */

    if (*host >= '0' && *host <= '9') {   /* Test for numeric node address: */
	char *strptr = host;
	while (*strptr) {
	    if (*strptr == '.') {
		dotcount_ip++;
	    } else if (!isdigit(*strptr)) {
		break;
	    }
	    strptr++;
	}
	if (*strptr) {		/* found non-numeric, assume domain name */
	    dotcount_ip = 0;
	}
    }

    /*
    **	Parse host number if present.
    */
    if (dotcount_ip == 3) {   /* Numeric node address: */

#ifdef DJGPP
	soc_in->sin_addr.s_addr = htonl(aton(host));
#else
#ifdef DGUX_OLD
	soc_in->sin_addr.s_addr = inet_addr(host).s_addr; /* See arpa/inet.h */
#else
#ifdef GUSI
	soc_in->sin_addr = inet_addr(host);		/* See netinet/in.h */
#else
	soc_in->sin_addr.s_addr = inet_addr(host);	/* See arpa/inet.h */
#endif /* GUSI */
#endif /* DGUX_OLD */
#endif /* DJGPP */
	FREE(host);
    } else {		    /* Alphanumeric node name: */
#ifdef MVS	/* Outstanding problem with crash in MVS gethostbyname */
	if (TRACE) {
	    fprintf(stderr,
		    "HTParseInet: Calling gethostbyname(%s)\n", host);
	}
#endif /* MVS */

#ifdef NSL_FORK
	/*
	**  Start block for fork-based gethostbyname() with
	**  checks for interrupts. - Tom Zerucha (tz@execpc.com) & FM
	*/
	{
	    /*
	    **	Pipe, child pid, and status buffers.
	    */
	    pid_t fpid, waitret = (pid_t)0;
	    int pfd[2], cstat, cst1 = 0;

	    pipe(pfd);

	    if ((fpid = fork()) == 0 ) {
		/*
		**  Child - for the long call.
		*/
		phost = gethostbyname(host);
		cst1 = 0;
		/*
		**  Return value (or nulls).
		*/
		if (phost != NULL)
		    write(pfd[1], phost->h_addr, phost->h_length);
		else
		    write(pfd[1], &cst1, 4);
		/*
		**  Return an error code.
		*/
		_exit(phost == NULL);
	    }

	    /*
	    **	(parent) Wait until lookup finishes, or interrupt.
	    */
	    cstat = 0;
	    while (cstat <= 0) {
		/*
		**  Exit when data sent.
		*/
		IOCTL(pfd[0], FIONREAD, &cstat);
		if (cstat > 0)
		    break;
		/*
		**  Exit if child exited.
		*/
		if ((waitret = waitpid(fpid, &cst1, WNOHANG)) > 0) {
		    break;
		}
		/*
		**  Abort if interrupt key pressed.
		*/
		if (HTCheckForInterrupt()) {
		    if (TRACE) {
			fprintf(stderr,
				"HTParseInet: INTERRUPTED gethostbyname.\n");
		    }
		    kill(fpid , SIGKILL);
		    waitpid(fpid, NULL, 0);
		    FREE(host);
		    close(pfd[0]);
		    close(pfd[1]);
		    return HT_INTERRUPTED;
		}
		/*
		**  Be nice to the system.
		*/
		sleep(1);
	    }
	    if (waitret <= 0) {
		waitret = waitpid(fpid, &cst1, WNOHANG);
	    }
	    if (TRACE) {
		if (WIFEXITED(cst1)) {
		    fprintf(stderr,
		      "HTParseInet: NSL_FORK child %d exited, status 0x%x.\n",
			    (int)waitret, cst1);
		} else if (WIFSIGNALED(cst1)) {
		    fprintf(stderr,
		  "HTParseInet: NSL_FORK child %d got signal, status 0x%x!\n",
			    (int)waitret, cst1);
#ifdef WCOREDUMP
		    if (WCOREDUMP(cst1)) {
			fprintf(stderr,
			      "HTParseInet: NSL_FORK child %d dumped core!\n",
				(int)waitret);
		    }
#endif /* WCOREDUMP */
		} else if (WIFSTOPPED(cst1)) {
		    fprintf(stderr,
		  "HTParseInet: NSL_FORK child %d is stopped, status 0x%x!\n",
			    (int)waitret, cst1);
		}
	    }
	    /*
	    **	Read as much as we can - should be the address.
	    */
	    IOCTL(pfd[0], FIONREAD, &cstat);
	    if (cstat < 4) {
		if (TRACE) {
		    fprintf(stderr,
		       "HTParseInet: NSL_FORK child returns only %d bytes.\n",
			    cstat);
		    fprintf(stderr,
			"             Trying again without forking.\n");
		}
		phost = gethostbyname(host);	/* See netdb.h */
		if (!phost) {
		    if (TRACE) {
			fprintf(stderr,
			 "HTParseInet: Can't find internet node name `%s'.\n",
				host);
		    }
		    memset((void *)&soc_in->sin_addr, 0, sizeof(soc_in->sin_addr));
		} else {
		    memcpy((void *)&soc_in->sin_addr,
			   phost->h_addr, phost->h_length);
		}
#ifdef NOTDEFINED
		cstat = read(pfd[0], (void *)&soc_in->sin_addr , 4);
#endif /* NOTDEFINED */
	    } else {
		cstat = read(pfd[0], (void *)&soc_in->sin_addr , cstat);
	    }
	    close(pfd[0]);
	    close(pfd[1]);
	}
	if (soc_in->sin_addr.s_addr == 0) {
	    if (TRACE) {
		fprintf(stderr,
			"HTParseInet: Can't find internet node name `%s'.\n",
			host);
	    }
	    FREE(host);
	    return -1;
	}
	FREE(host);
#ifdef MVS
	if (TRACE) {
	    fprintf(stderr,
		    "HTParseInet: gethostbyname() returned %d\n", phost);
	}
#endif /* MVS */

#else /* Not NSL_FORK: */
#ifdef DJGPP
	soc_in->sin_addr.s_addr = htonl(resolve(host));
	FREE(host);
	if (soc_in->sin_addr.s_addr == 0) {
		 if (TRACE)
			  fprintf(stderr,
			 "HTTPAccess: Can't find internet node name `%s'.\n",host);
		 return -1;  /* Fail? */
	}
#else
	phost = gethostbyname(host);	/* See netdb.h */
#ifdef MVS
	if (TRACE) {
	    fprintf(stderr,
		    "HTParseInet: gethostbyname() returned %d\n", phost);
	}
#endif /* MVS */
	if (!phost) {
	    if (TRACE) {
		fprintf(stderr,
			"HTParseInet: Can't find internet node name `%s'.\n",
			host);
	    }
	    FREE(host);
	    return -1;	/* Fail? */
	}
	FREE(host);
#if defined(VMS) && defined(CMU_TCP)
	/*
	**  In LIBCMU, phost->h_length contains not the length of one address
	**  (four bytes) but the number of bytes in *h_addr, i.e. some multiple
	**  of four. Thus we need to hard code the value here, and remember to
	**  change it if/when IP addresses change in size. :-(	LIBCMU is no
	**  longer supported, and CMU users are encouraged to obtain and use
	**  SOCKETSHR/NETLIB instead. - S. Bjorndahl
	*/
	memcpy((void *)&soc_in->sin_addr, phost->h_addr, 4);
#else
	memcpy((void *)&soc_in->sin_addr, phost->h_addr, phost->h_length);
#endif /* VMS && CMU_TCP */
#endif /* DJGPP */
#endif /* NSL_FORK */
    }

    if (TRACE) {
	fprintf(stderr,
	   "HTParseInet: Parsed address as port %d, IP address %d.%d.%d.%d\n",
		(int)ntohs(soc_in->sin_port),
		(int)*((unsigned char *)(&soc_in->sin_addr)+0),
		(int)*((unsigned char *)(&soc_in->sin_addr)+1),
		(int)*((unsigned char *)(&soc_in->sin_addr)+2),
		(int)*((unsigned char *)(&soc_in->sin_addr)+3));
    }
#endif	/* Internet vs. Decnet */

    return 0;	/* OK */
}

/*	Free our name for the host on which we are - FM
**	-------------------------------------------
**
*/
PRIVATE void free_HTTCP_hostname NOARGS
{
    FREE(hostname);
}

/*	Derive the name of the host on which we are
**	-------------------------------------------
**
*/
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64		/* Arbitrary limit */
#endif /* MAXHOSTNAMELEN */

PRIVATE void get_host_details NOARGS
{
    char name[MAXHOSTNAMELEN+1];	/* The name of this host */
#ifdef UCX
    char *domain_name;			/* The name of this host domain */
#endif /* UCX */
#ifdef NEED_HOST_ADDRESS		/* no -- needs name server! */
    struct hostent * phost;		/* Pointer to host -- See netdb.h */
#endif /* NEED_HOST_ADDRESS */
    int namelength = sizeof(name);

    if (hostname)
	return; 			/* Already done */
    gethostname(name, namelength);	/* Without domain */
    StrAllocCopy(hostname, name);
    atexit(free_HTTCP_hostname);
#ifdef UCX
    /*
    **	UCX doesn't give the complete domain name.
    **	Get rest from UCX$BIND_DOM logical.
    */
    if (strchr(hostname,'.') == NULL) { 	  /* Not full address */
	domain_name = getenv("UCX$BIND_DOMAIN");
	if (domain_name != NULL) {
	    StrAllocCat(hostname, ".");
	    StrAllocCat(hostname, domain_name);
	}
     }
#endif /* UCX */
    CTRACE(tfp, "TCP: Local host name is %s\n", hostname);

#ifndef DECNET	/* Decnet ain't got no damn name server 8#OO */
#ifdef NEED_HOST_ADDRESS		/* no -- needs name server! */
    phost = gethostbyname(name);	/* See netdb.h */
    if (!phost) {
	if (TRACE) fprintf(stderr,
		"TCP: Can't find my own internet node address for `%s'!!\n",
		name);
	return;  /* Fail! */
    }
    StrAllocCopy(hostname, phost->h_name);
    memcpy(&HTHostAddress, &phost->h_addr, phost->h_length);
    if (TRACE) fprintf(stderr, "     Name server says that I am `%s' = %s\n",
	    hostname, HTInetString(&HTHostAddress));
#endif /* NEED_HOST_ADDRESS */

#endif /* !DECNET */
}

PUBLIC CONST char * HTHostName NOARGS
{
    get_host_details();
    return hostname;
}

/*
**  Interruptable connect as implemented for Mosaic by Marc Andreesen
**  and hacked in for Lynx years ago by Lou Montulli, and further
**  modified over the years by numerous Lynx lovers. - FM
*/
PUBLIC int HTDoConnect ARGS4(
	CONST char *,	url,
	char *, 	protocol,
	int,		default_port,
	int *,		s)
{
    struct sockaddr_in soc_address;
    struct sockaddr_in *soc_in = &soc_address;
    int status;
    char *line = NULL;
    char *p1 = NULL;
    char *at_sign = NULL;
    char *host = NULL;

    /*
    **	Set up defaults.
    */
    soc_in->sin_family = AF_INET;
    soc_in->sin_port = htons(default_port);

    /*
    **	Get node name and optional port number.
    */
    p1 = HTParse(url, "", PARSE_HOST);
    if ((at_sign = strchr(p1, '@')) != NULL) {
	/*
	**  If there's an @ then use the stuff after it as a hostname.
	*/
	StrAllocCopy(host, (at_sign + 1));
    } else {
	StrAllocCopy(host, p1);
    }
    FREE(p1);

    line = (char *)calloc(1, (strlen(host) + strlen(protocol) + 128));
    if (line == NULL)
	outofmem(__FILE__, "HTDoConnect");
    sprintf (line, "Looking up %s.", host);
    _HTProgress (line);
    status = HTParseInet(soc_in, host);
    if (status) {
	if (status != HT_INTERRUPTED) {
	    sprintf (line, "Unable to locate remote host %s.", host);
	    _HTProgress(line);
	    status = HT_NO_DATA;
	}
	FREE(host);
	FREE(line);
	return status;
    }

    sprintf (line, "Making %s connection to %s.", protocol, host);
    _HTProgress (line);
    FREE(host);

    /*
    **	Now, let's get a socket set up from the server for the data.
    */
    *s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*s == -1) {
	HTAlert("socket failed.");
	FREE(line);
	return HT_NO_DATA;
    }

#ifndef DOSPATH
#if !defined(NO_IOCTL) || defined(USE_FCNTL)
    /*
    **	Make the socket non-blocking, so the connect can be canceled.
    **	This means that when we issue the connect we should NOT
    **	have to wait for the accept on the other end.
    */
    {
#ifdef USE_FCNTL
	int ret = fcntl(*s, F_SETFL, O_NONBLOCK);
#else
	int val = 1;
	int ret = IOCTL(*s, FIONBIO, &val);
#endif /* USE_FCNTL */
	if (ret == -1)
	    _HTProgress("Could not make connection non-blocking.");
    }
#endif /* !NO_IOCTL || USE_FCNTL */
#endif /* !DOSPATH */

    /*
    **	Issue the connect.  Since the server can't do an instantaneous
    **	accept and we are non-blocking, this will almost certainly return
    **	a negative status.
    */
#ifdef SOCKS
    if (socks_flag) {
	status = Rconnect(*s, (struct sockaddr*)&soc_address,
			  sizeof(soc_address));
	/*
	**  For long Rbind.
	*/
	socks_bind_remoteAddr = soc_address.sin_addr.s_addr;
    } else
#endif /* SOCKS */
    status = connect(*s, (struct sockaddr*)&soc_address, sizeof(soc_address));
#ifndef DJGPP
    /*
    **	According to the Sun man page for connect:
    **	   EINPROGRESS	       The socket is non-blocking and the  con-
    **			       nection cannot be completed immediately.
    **			       It is possible to select(2) for	comple-
    **			       tion  by  selecting the socket for writ-
    **			       ing.
    **	According to the Motorola SVR4 man page for connect:
    **	   EAGAIN	       The socket is non-blocking and the  con-
    **			       nection cannot be completed immediately.
    **			       It is possible to select for  completion
    **			       by  selecting  the  socket  for writing.
    **			       However, this is only  possible	if  the
    **			       socket  STREAMS	module	is  the topmost
    **			       module on  the  protocol  stack	with  a
    **			       write  service  procedure.  This will be
    **			       the normal case.
    */
    if ((status < 0) &&
	(SOCKET_ERRNO == EINPROGRESS || SOCKET_ERRNO == EAGAIN)) {
	struct timeval timeout;
	int ret;
	int tries=0;

	ret = 0;
	while (ret <= 0) {
	    fd_set writefds;

	    /*
	    **	Protect against an infinite loop.
	    */
	    if (tries++ >= 180000) {
		HTAlert("Connection failed for 180,000 tries.");
		FREE(line);
		return HT_NO_DATA;
	    }

	    timeout.tv_sec = 0;
	    timeout.tv_usec = 100000;
	    FD_ZERO(&writefds);
	    FD_SET(*s, &writefds);
#ifdef SOCKS
	    if (socks_flag)
		ret = Rselect(FD_SETSIZE, NULL,
			      (void *)&writefds, NULL, &timeout);
	    else
#endif /* SOCKS */
	    ret = select(FD_SETSIZE, NULL, (void *)&writefds, NULL, &timeout);

	   /*
	   **  If we suspend, then it is possible that select will be
	   **  interrupted.  Allow for this possibility. - JED
	   */
	   if ((ret == -1) && (errno == EINTR))
	     continue;

	    /*
	    **	Again according to the Sun and Motorola man pages for connect:
	    **	   EALREADY	       The socket is non-blocking and a  previ-
	    **			       ous  connection attempt has not yet been
	    **			       completed.
	    **	Thus if the SOCKET_ERRNO is NOT EALREADY we have a real error,
	    **	and should break out here and return that error.
	    **	Otherwise if it is EALREADY keep on trying to complete the
	    **	connection.
	    */
	    if ((ret < 0) && (SOCKET_ERRNO != EALREADY)) {
		status = ret;
		break;
	    } else if (ret > 0) {
		/*
		**  Extra check here for connection success, if we try to
		**  connect again, and get EISCONN, it means we have a
		**  successful connection.  But don't check with SOCKS.
		*/
#ifdef SOCKS
		if (socks_flag) {
		    status = 0;
		} else {
#endif /* SOCKS */
		status = connect(*s, (struct sockaddr*)&soc_address,
				 sizeof(soc_address));
#ifdef UCX
		/*
		**  A UCX feature: Instead of returning EISCONN
		**		 UCX returns EADDRINUSE.
		**  Test for this status also.
		*/
		if ((status < 0) && ((SOCKET_ERRNO == EISCONN) ||
				     (SOCKET_ERRNO == EADDRINUSE)))
#else
		if ((status < 0) && (SOCKET_ERRNO == EISCONN))
#endif /* UCX */
		{
		    status = 0;
		}

		if (status && (SOCKET_ERRNO == EALREADY)) /* new stuff LJM */
		    ret = 0; /* keep going */
		else
		    break;
#ifdef SOCKS
		}
#endif /* SOCKS */
	    }
#ifdef SOCKS
	    else if (!socks_flag)
#else
	    else
#endif /* SOCKS */
	    {
		/*
		**  The select says we aren't ready yet.  Try to connect
		**  again to make sure.  If we don't get EALREADY or EISCONN,
		**  something has gone wrong.  Break out and report it.
		**
		**  For some reason, SVR4 returns EAGAIN here instead of
		**  EALREADY, even though the man page says it should be
		**  EALREADY.
		**
		**  For some reason, UCX pre 3 apparently returns
		**  errno = 18242 instead the EALREADY or EISCONN.
		*/
		status = connect(*s, (struct sockaddr*)&soc_address,
				 sizeof(soc_address));
		if ((status < 0) &&
		    (SOCKET_ERRNO != EALREADY && SOCKET_ERRNO != EAGAIN) &&
#ifdef UCX
		    (SOCKET_ERRNO != 18242) &&
#endif /* UCX */
		    (SOCKET_ERRNO != EISCONN)) {
		    break;
		}
	    }
	    if (HTCheckForInterrupt()) {
		if (TRACE)
		    fprintf(stderr, "*** INTERRUPTED in middle of connect.\n");
		status = HT_INTERRUPTED;
		SOCKET_ERRNO = EINTR;
		break;
	    }
	}
    }
#endif /* !DJGPP */
    if (status < 0) {
	/*
	**  The connect attempt failed or was interrupted,
	**  so close up the socket.
	*/
	NETCLOSE(*s);
    }
#ifndef DOSPATH
#if !defined(NO_IOCTL) || defined(USE_FCNTL)
    else {
	/*
	**  Make the socket blocking again on good connect.
	*/
#ifdef USE_FCNTL
	int ret = fcntl(*s, F_SETFL, 0);
#else
	int val = 0;
	int ret = IOCTL(*s, FIONBIO, &val);
#endif /* USE_FCNTL */
	if (ret == -1)
	    _HTProgress("Could not restore socket to blocking.");
    }
#endif /* !NO_IOCTL || USE_FCNTL */
#endif /* !DOSPATH */

    FREE(line);
    return status;
}

/*
**  This is so interruptible reads can be implemented cleanly.
*/
PUBLIC int HTDoRead ARGS3(
	int,		fildes,
	void *, 	buf,
	unsigned,	nbyte)
{
    int ready, ret;
    fd_set readfds;
    struct timeval timeout;
    int tries=0;
#ifdef UCX
    int nb;
#endif /* UCX, BSN */

    if (fildes <= 0)
	return -1;

    if (HTCheckForInterrupt()) {
	SOCKET_ERRNO = EINTR;
	return (HT_INTERRUPTED);
    }

#if !defined(NO_IOCTL)
    ready = 0;
#else
    ready = 1;
#endif /* bypass for NO_IOCTL */
    while (!ready) {
	/*
	**  Protect against an infinite loop.
	*/
	if (tries++ >= 180000) {
	    HTAlert("Socket read failed for 180,000 tries.");
	    SOCKET_ERRNO = EINTR;
	    return HT_INTERRUPTED;
	}

	/*
	**  If we suspend, then it is possible that select will be
	**  interrupted.  Allow for this possibility. - JED
	*/
	do {
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 100000;
	    FD_ZERO(&readfds);
	    FD_SET(fildes, &readfds);
#ifdef SOCKS
	    if (socks_flag)
		ret = Rselect(FD_SETSIZE,
			      (void *)&readfds, NULL, NULL, &timeout);
	    else
#endif /* SOCKS */
		ret = select(FD_SETSIZE,
			     (void *)&readfds, NULL, NULL, &timeout);
	} while ((ret == -1) && (errno == EINTR));

	if (ret < 0) {
	    return -1;
	} else if (ret > 0) {
	    ready = 1;
	} else if (HTCheckForInterrupt()) {
	    SOCKET_ERRNO = EINTR;
	    return HT_INTERRUPTED;
	}
    }

#if !defined(UCX) || !defined(VAXC)
    return SOCKET_READ (fildes, buf, nbyte);
#else
    /*
    **	VAXC and UCX problem only.
    */
    errno = vaxc$errno = 0;
    nb = SOCKET_READ (fildes, buf, nbyte);
    CTRACE(tfp,
	   "Read - nb,errno,vaxc$errno: %d %d %d\n", nb,errno,vaxc$errno);
    if ((nb <= 0) && TRACE)
	perror ("HTTCP.C:HTDoRead:read");	   /* RJF */
    /*
    **	An errno value of EPIPE and nb < 0 indicates end-of-file on VAXC.
    */
    if ((nb <= 0) && (errno == EPIPE)) {
	nb = 0;
	errno = 0;
    }
    return nb;
#endif /* UCX, BSN */
}

#ifdef SVR4_BSDSELECT
/*
**  This is a fix for the difference between BSD's select() and
**  SVR4's select().  SVR4's select() can never return a value larger
**  than the total number of file descriptors being checked.  So, if
**  you select for read and write on one file descriptor, and both
**  are true, SVR4 select() will only return 1.  BSD select in the
**  same situation will return 2.
**
**	Additionally, BSD select() on timing out, will zero the masks,
**	while SVR4 does not.  This is fixed here as well.
**
**	Set your tabstops to 4 characters to have this code nicely formatted.
**
**	Jerry Whelan, guru@bradley.edu, June 12th, 1993
*/
#ifdef select
#undef select
#endif /* select */

#ifdef SOCKS
#ifdef Rselect
#undef Rselect
#endif /* Rselect */
#endif /* SOCKS */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>

PUBLIC int BSDselect ARGS5(
	int,			nfds,
	fd_set *,		readfds,
	fd_set *,		writefds,
	fd_set *,		exceptfds,
	struct timeval *,	timeout)
{
    int rval,
    i;

#ifdef SOCKS
    if (socks_flag)
	rval = Rselect(nfds, readfds, writefds, exceptfds, timeout);
    else
#endif /* SOCKS */
    rval = select(nfds, readfds, writefds, exceptfds, timeout);

    switch (rval) {
	case -1:
	    return(rval);
	    break;

	case 0:
	    if (readfds != NULL)
		FD_ZERO(readfds);
	    if (writefds != NULL)
		FD_ZERO(writefds);
	    if (exceptfds != NULL)
		FD_ZERO(exceptfds);
	    return(rval);
	    break;

	default:
	    for (i = 0, rval = 0; i < nfds; i++) {
		if ((readfds != NULL) && FD_ISSET(i, readfds))
		    rval++;
		if ((writefds != NULL) && FD_ISSET(i, writefds))
		    rval++;
		if ((exceptfds != NULL) && FD_ISSET(i, exceptfds))
		    rval++;

	    }
	    return(rval);
    }
/* Should never get here */
}
#endif /* SVR4_BSDSELECT */
