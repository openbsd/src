/*	$OpenBSD: main.c,v 1.10 1996/07/27 12:38:06 deraadt Exp $	*/

/*
 * main.c - Point-to-Point Protocol main module
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: main.c,v 1.10 1996/07/27 12:38:06 deraadt Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <utmp.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <net/if.h>

#include "pppd.h"
#include "magic.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "upap.h"
#include "chap.h"
#include "ccp.h"
#include "pathnames.h"
#include "patchlevel.h"

#ifdef IPX_CHANGE
#include "ipxcp.h"
#endif /* IPX_CHANGE */

/*
 * If REQ_SYSOPTIONS is defined to 1, pppd will not run unless
 * /etc/ppp/options exists.
 */
#ifndef	REQ_SYSOPTIONS
#define REQ_SYSOPTIONS	1
#endif

/* interface vars */
char ifname[IFNAMSIZ];		/* Interface name */
int ifunit;			/* Interface unit number */

char *progname;			/* Name of this program */
char hostname[MAXNAMELEN];	/* Our hostname */
static char pidfilename[MAXPATHLEN];	/* name of pid file */
static char default_devnam[MAXPATHLEN];	/* name of default device */
static pid_t pid;		/* Our pid */
static uid_t uid;		/* Our real user-id */

int ttyfd = -1;			/* Serial port file descriptor */
mode_t tty_mode = -1;           /* Original access permissions to tty */
int baud_rate;                  /* Actual bits/second for serial device */
int hungup;                     /* terminal has been hung up */


int phase;			/* where the link is at */
int kill_link;
int open_ccp_flag;
int redirect_stderr;          /* Connector's stderr should go to file */

u_char outpacket_buf[PPP_MRU+PPP_HDRLEN]; /* buffer for outgoing packet */
u_char inpacket_buf[PPP_MRU+PPP_HDRLEN]; /* buffer for incoming packet */

int hungup;			/* terminal has been hung up */
static int n_children;		/* # child processes still running */

int baud_rate;			/* Actual bits/second for serial device */

static int locked;		/* lock() has succeeded */

char *no_ppp_msg = "Sorry - this system lacks PPP kernel support\n";

static char *restricted_environ[] = {
	"PATH=" _PATH_STDPATH,
	NULL
};

/* Prototypes for procedures local to this file. */

static void cleanup __P((void));
static void close_tty __P((void));
static void get_input __P((void));
static void connect_time_expired __P((caddr_t));
static void calltimeout __P((void));
static struct timeval *timeleft __P((struct timeval *));
static void hup __P((int));
static void term __P((int));
static void chld __P((int));
static void toggle_debug __P((int));
static void open_ccp __P((int));
static void bad_signal __P((int));
static void holdoff_end __P((void *));
static int device_script __P((char *, int, int));
static void reap_kids __P((void));
static void pr_log __P((void *, char *, ...));

extern	char	*ttyname __P((int));
extern	char	*getlogin __P((void));

#ifdef ultrix
#undef	O_NONBLOCK
#define	O_NONBLOCK	O_NDELAY
#endif

#ifdef PRIMITIVE_SYSLOG 
#define setlogmask(x)
#endif

/*
 * PPP Data Link Layer "protocol" table.
 * One entry per supported protocol.
 * The last entry must be NULL.
 */
struct protent *protocols[] = {
    &lcp_protent,
    &pap_protent,
    &chap_protent,
    &ipcp_protent,
    &ccp_protent,
#ifdef IPX_CHANGE
    &ipxcp_protent,
#endif
    NULL
};

