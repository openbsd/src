/*
**	$Id: identd.c,v 1.1.1.1 1995/10/18 08:43:18 deraadt Exp $
**
** identd.c                       A TCP/IP link identification protocol server
**
** This program is in the public domain and may be used freely by anyone
** who wants to. 
**
** Last update: 22 April 1993
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#if defined(IRIX) || defined(SVR4) || defined(NeXT) || defined(__NetBSD__)
#  define SIGRETURN_TYPE void
#  define SIGRETURN_TYPE_IS_VOID
#else
#  define SIGRETURN_TYPE int
#endif

#ifdef SVR4
#  define STRNET
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifndef _AUX_SOURCE
#  include <sys/file.h>
#endif
#include <sys/time.h>
#include <sys/wait.h>

#include <pwd.h>
#include <grp.h>

#include <netinet/in.h>

#ifndef HPUX7
#  include <arpa/inet.h>
#endif

#if defined(MIPS) || defined(BSD43)
extern int errno;
#endif

#include "identd.h"
#include "error.h"

/* Antique unixes do not have these things defined... */
#ifndef FD_SETSIZE
#  define FD_SETSIZE 256
#endif

#ifndef FD_SET
#  ifndef NFDBITS
#    define NFDBITS   	(sizeof(int) * NBBY)  /* bits per mask */
#  endif
#  define FD_SET(n, p)  ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#endif

#ifndef FD_ZERO
#  define FD_ZERO(p)        bzero((char *)(p), sizeof(*(p)))
#endif

extern char *version;

extern void *calloc();
extern void *malloc();


char *path_unix = NULL;
char *path_kmem = NULL;

int verbose_flag = 0;
int debug_flag   = 0;
int syslog_flag  = 0;
int multi_flag   = 0;
int other_flag   = 0;
int unknown_flag = 0;
int number_flag  = 0;
int noident_flag = 0;

int lport = 0;
int fport = 0;

char *charset_name = NULL;
char *indirect_host = NULL;
char *indirect_password = NULL;

static int child_pid;

#ifdef LOG_DAEMON
static int syslog_facility = LOG_DAEMON;
#endif

/*
** The structure passing convention for GCC is incompatible with
** Suns own C compiler, so we define our own inet_ntoa() function.
** (This should only affect GCC version 1 I think, a well, this works
** for version 2 also so why bother.. :-)
*/
#if defined(__GNUC__) && defined(__sparc__)

#ifdef inet_ntoa
#undef inet_ntoa
#endif

char *inet_ntoa(ad)
  struct in_addr ad;
{
  unsigned long int s_ad;
  int a, b, c, d;
  static char addr[20];
  
  s_ad = ad.s_addr;
  d = s_ad % 256;
  s_ad /= 256;
  c = s_ad % 256;
  s_ad /= 256;
  b = s_ad % 256;
  a = s_ad / 256;
  sprintf(addr, "%d.%d.%d.%d", a, b, c, d);
  
  return addr;
}
#endif


/*
** Return the name of the connecting host, or the IP number as a string.
*/
char *gethost(addr)
  struct in_addr *addr;
{
  struct hostent *hp;

  
  hp = gethostbyaddr((char *) addr, sizeof(struct in_addr), AF_INET);
  if (hp)
    return hp->h_name;
  else
    return inet_ntoa(*addr);
}

/*
** Exit cleanly after our time's up.
*/
static SIGRETURN_TYPE
alarm_handler()
{
  if (syslog_flag)
    syslog(LOG_DEBUG, "SIGALRM triggered, exiting");
  
  exit(0);
}

#if !defined(hpux) && !defined(__hpux) && !defined(SVR4) || defined(_CRAY)
/*
** This is used to clean up zombie child processes
** if the -w or -b options are used.
*/
static SIGRETURN_TYPE
child_handler()
{
#if defined(IRIX) || defined(NeXT)
  union wait status;
#else
  int status;
#endif

  while (wait3(&status, WNOHANG, NULL) > 0)
    ;
  
#ifndef SIGRETURN_TYPE_IS_VOID
  return 0;
#endif
}
#endif


char *clearmem(bp, len)
  char *bp;
  int len;
{
  char *cp;

  cp = bp;
  while (len-- > 0)
    *cp++ = 0;

  return bp;
}


/*
** Main entry point into this daemon
*/
int main(argc,argv)
  int argc;
  char *argv[];
{
  int i, len;
  struct sockaddr_in sin;
  struct in_addr laddr, faddr;
  struct timeval tv;

  int background_flag = 0;
  int timeout = 0;
  char *portno = "113";
  char *bind_address = NULL;
  int set_uid = 0;
  int set_gid = 0;
  int inhibit_default_config = 0;
  int opt_count = 0;		/* Count of option flags */
  
#ifdef __convex__
  argc--;    /* get rid of extra argument passed by inetd */
#endif

  /*
  ** Prescan the arguments for "-f<config-file>" switches
  */
  inhibit_default_config = 0;
  for (i = 1; i < argc && argv[i][0] == '-'; i++)
    if (argv[i][1] == 'f')
      inhibit_default_config = 1;

  /*
  ** Parse the default config file - if it exists
  */
  if (!inhibit_default_config)
    parse_config(NULL, 1);
  
  /*
  ** Parse the command line arguments
  */
  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
    opt_count++;
    switch (argv[i][1])
    {
      case 'b':    /* Start as standalone daemon */
        background_flag = 1;
	break;

      case 'w':    /* Start from Inetd, wait mode */
	background_flag = 2;
	break;

      case 'i':    /* Start from Inetd, nowait mode */
	background_flag = 0;
	break;

      case 't':
	timeout = atoi(argv[i]+2);
	break;

      case 'p':
	portno = argv[i]+2;
	break;

      case 'a':
	bind_address = argv[i]+2;
	break;
	  
      case 'u':
	if (isdigit(argv[i][2]))
	  set_uid = atoi(argv[i]+2);
	else
 	{
	  struct passwd *pwd;

	  pwd = getpwnam(argv[i]+2);
	  if (!pwd)
	    ERROR1("no such user (%s) for -u option", argv[i]+2);
	  else
	  {
	    set_uid = pwd->pw_uid;
	    set_gid = pwd->pw_gid;
	  }
	}
	break;
	
      case 'g':
	if (isdigit(argv[i][2]))
	  set_gid = atoi(argv[i]+2);
	else
 	{
	  struct group *grp;

	  grp = getgrnam(argv[i]+2);
	  if (!grp)
	    ERROR1("no such group (%s) for -g option", argv[i]+2);
	  else
	    set_gid = grp->gr_gid;
	}
	break;

      case 'c':
	charset_name = argv[i]+2;
	break;

      case 'r':
	indirect_host = argv[i]+2;
	break;

      case 'l':    /* Use the Syslog daemon for logging */
	syslog_flag++;
	break;

      case 'o':
	other_flag = 1;
	break;

      case 'e':
	unknown_flag = 1;
	break;

      case 'n':
	number_flag = 1;
	break;
       
      case 'V':    /* Give version of this daemon */
	printf("[in.identd, version %s]\r\n", version);
	exit(0);
	break;

      case 'v':    /* Be verbose */
	verbose_flag++;
	break;
	  
      case 'd':    /* Enable debugging */
	debug_flag++;
	break;

      case 'm':    /* Enable multiline queries */
	multi_flag++;
	break;

      case 'N':    /* Enable users ".noident" files */
	noident_flag++;
	break;
    }
  }
  
#if defined(_AUX_SOURCE) || defined (SUNOS35)
  /* A/UX 2.0* & SunOS 3.5 calls us with an argument XXXXXXXX.YYYY
  ** where XXXXXXXXX is the hexadecimal version of the callers
  ** IP number, and YYYY is the port/socket or something.
  ** It seems to be impossible to pass arguments to a daemon started
  ** by inetd.
  **
  ** Just in case it is started from something else, then we only
  ** skip the argument if no option flags have been seen.
  */
  if (opt_count == 0)
    argc--;