int
main(argc, argv)
    int argc;
    char *argv[];
{
    int i, nonblock, fdflags;
    struct sigaction sa;
    FILE *pidfile;
    char *p;
    struct passwd *pw;
    struct timeval timo;
    sigset_t mask;
    struct protent *protp;
    struct stat statbuf;

    phase = PHASE_INITIALIZE;
    p = ttyname(0);
    if (p)
	strcpy(devnam, p);
    strcpy(default_devnam, devnam);

    /* Initialize syslog facilities */
#ifdef PRIMITIVE_SYSLOG
    openlog("pppd", LOG_PID);
#else
    openlog("pppd", LOG_PID | LOG_NDELAY, LOG_PPP);
    setlogmask(LOG_UPTO(LOG_INFO));
#endif

    if (gethostname(hostname, MAXNAMELEN) < 0 ) {
        syslog(LOG_ERR, "couldn't get hostname: %m");
	die(1);
    }
    hostname[MAXNAMELEN-1] = 0;

    uid = getuid();

    /*
     * Initialize to the standard option set, then parse, in order,
     * the system options file, the user's options file,
     * the tty's options file, and the command line arguments.
     */
    for (i = 0; (protp = protocols[i]) != NULL; ++i)
        (*protp->init)(0);
  
    progname = *argv;

    if (!options_from_file(_PATH_SYSOPTIONS, REQ_SYSOPTIONS, 0)
        || !options_from_user())
        exit(1);
    scan_args(argc-1, argv+1);  /* look for tty name on command line */
    if (!options_for_tty()
        || !parse_args(argc-1, argv+1))
	exit(1);

    if (!ppp_available()) {
	fprintf(stderr, no_ppp_msg);
	exit(1);
    }

    /*
     * Check that the options given are valid and consistent.
     */
    sys_check_options();
    auth_check_options();
    for (i = 0; (protp = protocols[i]) != NULL; ++i)
	if (protp->check_options != NULL)
	    (*protp->check_options)();
    if (demand && connector == 0) {
	fprintf(stderr, "%s: connect script required for demand-dialling\n",
		progname);
	exit(1);
    }

    /*
     * If the user has specified the default device name explicitly,
     * pretend they hadn't.
     */
    if (!default_device && strcmp(devnam, default_devnam) == 0)
	default_device = 1;
    redirect_stderr = !nodetach || default_device;


    /*
     * Initialize system-dependent stuff and magic number package.
     */
    sys_init();
    magic_init();
    if (debug)
      setlogmask(LOG_UPTO(LOG_DEBUG));


    /*
     * Detach ourselves from the terminal, if required,
     * and identify who is running us.
     */
    if (!default_device && !nodetach && daemon(0, 0) < 0) {
	perror("Couldn't detach from controlling terminal");
	exit(1);
    }
    pid = getpid();
    p = getlogin();
    if (p == NULL) {
	pw = getpwuid(uid);
	if (pw != NULL && pw->pw_name != NULL)
	    p = pw->pw_name;
	else
	    p = "(unknown)";
    }
    syslog(LOG_NOTICE, "pppd %s.%d started by %s, uid %d",
	   VERSION, PATCHLEVEL, p, uid);
  
    /*
     * Compute mask of all interesting signals and install signal handlers
     * for each.  Only one signal handler may be active at a time.  Therefore,
     * all other signals should be masked when any handler is executing.
     */
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGCHLD);

#define SIGNAL(s, handler)	{ \
	sa.sa_handler = handler; \
	if (sigaction(s, &sa, NULL) < 0) { \
	    syslog(LOG_ERR, "Couldn't establish signal handler (%d): %m", s); \
	    die(1); \
	} \
    }

    sa.sa_mask = mask;
    sa.sa_flags = 0;
    SIGNAL(SIGHUP, hup);		/* Hangup */
    SIGNAL(SIGINT, term);		/* Interrupt */
    SIGNAL(SIGTERM, term);		/* Terminate */
    SIGNAL(SIGCHLD, chld);

    SIGNAL(SIGUSR1, toggle_debug);	/* Toggle debug flag */
    SIGNAL(SIGUSR2, open_ccp);		/* Reopen CCP */

    /*
     * Install a handler for other signals which would otherwise
     * cause pppd to exit without cleaning up.
     */
    SIGNAL(SIGABRT, bad_signal);
    SIGNAL(SIGALRM, bad_signal);
    SIGNAL(SIGFPE, bad_signal);
    SIGNAL(SIGILL, bad_signal);
    SIGNAL(SIGPIPE, bad_signal);
    SIGNAL(SIGQUIT, bad_signal);
    SIGNAL(SIGSEGV, bad_signal);
#ifdef SIGBUS
    SIGNAL(SIGBUS, bad_signal);
#endif
#ifdef SIGEMT
    SIGNAL(SIGEMT, bad_signal);
#endif
#ifdef SIGPOLL
    SIGNAL(SIGPOLL, bad_signal);
#endif
#ifdef SIGPROF
    SIGNAL(SIGPROF, bad_signal);
#endif
#ifdef SIGSYS
    SIGNAL(SIGSYS, bad_signal);
#endif
#ifdef SIGTRAP
    SIGNAL(SIGTRAP, bad_signal);
#endif
#ifdef SIGVTALRM
    SIGNAL(SIGVTALRM, bad_signal);
#endif
#ifdef SIGXCPU
    SIGNAL(SIGXCPU, bad_signal);
#endif
#ifdef SIGXFSZ
    SIGNAL(SIGXFSZ, bad_signal);
#endif

    /*
     * Apparently we can get a SIGPIPE when we call syslog, if
     * syslogd has died and been restarted.  Ignoring it seems
     * be sufficient.
     */
    signal(SIGPIPE, SIG_IGN);

    /*
     * If we're doing dial-on-demand, set up the interface now.
     */
    if (demand) {
	/*
	 * Open the loopback channel and set it up to be the ppp interface.
	 */
	open_ppp_loopback();

	syslog(LOG_INFO, "Using interface ppp%d", ifunit);
	(void) sprintf(ifname, "ppp%d", ifunit);

	/* write pid to file */
	(void) sprintf(pidfilename, "%s%s.pid", _PATH_VARRUN, ifname);
	if ((pidfile = fopen(pidfilename, "w")) != NULL) {
	    fprintf(pidfile, "%d\n", pid);
	    (void) fclose(pidfile);
	} else {
	    syslog(LOG_ERR, "Failed to create pid file %s: %m", pidfilename);
	    pidfilename[0] = 0;
	}

	/*
	 * Configure the interface and mark it up, etc.
	 */
	demand_conf();
    }

    for (;;) {

	if (demand) {
	    /*
	     * Don't do anything until we see some activity.
	     */
	    phase = PHASE_DORMANT;
	    kill_link = 0;
	    demand_unblock();
	    for (;;) {
		wait_loop_output(timeleft(&timo));
		calltimeout();
		if (kill_link) {
		    if (!persist)
			die(0);
		    kill_link = 0;
		}
		if (get_loop_output())
		    break;
		reap_kids();
	    }

	    /*
	     * Now we want to bring up the link.
	     */
	    demand_block();
	    syslog(LOG_INFO, "Starting link");
	}

	/*
	 * Lock the device if we've been asked to.
	 */
	if (lockflag && !default_device) {
	    if (lock(devnam) < 0)
		goto fail;
	    locked = 1;
	}

	/*
	 * Open the serial device and set it up to be the ppp interface.
	 * If we're dialling out, or we don't want to use the modem lines,
	 * we open it in non-blocking mode, but then we need to clear
	 * the non-blocking I/O bit.
	 */
	nonblock = (connector || !modem)? O_NONBLOCK: 0;
	if ((ttyfd = open(devnam, nonblock | O_RDWR, 0)) < 0) {
	    syslog(LOG_ERR, "Failed to open %s: %m", devnam);
	    goto fail;
	}
	if (nonblock) {
	    if ((fdflags = fcntl(ttyfd, F_GETFL)) == -1
		|| fcntl(ttyfd, F_SETFL, fdflags & ~O_NONBLOCK) < 0)
		syslog(LOG_WARNING,
		       "Couldn't reset non-blocking mode on device: %m");
	}
	hungup = 0;
	kill_link = 0;

        /*
         * Do the equivalent of `mesg n' to stop broadcast messages.
         */
        if (fstat(ttyfd, &statbuf) < 0
            || fchmod(ttyfd, statbuf.st_mode & ~(S_IWGRP | S_IWOTH)) < 0) {
            syslog(LOG_WARNING,
                   "Couldn't restrict write permissions to %s: %m", devnam);
        } else
            tty_mode = statbuf.st_mode;

	/* run connection script */
	if (connector && connector[0]) {
	    MAINDEBUG((LOG_INFO, "Connecting with <%s>", connector));

	    /* set line speed, flow control, etc.; set CLOCAL for now */
	    set_up_tty(ttyfd, 1);

	    /* drop dtr to hang up in case modem is off hook */
	    if (!default_device && modem) {
		setdtr(ttyfd, FALSE);
		sleep(1);
		setdtr(ttyfd, TRUE);
	    }

	    if (device_script(connector, ttyfd, ttyfd) < 0) {
		syslog(LOG_ERR, "Connect script failed");
		setdtr(ttyfd, FALSE);
		goto fail;
	    }

	    syslog(LOG_INFO, "Serial connection established.");
	    sleep(1);		/* give it time to set up its terminal */
	}

	/* set line speed, flow control, etc.; clear CLOCAL if modem option */
	set_up_tty(ttyfd, 0);

	/* run welcome script, if any */
	if (welcomer && welcomer[0]) {
	    if (device_script(welcomer, ttyfd, ttyfd) < 0)
		syslog(LOG_WARNING, "Welcome script failed");
	}

	/* set up the serial device as a ppp interface */
	establish_ppp(ttyfd);

	if (!demand) {
	    
	    syslog(LOG_INFO, "Using interface ppp%d", ifunit);
	    (void) sprintf(ifname, "ppp%d", ifunit);
	    
	    /* write pid to file */
	    (void) sprintf(pidfilename, "%s%s.pid", _PATH_VARRUN, ifname);
	    if ((pidfile = fopen(pidfilename, "w")) != NULL) {
		fprintf(pidfile, "%d\n", pid);
		(void) fclose(pidfile);
	    } else {
		syslog(LOG_ERR, "Failed to create pid file %s: %m",
		       pidfilename);
		pidfilename[0] = 0;
	    }
	}

	/*
	 * Set a timeout to close the connection once the maximum
	 * connect time has expired.
	 */
	if (maxconnect > 0)
	    TIMEOUT(connect_time_expired, 0, maxconnect);

	/*
	 * Start opening the connection and wait for
	 * incoming events (reply, timeout, etc.).
	 */
	syslog(LOG_NOTICE, "Connect: %s <--> %s", ifname, devnam);
	lcp_lowerup(0);
	lcp_open(0);		/* Start protocol */
	for (phase = PHASE_ESTABLISH; phase != PHASE_DEAD; ) {
	    wait_input(timeleft(&timo));
	    calltimeout();
	    get_input();
	    if (kill_link) {
		lcp_close(0, "User request");
		phase = PHASE_TERMINATE;
		kill_link = 0;
	    }
	    if (open_ccp_flag) {
		if (phase == PHASE_NETWORK) {
		    ccp_fsm[0].flags = OPT_RESTART; /* clears OPT_SILENT */
                    (*ccp_protent.open)(0);
		}
		open_ccp_flag = 0;
	    }
	    reap_kids();	/* Don't leave dead kids lying around */
	}

	/*
	 * If we may want to bring the link up again, transfer
	 * the ppp unit back to the loopback.  Set the
	 * real serial device back to its normal mode of operation.
	 */
	clean_check();
	if (demand)
	    restore_loop();
	disestablish_ppp(ttyfd);

	/*
	 * Run disconnector script, if requested.
	 * XXX we may not be able to do this if the line has hung up!
	 */
	if (disconnector && !hungup) {
	    set_up_tty(ttyfd, 1);
	    if (device_script(disconnector, ttyfd, ttyfd) < 0) {
		syslog(LOG_WARNING, "disconnect script failed");
	    } else {
		syslog(LOG_INFO, "Serial link disconnected.");
	    }
	}

    fail:
	close_tty();
	if (locked) {
	    unlock();
	    locked = 0;
	}

	if (!demand) {
            if (pidfilename[0] != 0
                && unlink(pidfilename) < 0 && errno != ENOENT)
		syslog(LOG_WARNING, "unable to delete pid file: %m");
	    pidfilename[0] = 0;
	}

	if (!persist)
	    break;

	if (demand)
	    demand_discard();
	if (holdoff > 0) {
	    phase = PHASE_HOLDOFF;
	    TIMEOUT(holdoff_end, NULL, holdoff);
	    do {
		wait_time(timeleft(&timo));
		calltimeout();
		if (kill_link) {
		    if (!persist)
			die(0);
		    kill_link = 0;
		    phase = PHASE_DORMANT; /* allow signal to end holdoff */
		}
		reap_kids();
	    } while (phase == PHASE_HOLDOFF);
	}
    }

    die(0);
}