#endif

  /*
  ** Path to kernel namelist file specified on command line
  */
  if (i < argc)
    path_unix = argv[i++];

  /*
  ** Path to kernel memory device specified on command line
  */
  if (i < argc)
    path_kmem = argv[i++];


  /*
  ** Open the kernel memory device and read the nlist table
  */
  if (k_open() < 0)
      ERROR("main: k_open");

  /*
  ** Do the special handling needed for the "-b" flag
  */
  if (background_flag == 1)
  {
    struct sockaddr_in addr;
    struct servent *sp;
    int fd;

    
    if (fork())
      exit(0);

    close(0);
    close(1);
    close(2);

    if (fork())
      exit(0);
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
      ERROR("main: socket");
    
    if (fd != 0)
      dup2(fd, 0);

    clearmem(&addr, sizeof(addr));
    
    addr.sin_len = sizeof(struct sockaddr_in);
    addr.sin_family = AF_INET;
    if (bind_address == NULL)
      addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
    {
      if (inet_aton(bind_address, &addr.sin_addr) == 0)
      {
	struct hostent *hp;

	hp = gethostbyname(bind_address);
	if (!hp)
	  ERROR1("no such address (%s) for -a switch", bind_address);

	memcpy(&addr.sin_addr, hp->h_addr, sizeof(addr.sin_addr));
      }
    }

    if (isdigit(portno[0]))
      addr.sin_port = htons(atoi(portno));
    else
    {
      sp = getservbyname(portno, "tcp");
      if (sp == NULL)
	ERROR1("main: getservbyname: %s", portno);
      addr.sin_port = sp->s_port;
    }
    
    if (bind(0, (struct sockaddr *) &addr, sizeof(addr)) < 0)
      ERROR("main: bind");

    if (listen(0, 3) < 0)
      ERROR("main: listen");
  }
  
  if (set_gid)
    if (setgid(set_gid) == -1)
      ERROR("main: setgid");
  
  if (set_uid)
    if (setuid(set_uid) == -1)
      ERROR("main: setuid");

  /*
  ** Do some special handling if the "-b" or "-w" flags are used
  */
  if (background_flag)
  {
    int nfds, fd;
    fd_set read_set;


    /*
    ** Set up the SIGCHLD signal child termination handler so
    ** that we can avoid zombie processes hanging around and
    ** handle childs terminating before being able to complete the
    ** handshake.
    */
#if (defined(SVR4) || defined(hpux) || defined(__hpux) || \
     defined(_CRAY) || defined(_AUX_SOURCE))
    signal(SIGCHLD, SIG_IGN);
#else
    signal(SIGCHLD, (SIGRETURN_TYPE (*)()) child_handler);
#endif
    
    /*
    ** Loop and dispatch client handling processes
    */
    do
    {
      /*
      ** Terminate if we've been idle for 'timeout' seconds
      */
      if (background_flag == 2 && timeout)
      {
	signal(SIGALRM, alarm_handler);
	alarm(timeout);
      }
      
      /*
      ** Wait for a connection request to occur.
      ** Ignore EINTR (Interrupted System Call).
      */
      do
      {
	FD_ZERO(&read_set);
	FD_SET(0, &read_set);

	if (timeout)
	{
	  tv.tv_sec = timeout;
	  tv.tv_usec = 0;
	  nfds = select(FD_SETSIZE, &read_set, NULL, NULL, &tv);
	}
	else

	nfds = select(FD_SETSIZE, &read_set, NULL, NULL, NULL);
      } while (nfds < 0  && errno == EINTR);

      /*
      ** An error occured in select? Just die
      */
      if (nfds < 0)
	ERROR("main: select");

      /*
      ** Timeout limit reached. Exit nicely
      */
      if (nfds == 0)
	exit(0);
      
      /*
      ** Disable the alarm timeout
      */
      alarm(0);
      
      /*
      ** Accept the new client
      */
      fd = accept(0, NULL, NULL);
      if (fd == -1)
	ERROR1("main: accept. errno = %d", errno);
      
      /*
      ** And fork, then close the fd if we are the parent.
      */
      child_pid = fork();
    } while (child_pid && (close(fd), 1));

    /*
    ** We are now in child, the parent has returned to "do" above.
    */
    if (dup2(fd, 0) == -1)
      ERROR("main: dup2: failed fd 0");
    
    if (dup2(fd, 1) == -1)
      ERROR("main: dup2: failed fd 1");
    
    if (dup2(fd, 2) == -1)
      ERROR("main: dup2: failed fd 2");
  }

  /*
  ** Get foreign internet address
  */
  len = sizeof(sin);
  if (getpeername(0, (struct sockaddr *) &sin, &len) == -1)
  {
    /*
    ** A user has tried to start us from the command line or
    ** the network link died, in which case this message won't
    ** reach to other end anyway, so lets give the poor user some
    ** errors.
    */
    perror("in.identd: getpeername()");
    exit(1);
  }
  
  faddr = sin.sin_addr;


  /*
  ** Open the connection to the Syslog daemon if requested
  */
  if (syslog_flag)
  {
#ifdef LOG_DAEMON
    openlog("identd", LOG_PID, syslog_facility);
#else
    openlog("identd", LOG_PID);
#endif
    
    syslog(LOG_INFO, "Connection from %s", gethost(&faddr));
  }
  

  /*
  ** Get local internet address
  */
  len = sizeof(sin);
#ifdef ATTSVR4
  if (t_getsockname(0, (struct sockaddr *) &sin, &len) == -1)
#else
  if (getsockname(0, (struct sockaddr *) &sin, &len) == -1)
#endif
  {
    /*
    ** We can just die here, because if this fails then the
    ** network has died and we haven't got anyone to return
    ** errors to.
    */
    exit(1);
  }
  laddr = sin.sin_addr;


  /*
  ** Get the local/foreign port pair from the luser
  */
  parse(stdin, &laddr, &faddr);

  exit(0);
}