/*
 * holdoff_end - called via a timeout when the holdoff period ends.
 */
static void
holdoff_end(arg)
    void *arg;
{
    phase = PHASE_DORMANT;
}

/*
 * get_input - called when incoming data is available.
 */
static void
get_input()
{
    int len, i;
    u_char *p;
    u_short protocol;
    struct protent *protp;

    p = inpacket_buf;	/* point to beginning of packet buffer */

    len = read_packet(inpacket_buf);
    if (len < 0)
	return;

    if (len == 0) {
	syslog(LOG_NOTICE, "Modem hangup");
	hungup = 1;
	lcp_lowerdown(0);	/* serial link is no longer available */
	link_terminated(0);
	return;
    }

    if (debug /*&& (debugflags & DBG_INPACKET)*/)
	log_packet(p, len, "rcvd ");

    if (len < PPP_HDRLEN) {
	MAINDEBUG((LOG_INFO, "io(): Received short packet."));
	return;
    }

    p += 2;				/* Skip address and control */
    GETSHORT(protocol, p);
    len -= PPP_HDRLEN;

    /*
     * Toss all non-LCP packets unless LCP is OPEN.
     */
    if (protocol != PPP_LCP && lcp_fsm[0].state != OPENED) {
	MAINDEBUG((LOG_INFO,
		   "io(): Received non-LCP packet when LCP not open."));
	return;
    }

    /*
     * Upcall the proper protocol input routine.
     */
    for (i = 0; (protp = protocols[i]) != NULL; ++i) {
	if (protp->protocol == protocol && protp->enabled_flag) {
	    (*protp->input)(0, p, len);
	    return;
	}
        if (protocol == (protp->protocol & ~0x8000) && protp->enabled_flag
	    && protp->datainput != NULL) {
	    (*protp->datainput)(0, p, len);
	    return;
	}
    }

    if (debug)
    	syslog(LOG_WARNING, "Unsupported protocol (0x%x) received", protocol);
    lcp_sprotrej(0, p - PPP_HDRLEN, len + PPP_HDRLEN);
}


/*
 * quit - Clean up state and exit (with an error indication).
 */
void 
quit()
{
    die(1);
}

/*
 * die - like quit, except we can specify an exit status.
 */
void
die(status)
    int status;
{
    cleanup();
    syslog(LOG_INFO, "Exit.");
    exit(status);
}

/*
 * connect_time_expired - log a message and close the connection.
 */
static void
connect_time_expired(arg)
    caddr_t arg;
{
    syslog(LOG_INFO, "Connect time expired");

    phase = PHASE_TERMINATE;
    lcp_close(0, "Connect time expired");	/* Close connection */
}

/*
 * cleanup - restore anything which needs to be restored before we exit
 */
/* ARGSUSED */
static void
cleanup()
{
    sys_cleanup();

    if (ttyfd >= 0)
	close_tty();

    if (pidfilename[0] != 0 && unlink(pidfilename) < 0 && errno != ENOENT) 
	syslog(LOG_WARNING, "unable to delete pid file: %m");
    pidfilename[0] = 0;

    if (locked)
	unlock();
}

/*
 * close_tty - restore the terminal device and close it.
 */
static void
close_tty()
{
    disestablish_ppp(ttyfd);

    /* drop dtr to hang up */
    if (modem) {
	setdtr(ttyfd, FALSE);
	/*
	 * This sleep is in case the serial port has CLOCAL set by default,
	 * and consequently will reassert DTR when we close the device.
	 */
	sleep(1);
    }

    restore_tty(ttyfd);

    if (tty_mode != (mode_t) -1)
      chmod(devnam, tty_mode);

    close(ttyfd);
    ttyfd = -1;
}


struct	callout {
    struct timeval	c_time;		/* time at which to call routine */
    caddr_t		c_arg;		/* argument to routine */
    void		(*c_func)();	/* routine */
    struct		callout *c_next;
};

static struct callout *callout = NULL;	/* Callout list */
static struct timeval timenow;		/* Current time */

/*
 * timeout - Schedule a timeout.
 *
 * Note that this timeout takes the number of seconds, NOT hz (as in
 * the kernel).
 */
void
timeout(func, arg, time)
    void (*func)();
    caddr_t arg;
    int time;
{
    struct callout *newp, *p, **pp;
  
    MAINDEBUG((LOG_DEBUG, "Timeout %lx:%lx in %d seconds.",
	       (long) func, (long) arg, time));
  
    /*
     * Allocate timeout.
     */
    if ((newp = (struct callout *) malloc(sizeof(struct callout))) == NULL) {
	syslog(LOG_ERR, "Out of memory in timeout()!");
	die(1);
    }
    newp->c_arg = arg;
    newp->c_func = func;
    gettimeofday(&timenow, NULL);
    newp->c_time.tv_sec = timenow.tv_sec + time;
    newp->c_time.tv_usec = timenow.tv_usec;
  
    /*
     * Find correct place and link it in.
     */
    for (pp = &callout; (p = *pp); pp = &p->c_next)
	if (newp->c_time.tv_sec < p->c_time.tv_sec
	    || (newp->c_time.tv_sec == p->c_time.tv_sec
		&& newp->c_time.tv_usec < p->c_time.tv_sec))
	    break;
    newp->c_next = p;
    *pp = newp;
}


/*
 * untimeout - Unschedule a timeout.
 */
void
untimeout(func, arg)
    void (*func)();
    caddr_t arg;
{
    struct callout **copp, *freep;
  
    MAINDEBUG((LOG_DEBUG, "Untimeout %lx:%lx.", (long) func, (long) arg));
  
    /*
     * Find first matching timeout and remove it from the list.
     */
    for (copp = &callout; (freep = *copp); copp = &freep->c_next)
	if (freep->c_func == func && freep->c_arg == arg) {
	    *copp = freep->c_next;
	    (void) free((char *) freep);
	    break;
	}
}


/*
 * calltimeout - Call any timeout routines which are now due.
 */
static void
calltimeout()
{
    struct callout *p;

    while (callout != NULL) {
	p = callout;

	if (gettimeofday(&timenow, NULL) < 0) {
	    syslog(LOG_ERR, "Failed to get time of day: %m");
	    die(1);
	}
	if (!(p->c_time.tv_sec < timenow.tv_sec
	      || (p->c_time.tv_sec == timenow.tv_sec
		  && p->c_time.tv_usec <= timenow.tv_usec)))
	    break;		/* no, it's not time yet */

	callout = p->c_next;
	(*p->c_func)(p->c_arg);

	free((char *) p);
    }
}


/*
 * timeleft - return the length of time until the next timeout is due.
 */
static struct timeval *
timeleft(tvp)
    struct timeval *tvp;
{
    if (callout == NULL)
	return NULL;

    gettimeofday(&timenow, NULL);
    tvp->tv_sec = callout->c_time.tv_sec - timenow.tv_sec;
    tvp->tv_usec = callout->c_time.tv_usec - timenow.tv_usec;
    if (tvp->tv_usec < 0) {
	tvp->tv_usec += 1000000;
	tvp->tv_sec -= 1;
    }
    if (tvp->tv_sec < 0)
	tvp->tv_sec = tvp->tv_usec = 0;

    return tvp;
}
    

/*
 * hup - Catch SIGHUP signal.
 *
 * Indicates that the physical layer has been disconnected.
 * We don't rely on this indication; if the user has sent this
 * signal, we just take the link down.
 */
static void
hup(sig)
    int sig;
{
    syslog(LOG_INFO, "Hangup (SIGHUP)");
    kill_link = 1;
}


/*
 * term - Catch SIGTERM signal and SIGINT signal (^C/del).
 *
 * Indicates that we should initiate a graceful disconnect and exit.
 */
/*ARGSUSED*/
static void
term(sig)
    int sig;
{
    syslog(LOG_INFO, "Terminating on signal %d.", sig);
    persist = 0;		/* don't try to restart */
    kill_link = 1;
}


/*
 * chld - Catch SIGCHLD signal.
 * Calls reap_kids to get status for any dead kids.
 */
static void
chld(sig)
    int sig;
{
    reap_kids();
}


/*
 * toggle_debug - Catch SIGUSR1 signal.
 *
 * Toggle debug flag.
 */
/*ARGSUSED*/
static void
toggle_debug(sig)
    int sig;
{
    debug = !debug;
    if (debug) {
      setlogmask(LOG_UPTO(LOG_DEBUG));
    } else {
      setlogmask(LOG_UPTO(LOG_WARNING));
    }
}


/*
 * open_ccp - Catch SIGUSR2 signal.
 *
 * Try to (re)negotiate compression.
 */
/*ARGSUSED*/
static void
open_ccp(sig)
    int sig;
{
    open_ccp_flag = 1;
}


/*
 * bad_signal - We've caught a fatal signal.  Clean up state and exit.
 */
static void
bad_signal(sig)
    int sig;
{
    syslog(LOG_ERR, "Fatal signal %d", sig);
    die(1);
}


/*
 * device_script - run a program to connect or disconnect the
 * serial device.
 */
static int
device_script(program, in, out)
    char *program;
    int in, out;
{
    int pid;
    int status;
    int errfd;

    pid = fork();

    if (pid < 0) {
	syslog(LOG_ERR, "Failed to create child process: %m");
	die(1);
    }

    if (pid == 0) {
	sys_close();
        closelog();
        if (in == out) {   
            if (in != 0) {
                dup2(in, 0);
                close(in);
            }
            dup2(0, 1);
        } else {
            if (out == 0)
                out = dup(out);
            if (in != 0) {
                dup2(in, 0);
                close(in);
            }
            if (out != 1) {
	        dup2(out, 1);
                close(out);
            }
        }
        if (redirect_stderr) {
            close(2);
            errfd = open(_PATH_CONNERRS, O_WRONLY | O_APPEND | O_CREAT, 0644);
            if (errfd >= 0 && errfd != 2) {
	        dup2(errfd, 2);
                close(errfd);
            }
        }
	setuid(getuid());
	setgid(getgid());
	execl("/bin/sh", "sh", "-c", program, (char *)0);
	syslog(LOG_ERR, "could not exec /bin/sh: %m");
	_exit(99);
	/* NOTREACHED */
    }

    while (waitpid(pid, &status, 0) < 0) {
	if (errno == EINTR)
	    continue;
	syslog(LOG_ERR, "error waiting for (dis)connection process: %m");
	die(1);
    }

    return (status == 0 ? 0 : -1);
}


/*
 * run-program - execute a program with given arguments,
 * but don't wait for it.
 * If the program can't be executed, logs an error unless
 * must_exist is 0 and the program file doesn't exist.
 */
int
run_program(prog, args, must_exist)
    char *prog;
    char **args;
    int must_exist;
{
    int pid;

    pid = fork();
    if (pid == -1) {
	syslog(LOG_ERR, "Failed to create child process for %s: %m", prog);
	return -1;
    }
    if (pid == 0) {
	int new_fd;

	/* Leave the current location */
	(void) setsid();    /* No controlling tty. */
	(void) umask (S_IRWXG|S_IRWXO);
	(void) chdir ("/"); /* no current directory. */
	setuid(geteuid());
	setgid(getegid());

	/* Ensure that nothing of our device environment is inherited. */
	sys_close();
        closelog();  
	close (0);
	close (1);
	close (2);
	close (ttyfd);  /* tty interface to the ppp device */

        /* Don't pass handles to the PPP device, even by accident. */
	new_fd = open (_PATH_DEVNULL, O_RDWR);
	if (new_fd >= 0) {
	    if (new_fd != 0) {
	        dup2  (new_fd, 0); /* stdin <- /dev/null */
		close (new_fd);
	    }
	    dup2 (0, 1); /* stdout -> /dev/null */
	    dup2 (0, 2); /* stderr -> /dev/null */
	}

#ifdef BSD
	/* Force the priority back to zero if pppd is running higher. */
	if (setpriority (PRIO_PROCESS, 0, 0) < 0)
	    syslog (LOG_WARNING, "can't reset priority to 0: %m"); 
#endif

	/* SysV recommends a second fork at this point. */

        /* run the program; give it a null environment */
        execve(prog, args, restricted_environ);
	if (must_exist || errno != ENOENT)
	    syslog(LOG_WARNING, "Can't execute %s: %m", prog);
	_exit(-1);
    }
    MAINDEBUG((LOG_DEBUG, "Script %s started; pid = %d", prog, pid));
    ++n_children;
    return 0;
}


/*
 * reap_kids - get status from any dead child processes,
 * and log a message for abnormal terminations.
 */
static void
reap_kids()
{
    int pid, status;

    if (n_children == 0)
	return;
    if ((pid = waitpid(-1, &status, WNOHANG)) == -1) {
	if (errno != ECHILD)
	    syslog(LOG_ERR, "Error waiting for child process: %m");
	return;
    }
    if (pid > 0) {
	--n_children;
	if (WIFSIGNALED(status)) {
	    syslog(LOG_WARNING, "Child process %d terminated with signal %d",
		   pid, WTERMSIG(status));
	}
    }
}


/*
 * log_packet - format a packet and log it.
 */

char line[256];			/* line to be logged accumulated here */
char *linep;

void
log_packet(p, len, prefix)
    u_char *p;
    int len;
    char *prefix;
{
    strcpy(line, prefix);
    linep = line + strlen(line);
    format_packet(p, len, pr_log, NULL);
    if (linep != line)
	syslog(LOG_DEBUG, "%s", line);
}

/*
 * format_packet - make a readable representation of a packet,
 * calling `printer(arg, format, ...)' to output it.
 */
void
format_packet(p, len, printer, arg)
    u_char *p;
    int len;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int i, n;
    u_short proto;
    u_char x;
    struct protent *protp;

    if (len >= PPP_HDRLEN && p[0] == PPP_ALLSTATIONS && p[1] == PPP_UI) {
	p += 2;
	GETSHORT(proto, p);
	len -= PPP_HDRLEN;
	for (i = 0; (protp = protocols[i]) != NULL; ++i)
	    if (proto == protp->protocol)
		break;
	if (protp != NULL) {
	    printer(arg, "[%s", protp->name);
	    n = (*protp->printpkt)(p, len, printer, arg);
	    printer(arg, "]");
	    p += n;
	    len -= n;
	} else {
	    printer(arg, "[proto=0x%x]", proto);
	}
    }

    for (; len > 0; --len) {
	GETCHAR(x, p);
	printer(arg, " %.2x", x);
    }
}

static void
pr_log __V((void *arg, char *fmt, ...))
{
    int n;
    va_list pvar;
    char buf[256];

#if __STDC__
    va_start(pvar, fmt);
#else
    void *arg;
    char *fmt;
    va_start(pvar);
    arg = va_arg(pvar, void *);
    fmt = va_arg(pvar, char *);
#endif

    vsprintf(buf, fmt, pvar);
    va_end(pvar);

    n = strlen(buf);
    if (linep + n + 1 > line + sizeof(line)) {
	syslog(LOG_DEBUG, "%s", line);
	linep = line;
    }
    strcpy(linep, buf);
    linep += n;
}

/*
 * print_string - print a readable representation of a string using
 * printer.
 */
void
print_string(p, len, printer, arg)
    char *p;
    int len;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int c;

    printer(arg, "\"");
    for (; len > 0; --len) {
	c = *p++;
        if (' ' <= c && c <= '~') {
            if (c == '\\' || c == '"')
                printer(arg, "\\");
	    printer(arg, "%c", c);
        } else {
            switch (c) {
            case '\n':
                printer(arg, "\\n");
                break;
            case '\r':
                printer(arg, "\\r");
                break;   
            case '\t':
                printer(arg, "\\t");
                break;
            default:
	        printer(arg, "\\%.3o", c);
            }
      }
    }

    printer(arg, "\"");
}

/*
 * novm - log an error message saying we ran out of memory, and die.
 */
void
novm(msg)
    char *msg;
{
    syslog(LOG_ERR, "Virtual memory exhausted allocating %s\n", msg);
    die(1);
}

/*
 * fmtmsg - format a message into a buffer.  Like sprintf except we
 * also specify the length of the output buffer, and we handle
 * %r (recursive format), %m (error message) and %I (IP address) formats.
 * Doesn't do floating-point formats.
 * Returns the number of chars put into buf.
 */
int
fmtmsg __V((char *buf, int buflen, char *fmt, ...))
{
    va_list args;
    int n;
                
#if __STDC__
    va_start(args, fmt);
#else
    char *buf;
    int buflen;
    char *fmt;
    va_start(args);
    buf = va_arg(args, char *);
    buflen = va_arg(args, int);
    fmt = va_arg(args, char *);
#endif
    n = vfmtmsg(buf, buflen, fmt, args);
    va_end(args);
    return n;
}
 
/*
 * vfmtmsg - like fmtmsg, takes a va_list instead of a list of args.
 */
#define OUTCHAR(c)      (buflen > 0? (--buflen, *buf++ = (c)): 0)
   
int
vfmtmsg(char *buf, int buflen, char *fmt, va_list args)
{
    int c, i, n;
    int width, prec, fillch;
    int base, len, neg, quoted;
    unsigned long val;  
    char *str, *f, *buf0;
    unsigned char *p;
    va_list a; 
    char num[32];
    time_t t;
    static char hexchars[16] = "0123456789abcdef";
    
    buf0 = buf;
    --buflen;
    while (buflen > 0) {
        for (f = fmt; *f != '%' && *f != 0; ++f)
            ;
        if (f > fmt) {
            len = f - fmt;
            if (len > buflen)
                len = buflen;
            memcpy(buf, fmt, len);
            buf += len;
            buflen -= len;
            fmt = f;
        }
        if (*fmt == 0)
            break;
        c = *++fmt;
        width = prec = 0;
        fillch = ' ';   
        if (c == '0') {  
            fillch = '0';
            c = *++fmt;
        }
        if (c == '*') {
            width = va_arg(args, int);
            c = *++fmt;
        } else {
            while (isdigit(c)) {
                width = width * 10 + c - '0';
                c = *++fmt;
            }
        }
        if (c == '.') {   
            c = *++fmt;
            if (c == '*') {  
                prec = va_arg(args, int);
                c = *++fmt;
            } else {
                while (isdigit(c)) {
                    prec = prec * 10 + c - '0';
                    c = *++fmt;
                } 
            }
        }
        str = 0;
        base = 0;
        neg = 0;
        ++fmt;
        switch (c) {
        case 'd':
            i = va_arg(args, int);
            if (i < 0) {
                neg = 1;
                val = -i;
            } else
                val = i;   
            base = 10;
            break;
        case 'o':
            val = va_arg(args, unsigned int);
            base = 8;
            break;
        case 'x':
            val = va_arg(args, unsigned int);
            base = 16;
            break;
        case 'p':
            val = (unsigned long) va_arg(args, void *);
            base = 16;
            neg = 2;
            break;
        case 's':
            str = va_arg(args, char *);
            break;
        case 'c':   
            num[0] = va_arg(args, int);
            num[1] = 0;
            str = num;  
            break;
        case 'm':
            str = strerror(errno);
            break;
        case 'I':
            str = ip_ntoa(va_arg(args, u_int32_t));
            break;
        case 'r':
            f = va_arg(args, char *);
            a = va_arg(args, va_list);
            n = vfmtmsg(buf, buflen + 1, f, a);
            buf += n;
            buflen -= n;
            continue;
        case 't':
            time(&t);
            str = ctime(&t);
            str += 4;           /* chop off the day name */
            str[15] = 0;        /* chop off year and newline */
            break;
        case 'v':               /* "visible" string */
        case 'q':               /* quoted string */
            quoted = c == 'q';
            p = va_arg(args, unsigned char *);
            if (fillch == '0' && prec > 0) {
                n = prec;
            } else {
                n = strlen((char *)p);
                if (prec > 0 && prec < n)
                    n = prec;
            }
            while (n > 0 && buflen > 0) {
                c = *p++;
                --n;
                if (!quoted && c >= 0x80) {
                    OUTCHAR('M');
                    OUTCHAR('-');
                    c -= 0x80;
                }
                if (quoted && (c == '"' || c == '\\'))
                    OUTCHAR('\\');
                if (c < 0x20 || 0x7f <= c && c < 0xa0) {
                    if (quoted) {
                        OUTCHAR('\\');
                        switch (c) {
                        case '\t':      OUTCHAR('t');   break;
                        case '\n':      OUTCHAR('n');   break;
                        case '\b':      OUTCHAR('b');   break;
                        case '\f':      OUTCHAR('f');   break;
                        default:
                            OUTCHAR('x');   
                            OUTCHAR(hexchars[c >> 4]);
                            OUTCHAR(hexchars[c & 0xf]);
                        }
                    } else {
                        if (c == '\t')
                            OUTCHAR(c);
                        else {
                            OUTCHAR('^');
                            OUTCHAR(c ^ 0x40);
                        }
                    }
                } else
                    OUTCHAR(c);
            }
            continue;
        default:
            *buf++ = '%';
            if (c != '%')
                --fmt;          /* so %z outputs %z etc. */
            --buflen;
            continue;
        }
        if (base != 0) {
            str = num + sizeof(num);
            *--str = 0;
            while (str > num + neg) {
                *--str = hexchars[val % base];
                val = val / base;
                if (--prec <= 0 && val == 0)
                    break;  
            }
            switch (neg) {
            case 1:
                *--str = '-';
                break;
            case 2:
                *--str = 'x';
                *--str = '0';
                break;
            }
            len = num + sizeof(num) - 1 - str;
        } else {
            len = strlen(str);
            if (prec > 0 && len > prec)
                len = prec;
        }
        if (width > 0) {
            if (width > buflen)
                width = buflen;
            if ((n = width - len) > 0) {
                buflen -= n;
                for (; n > 0; --n)   
                    *buf++ = fillch;
            }
        }
        if (len > buflen)   
            len = buflen;
        memcpy(buf, str, len);
        buf += len;
        buflen -= len;
    }
    *buf = 0;
    return buf - buf0;
}
                
