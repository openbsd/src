/* $OpenBSD: netcat.c,v 1.19 2001/01/16 20:20:48 ericj Exp $ */

/* Netcat 1.10 RELEASE 960320
 *
 *   A damn useful little "backend" utility begun 950915 or thereabouts,
 *   as *Hobbit*'s first real stab at some sockets programming.  Something that
 *   should have and indeed may have existed ten years ago, but never became a
 *   standard Unix utility.  IMHO, "nc" could take its place right next to cat,
 *   cp, rm, mv, dd, ls, and all those other cryptic and Unix-like things.
 *
 *   Read the README for the whole story, doc, applications, etc.
 *
 *   Layout:
 *	conditional includes:
 *	includes:
 *	handy defines:
 *	globals:
 *	malloced globals:
 *	cmd-flag globals:
 *	support routines:
 *	readwrite select loop:
 *	main:
 *
 *  bluesky:
 *	parse ranges of IP address as well as ports, perhaps
 *	RAW mode!
 *	backend progs to grab a pty and look like a real telnetd?!
 *	backend progs to do various encryption modes??!?!
*/


#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>		/* hostent, gethostby*, getservby* */
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <fcntl.h>
#include <resolv.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#define SLEAZE_PORT 31337	/* for UDP-scan RTT trick, change if ya want */
#define BIGSIZ 8192		/* big buffers */

struct host_info {
	char	name[MAXHOSTNAMELEN];	/* DNS name */
	char	addrs[8][24];		/* ascii-format IP addresses */
	struct 	in_addr iaddrs[8];	/* in_addr.s_addr: ulong */
};

struct port_info {
	char    name[64];	/* name in /etc/services */
	char    anum[8];	/* ascii-format number */
	u_short  num;		/* real host-order number */
};

/* globals: */
jmp_buf jbuf;			/* timer jump buffer*/
int     jval = 0;		/* timer crud */
int     netfd = -1;
int     ofd = 0;		/* hexdump output fd */

int     gatesidx = 0;		/* LSRR hop count */
int     gatesptr = 4;		/* initial LSRR pointer, settable */
u_short  Single = 1;		/* zero if scanning */
unsigned int insaved = 0;	/* stdin-buffer size for multi-mode */
unsigned int wrote_out = 0;	/* total stdout bytes */
unsigned int wrote_net = 0;	/* total net bytes */
static char hexnibs[20] = "0123456789abcdef  ";

/* will malloc up the following globals: */
struct timeval timer1, timer2;
struct sockaddr_in    *lclend = NULL;	/* sockaddr_in structs */
struct sockaddr_in    *remend = NULL;
struct host_info  **gates = NULL;	/* LSRR hop hinfo */
char   *optbuf = NULL;			/* LSRR or sockopts */
char   *bigbuf_in;			/* data buffers */
char   *bigbuf_net;
fd_set fds1, fds2;
struct port_info   *pinfo = NULL;	/* for getpinfo / getservby* */
unsigned char *stage = NULL;		/* hexdump line buffer */

/* global cmd flags: */
u_short  o_alla = 0;
unsigned int o_interval = 0;
u_short  o_listen = 0;
u_short  o_nflag = 0;
u_short  o_wfile = 0;
u_short  o_random = 0;
u_short  o_udpmode = 0;
u_short  o_verbose = 0;
unsigned int o_wait = 0;
u_short  o_zero = 0;

/* Function Prototype's */
void	help __P(());
void	nlog __P((int, char *, ...));
void	usage __P((int));

/* 
 * support routines -- the bulk of this thing.  Placed in such an order that
 * we don't have to forward-declare anything: 
 */

/* 
 * catch :
 * no-brainer interrupt handler
 */
void 
catch()
{
	if (o_verbose)	/* normally we don't care */
		nlog(1, "Sent %i Rcvd %i", wrote_net, wrote_out);
	nlog(1, " punt!");
}

/* timeout and other signal handling cruft */
void 
tmtravel()
{
	signal(SIGALRM, SIG_IGN);
	alarm(0);
	if (jval == 0)
		nlog(1, "spurious timer interrupt!");
	longjmp(jbuf, jval);
}

/*
 * arm :
 * set the timer.  Zero secs arg means unarm
 */
void 
arm(num, secs)
	unsigned int num;
	unsigned int secs;
{
	if (secs == 0) {
		signal(SIGALRM, SIG_IGN);
		alarm(0);
		jval = 0;
	} else {
		signal(SIGALRM, tmtravel);
		alarm(secs);
		jval = num;
	}	
}

/*
 * findline :
 * find the next newline in a buffer; return inclusive size of that "line",
 * or the entire buffer size, so the caller knows how much to then write().
 * Not distinguishing \n vs \r\n for the nonce; it just works as is... 
 */
unsigned int 
findline(buf, siz)
	char   *buf;
	unsigned int siz;
{
	char *p;
	int x;
	if (!buf)		/* various sanity checks... */
		return (0);
	if (siz > BIGSIZ)
		return (0);
	x = siz;
	for (p = buf; x > 0; x--) {
		if (*p == '\n') {
			x = (int) (p - buf);
			x++;	/* 'sokay if it points just past the end! */
			    return (x);
		}
		p++;
	}
	    return (siz);
}

/*
 * comparehosts :
 * cross-check the host_info we have so far against new gethostby*() info,
 * and holler about mismatches.  Perhaps gratuitous, but it can't hurt to
 * point out when someone's DNS is fukt.  Returns 1 if mismatch, in case
 * someone else wants to do something about it. 
 */
int 
comparehosts(hinfo, hp)
	struct host_info   *hinfo;
	struct hostent *hp;
{
	if (strcasecmp(hinfo->name, hp->h_name) != 0) {
		nlog(0, "DNS fwd/rev mismatch: %s != %s", hinfo->name, hp->h_name);
		return (1);
	}
	return (0);
}

/* 
 * gethinfo:
 * resolve a host 8 ways from sunday; return a new host_info struct with its
 * info.  The argument can be a name or [ascii] IP address; it will try its
 * damndest to deal with it.  "numeric" governs whether we do any DNS at all,
 * and we also check o_verbose for what's appropriate work to do.
 */
struct host_info   *
gethinfo(name, numeric)
	char   *name;
	u_short  numeric;
{
	struct hostent *hostent;
	struct in_addr iaddr;
	struct host_info *hinfo = NULL;
	int x;

	if (name)
		hinfo = (struct host_info *) calloc(1, sizeof(struct host_info));

	if (!hinfo)
		nlog(1, "error obtaining host information");

	strlcpy(hinfo->name, "(UNKNOWN)", sizeof(hinfo->name));
	if (inet_aton(name, &iaddr) == 0) {
		if (numeric)
			nlog(1, "Can't parse %s as an IP address", name);

		/*
		 * failure to look up a name is fatal, 
		 * since we can't do anything with it.
		 */
		hostent = gethostbyname(name);
		if (!hostent)
			nlog(1, "%s: forward host lookup failed: ", name);

		strlcpy(hinfo->name, hostent->h_name, MAXHOSTNAMELEN);
		for (x = 0; hostent->h_addr_list[x] && (x < 8); x++) {
			memcpy(&hinfo->iaddrs[x], hostent->h_addr_list[x],
			    sizeof(struct in_addr));
			strlcpy(hinfo->addrs[x], inet_ntoa(hinfo->iaddrs[x]),
			    sizeof(hinfo->addrs[0]));
		}
		/* Go ahead and return if we don't want to view more */
		if (!o_verbose)	
			return (hinfo);

		/*
		 * Do inverse lookups in separate loop based on our collected 
	   	 * forward addrs, since gethostby* tends to crap into the same
		 * buffer over and over.
		 */
		for (x = 0; hinfo->iaddrs[x].s_addr && (x < 8); x++) {
			hostent = gethostbyaddr((char *) &hinfo->iaddrs[x],
			    sizeof(struct in_addr), AF_INET);
			if ((!hostent) || (!hostent->h_name))
				nlog(0, "Warning: inverse host lookup failed for %s: ",
				    hinfo->addrs[x]);
			else
				(void) comparehosts(hinfo, hostent);
		}

	} else {		/* not INADDR_NONE: numeric addresses... */
		memcpy(hinfo->iaddrs, &iaddr, sizeof(struct in_addr));
		strlcpy(hinfo->addrs[0], inet_ntoa(iaddr), sizeof(hinfo->addrs));
		/* If all that's wanted is numeric IP, go ahead and leave */
		if (numeric)	
			return (hinfo);

		/* Go ahead and return if we don't want to view more */
		if (!o_verbose)	
			return (hinfo);

		hostent = gethostbyaddr((char *) &iaddr, 
					sizeof(struct in_addr), AF_INET);

		/* 
		 * numeric or not, failure to look up a PTR is 
		 * *not* considered fatal 
		 */
		if (!hostent)
			nlog(0, "%s: inverse host lookup failed: ", name);
		else {
			strlcpy(hinfo->name, hostent->h_name, MAXHOSTNAMELEN);
			hostent = gethostbyname(hinfo->name);
			if ((!hostent) || (!hostent->h_addr_list[0]))
				nlog(0, "Warning: forward host lookup failed for %s: ",
				    hinfo->name);
			else
				(void) comparehosts(hinfo, hostent);
		}
	}

	/*
	 * Whatever-all went down previously, we should now have a host_info 
	 * struct with at least one IP address in it.
	 */
	return (hinfo);
}

/*
 * getpinfo:
 * Same general idea as gethinfo-- look up a port in /etc/services, fill
 * in global port_info, but return the actual port *number*.  Pass ONE of:
 *	pstring to resolve stuff like "23" or "exec";
 *	pnum to reverse-resolve something that's already a number.
 * If o_nflag is on, fill in what we can but skip the getservby??? stuff.
 * Might as well have consistent behavior here, and it *is* faster.
 */
u_short 
getpinfo(pstring, pnum)
	char   *pstring;
	unsigned int pnum;
{
	struct servent *servent;
	int x;
	int y;

	pinfo->name[0] = '?';/* fast preload */
	pinfo->name[1] = '\0';

	/*
	 * case 1: reverse-lookup of a number; placed first since this case
	 * is much more frequent if we're scanning.
	 */
	if (pnum) {
		/* Can't be both */
		if (pstring)
			return (0);

		x = pnum;
		if (o_nflag)	/* go faster, skip getservbyblah */
			goto gp_finish;
		y = htons(x);	/* gotta do this -- see Fig.1 below */
		servent = getservbyport(y, o_udpmode ? "udp" : "tcp");
		if (servent) {
			y = ntohs(servent->s_port);
			if (x != y)
				nlog(0, "Warning: port-bynum mismatch, %d != %d", x, y);
			strlcpy(pinfo->name, servent->s_name,
			    sizeof(pinfo->name));
		}
		goto gp_finish;
	}
	/*
	 * case 2: resolve a string, but we still give preference to numbers
	 * instead of trying to resolve conflicts.  None of the entries in *my*
	 * extensive /etc/services begins with a digit, so this should "always
	 * work" unless you're at 3com and have some company-internal services
	 * defined.
	 */
	if (pstring) {
		/* Can't be both */
		if (pnum)
			return (0);

		x = atoi(pstring);
		if (x)
			return (getpinfo(NULL, x));	/* recurse for
							 * numeric-string-arg */
		if (o_nflag)
			return (0);
		servent = getservbyname(pstring, o_udpmode ? "udp" : "tcp");
		if (servent) {
			strlcpy(pinfo->name, servent->s_name,
			    sizeof(pinfo->name));
			x = ntohs(servent->s_port);
			goto gp_finish;
		}		/* if servent */
	}			/* if pstring */
	return (0);		/* catches any problems so far */

gp_finish:
	/*
	 * Fall here whether or not we have a valid servent at this point, with
   	 * x containing our [host-order and therefore useful, dammit] port number.
	 */
	sprintf(pinfo->anum, "%d", x);	/* always load any numeric
						 * specs! */
	pinfo->num = (x & 0xffff);	/* u_short, remember... */
	return (pinfo->num);
}

/*
 * nextport :
 * Come up with the next port to try, be it random or whatever.  "block" is
 * a ptr to randports array, whose bytes [so far] carry these meanings:
 *	0	ignore
 * 	1	to be tested
 *	2	tested [which is set as we find them here]
 * returns a u_short random port, or 0 if all the t-b-t ones are used up. 
 */
u_short 
nextport(block)
	char   *block;
{
	unsigned int x;
	unsigned int y;

	y = 70000;			/* high safety count for rnd-tries */
	while (y > 0) {
		x = (arc4random() & 0xffff);
		if (block[x] == 1) {	/* try to find a not-done one... */
			block[x] = 2;
			break;
		}
		x = 0;		
		y--;
	}
	if (x)
		return (x);

	y = 65535;		/* no random one, try linear downsearch */
	while (y > 0) {		/* if they're all used, we *must* be sure! */
		if (block[y] == 1) {
			block[y] = 2;
			break;
		}
		y--;
	}
	if (y)
		return (y);	/* at least one left */

	return (0);
}

/*
 * loadports :
 * set "to be tested" indications in BLOCK, from LO to HI.  Almost too small
 * to be a separate routine, but makes main() a little cleaner.
 */
void 
loadports(block, lo, hi)
	char   *block;
	u_short  lo;
	u_short  hi;
{
	u_short  x;

	if (!block)
		nlog(1, "loadports: no block?!");
	if ((!lo) || (!hi))
		nlog(1, "loadports: bogus values %d, %d", lo, hi);
	x = hi;
	while (lo <= x) {
		block[x] = 1;
		x--;
	}
}


/*
 * doconnect :
 * do all the socket stuff, and return an fd for one of
 *	an open outbound TCP connection
 * 	a UDP stub-socket thingie
 * with appropriate socket options set up if we wanted source-routing, or
 *	an unconnected TCP or UDP socket to listen on.
 * Examines various global o_blah flags to figure out what-all to do. 
 */
int 
doconnect(rad, rp, lad, lp)
	struct in_addr     *rad;
	u_short  rp;
	struct in_addr     *lad;
	u_short  lp;
{
	int nnetfd = 0;
	int rr;
	int     x, y;

	/* grab a socket; set opts */
	while (nnetfd == 0) {
		if (o_udpmode)
			nnetfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		else
			nnetfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (nnetfd < 0)
			nlog(1, "Can't get socket");
	}

	x = 1;
	rr = setsockopt(nnetfd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));
	if (rr == -1)
		nlog(1, NULL);

	/* fill in all the right sockaddr crud */
	lclend->sin_family = AF_INET;
	remend->sin_family = AF_INET;

	/* if lad/lp, do appropriate binding */
	if (lad)
		memcpy(&lclend->sin_addr.s_addr, lad, sizeof(struct in_addr));
	if (lp)
		lclend->sin_port = htons(lp);

	rr = 0;
	if (lad || lp) {
		x = (int) lp;
		/* try a few times for the local bind, a la ftp-data-port... */
		for (y = 4; y > 0; y--) {
			rr = bind(nnetfd, (struct sockaddr *) lclend,
			    sizeof(struct sockaddr_in));
			if (rr == 0)
				break;
			if (errno != EADDRINUSE)
				break;
			else {
				nlog(0, "retrying local %s:%d", inet_ntoa(lclend->sin_addr), lp);
				sleep(2);
			}
		}
	}
	if (rr)
		nlog(1, "Can't grab %s:%d with bind",
		    inet_ntoa(lclend->sin_addr), lp);

	if (o_listen)
		return (nnetfd);/* thanks, that's all for today */

	memcpy(&remend->sin_addr.s_addr, rad, sizeof(struct in_addr));
	remend->sin_port = htons(rp);

	/* wrap connect inside a timer, and hit it */
	arm(1, o_wait);
	if (setjmp(jbuf) == 0) {
		rr = connect(nnetfd, (struct sockaddr *) remend, 
						sizeof(struct sockaddr));
	} else {
		rr = -1;
		errno = ETIMEDOUT;
	}
	arm(0, 0);
	if (rr == 0)
		return (nnetfd);
	close(nnetfd);
	return (-1);
}

/*
 * dolisten :
 * just like doconnect, and in fact calls a hunk of doconnect, but listens for
 * incoming and returns an open connection *from* someplace.  If we were
 * given host/port args, any connections from elsewhere are rejected.  This
 * in conjunction with local-address binding should limit things nicely.
 */
int 
dolisten(rad, rp, lad, lp)
	struct in_addr     *rad;
	u_short  rp;
	struct in_addr     *lad;
	u_short  lp;
{
	int nnetfd;
	int rr;
	struct host_info   *whozis = NULL;
	int     x;
	char   *cp;
	u_short  z;

	/*
	 * Pass everything off to doconnect, 
	 * who in o_listen mode just gets a socket 
	 */
	nnetfd = doconnect(rad, rp, lad, lp);
	if (nnetfd <= 0)
		return (-1);
	if (o_udpmode) {
		if (!lp)
			nlog(1, "UDP listen needs -p arg");
	} else {
		rr = listen(nnetfd, 1);
		if (rr < 0)	
			nlog(1, "error listening");
	}

	if (o_verbose) {
		x = sizeof(struct sockaddr);
		rr = getsockname(nnetfd, (struct sockaddr *) lclend, &x);
		if (rr < 0)
			nlog(0, "local getsockname failed");
		strcpy(bigbuf_net, "listening on [");	/* buffer reuse... */
		if (lclend->sin_addr.s_addr)
			strcat(bigbuf_net, inet_ntoa(lclend->sin_addr));
		else
			strcat(bigbuf_net, "any");
		strcat(bigbuf_net, "] ...");
		z = ntohs(lclend->sin_port);
		nlog(0, "%s %d", bigbuf_net, z);
	}			/* verbose -- whew!! */
	/*
	 * UDP is a speeeeecial case -- we have to do I/O *and* get the
	 * calling party's particulars all at once, listen() and accept()
	 * don't apply. At least in the BSD universe, however, recvfrom/PEEK
	 * is enough to tell us something came in, and we can set things up so
	 * straight read/write actually does work after all.  Yow.  YMMV on
	 * strange platforms!
	 */
	if (o_udpmode) {
		x = sizeof(struct sockaddr);	/* retval for recvfrom */
		arm(2, o_wait);	/* might as well timeout this, too */
		if (setjmp(jbuf) == 0) {	/* do timeout for initial
						 * connect */
			rr = recvfrom(nnetfd, bigbuf_net, BIGSIZ, MSG_PEEK,
					(struct sockaddr *)remend, &x);
		} else
			goto dol_tmo;
		arm(0, 0);
		rr = connect(nnetfd, (struct sockaddr *)remend, 
						sizeof(struct sockaddr));
		goto whoisit;
	}
	/* fall here for TCP */
	x = sizeof(struct sockaddr);
	arm(2, o_wait);	
	if (setjmp(jbuf) == 0) {
		rr = accept(nnetfd, (struct sockaddr *) remend, &x);
	} else
		goto dol_tmo;
	arm(0, 0);
	close(nnetfd);		/* dump the old socket */
	nnetfd = rr;		/* here's our new one */

whoisit:
	if (rr < 0)
		goto dol_err;	/* bail out if any errors so far */

	/*
	 * Find out what address the connection was *to* on 
	 * our end, in case we're doing a listen-on-any on
	 * a multihomed machine. This allows one to offer 
	 * different services via different alias addresses,
	 * such as the "virtual web site" hack.
	 */
	memset(bigbuf_net, 0, 64);
	cp = &bigbuf_net[32];
	x = sizeof(struct sockaddr);
	rr = getsockname(nnetfd, (struct sockaddr *) lclend, &x);
	if (rr < 0)
		nlog(0, "post-rcv getsockname failed");
	strcpy(cp, inet_ntoa(lclend->sin_addr));

	z = ntohs(remend->sin_port);
	strcpy(bigbuf_net, inet_ntoa(remend->sin_addr));
	whozis = gethinfo(bigbuf_net, o_nflag);
	x = 0;
	if (rad)		/* xxx: fix to go down the *list* if we have
				 * one? */
		if (memcmp(rad, whozis->iaddrs, sizeof(struct sockaddr)))
			x = 1;
	if (rp) {
		if (z != rp)
			x = 1;
	}
	if (x) {
		nlog(1, "invalid connection to [%s] from %s [%s] %d",
		    cp, whozis->name, whozis->addrs[0], z);
	}
	if (o_verbose) {
		nlog(0, "connect to [%s] from %s [%s] %d",	
	    		cp, whozis->name, whozis->addrs[0], z);
	}
	return (nnetfd);

dol_tmo:
	errno = ETIMEDOUT;
dol_err:
	close(nnetfd);
	return (-1);
}

/*
 * udptest :
 * fire a couple of packets at a UDP target port, just to see if it's really
 * there.  On BSD kernels, ICMP host/port-unreachable errors get delivered to
 * our socket as ECONNREFUSED write errors.  On SV kernels, we lose; we'll have
 * to collect and analyze raw ICMP ourselves a la satan's probe_udp_ports
 * backend.  Guess where one could swipe the appropriate code from...
 * 
 * Use the time delay between writes if given, otherwise use the "tcp ping"
 * trick for getting the RTT.  [I got that idea from pluvius, and warped it.]
 * Return either the original fd, or clean up and return -1. 
 */
int
udptest(fd, where)
	int     fd;
	struct in_addr     *where;
{
	int rr;

	rr = write(fd, bigbuf_in, 1);
	if (rr != 1)
		nlog(0, "udptest first write failed: ");
	if (o_wait)
		sleep(o_wait);
	else {
		o_udpmode = 0;
		o_wait = 5;
		rr = doconnect(where, SLEAZE_PORT, 0, 0);
		if (rr > 0)
			close(rr);
		o_wait = 0;
		o_udpmode++;
	}
	rr = write(fd, bigbuf_in, 1);
	if (rr == 1)
		return (fd);
	close(fd);
	return (-1);
}

/*
 * oprint :
 * Hexdump bytes shoveled either way to a running logfile, in the format:
 * D offset       -  - - - --- 16 bytes --- - - -  -     # .... ascii .....
 * where "which" sets the direction indicator, D:
 * 0 -- sent to network, or ">"
 * 1 -- rcvd and printed to stdout, or "<"
 * and "buf" and "n" are data-block and length.  If the current block generates
 * a partial line, so be it; we *want* that lockstep indication of who sent
 * what when.  Adapted from dgaudet's original example -- but must be ripping
 * *fast*, since we don't want to be too disk-bound.
 */
void 
oprint(which, buf, n)
	int     which;
	char   *buf;
	int     n;
{
	int     bc;		/* in buffer count */
	int     obc;		/* current "global" offset */
	int     soc;		/* stage write count */
	unsigned char *p;	/* main buf ptr; m.b. unsigned here */
	unsigned char *op;	/* out hexdump ptr */
	unsigned char *a;	/* out asc-dump ptr */
	int x;
	unsigned int y;

	if (!ofd)
		nlog(1, "oprint called with no open fd?!");
	if (n == 0)
		return;

	op = stage;
	if (which) {
		*op = '<';
		obc = wrote_out;/* use the globals! */
	} else {
		*op = '>';
		obc = wrote_net;
	}
	op++;			/* preload "direction" */
	*op = ' ';
	p = (unsigned char *) buf;
	bc = n;
	stage[59] = '#';	/* preload separator */
	stage[60] = ' ';

	while (bc) {		/* for chunk-o-data ... */
		x = 16;
		soc = 78;	/* len of whole formatted line */
		if (bc < x) {
			soc = soc - 16 + bc;	/* fiddle for however much is
						 * left */
			x = (bc * 3) + 11;	/* 2 digits + space per, after
						 * D & offset */
			op = &stage[x];
			x = 16 - bc;
			while (x) {
				*op++ = ' ';	/* preload filler spaces */
				*op++ = ' ';
				*op++ = ' ';
				x--;
			}
			x = bc;	/* re-fix current linecount */
		}		/* if bc < x */
		bc -= x;	/* fix wrt current line size */
		sprintf(&stage[2], "%8.8x ", obc);	/* xxx: still slow? */
		obc += x;	/* fix current offset */
		op = &stage[11];/* where hex starts */
		a = &stage[61];	/* where ascii starts */

		while (x) {	/* for line of dump, however long ... */
			y = (int) (*p >> 4);	/* hi half */
			*op = hexnibs[y];
			op++;
			y = (int) (*p & 0x0f);	/* lo half */
			*op = hexnibs[y];
			op++;
			*op = ' ';
			op++;
			if ((*p > 31) && (*p < 127))
				*a = *p;	/* printing */
			else
				*a = '.';	/* nonprinting, loose def */
			a++;
			p++;
			x--;
		}		/* while x */
		*a = '\n';	/* finish the line */
		x = write(ofd, stage, soc);
		if (x < 0)
			nlog(1, "ofd write err");
	}
}

#ifdef TELNET
u_short  o_tn = 0;		/* global -t option */

/*
 *  atelnet :
 * Answer anything that looks like telnet negotiation with don't/won't.
 * This doesn't modify any data buffers, update the global output count,
 * or show up in a hexdump -- it just shits into the outgoing stream.
 * Idea and codebase from Mudge@l0pht.com.
 */
void 
atelnet(buf, size)
	unsigned char *buf;	/* has to be unsigned here! */
	unsigned int size;
{
	static unsigned char obuf[4];	/* tiny thing to build responses into */
	int x;
	unsigned char y;
	unsigned char *p;

	y = 0;
	p = buf;
	x = size;
	while (x > 0) {
		if (*p != 255)	/* IAC? */
			goto notiac;
		obuf[0] = 255;
		p++;
		x--;
		if ((*p == 251) || (*p == 252))	/* WILL or WONT */
			y = 254;/* -> DONT */
		if ((*p == 253) || (*p == 254))	/* DO or DONT */
			y = 252;/* -> WONT */
		if (y) {
			obuf[1] = y;
			p++;
			x--;
			obuf[2] = *p;
			(void) write(netfd, obuf, 3);
			/* 
			 * if one wanted to bump wrote_net or do
			 * a hexdump line, here's the place.
			 */ 
			y = 0;
		}
notiac:
		p++;
		x--;
	}
}
#endif /* TELNET */

/*
 *  readwrite :
 * handle stdin/stdout/network I/O.  Bwahaha!! -- the select loop from hell.
 * In this instance, return what might become our exit status.
 */
int 
readwrite(fd)
	int     fd;
{
	int rr;
	char *zp;	/* stdin buf ptr */
	char *np;	/* net-in buf ptr */
	unsigned int rzleft;
	unsigned int rnleft;
	u_short  netretry;	/* net-read retry counter */
	u_short  wretry;	/* net-write sanity counter */
	u_short  wfirst;	/* one-shot flag to skip first net read */

	/*
	 * if you don't have all this FD_* macro hair in sys/types.h, 
	 * you'll have to either find it or do your own bit-bashing:
	 * *ds1 |= (1 << fd), etc.
	 */
	if (fd > FD_SETSIZE) {
		nlog(0, "Preposterous fd value %d", fd);
		return (1);
	}
	FD_SET(fd, &fds1);
	netretry = 2;
	wfirst = 0;
	rzleft = rnleft = 0;
	if (insaved) {
		rzleft = insaved;	/* preload multi-mode fakeouts */
		zp = bigbuf_in;
		wfirst = 1;
		/* If not scanning, this is a one-off first */
		if (Single)
			insaved = 0;
		else {
			FD_CLR(0, &fds1);
			close(0);
		}
	}
	if (o_interval)
		sleep(o_interval);

	while (FD_ISSET(fd, &fds1)) {	/* i.e. till the *net* closes! */
		struct timeval *tv;

		wretry = 8200;		/* more than we'll ever hafta write */
		if (wfirst) {		/* any saved stdin buffer? */
			wfirst = 0;	/* clear flag for the duration */
			goto shovel;	/* and go handle it first */
		}
		fds2 = fds1;
		if (timer1.tv_sec > 0 || timer1.tv_usec > 0) {
			memcpy(&timer2, &timer1, sizeof(struct timeval));
			tv = &timer2;
		} else
			tv = NULL;
		rr = select(getdtablesize(), &fds2, 0, 0, tv);
		if (rr < 0) {
			if (errno != EINTR) {
				nlog(0, "Select Failure");
				close(fd);
				return (1);
			}
		}
		/* if we have a timeout AND stdin is closed AND we haven't
		 * heard anything from the net during that time, assume it's
		 * dead and close it too. */
		if (rr == 0) {
			if (!FD_ISSET(0, &fds1))
				netretry--;	/* we actually try a coupla
						 * times. */
			if (!netretry) {
				if (o_verbose > 1)	/* normally we don't
							 * care */
					nlog(0, "net timeout");
				close(fd);
				return (0);	/* not an error! */
			}
		}		/* select timeout */
		/* XXX: should we check the exception fds too?  The read fds
		 * seem to give us the right info, and none of the examples I
		 * found bothered. */
		/* Ding!!  Something arrived, go check all the incoming
		 * hoppers, net first */
		if (FD_ISSET(fd, &fds2)) {	/* net: ding! */
			rr = read(fd, bigbuf_net, BIGSIZ);
			if (rr <= 0) {
				FD_CLR(fd, &fds1);	/* net closed, we'll
							 * finish up... */
				rzleft = 0;	/* can't write anymore: broken
						 * pipe */
			} else {
				rnleft = rr;
				np = bigbuf_net;
#ifdef TELNET
				if (o_tn)
					atelnet(np, rr);	/* fake out telnet stuff */
#endif /* TELNET */
			}
		}
		/* if we're in "slowly" mode there's probably still stuff in
		 * the stdin buffer, so don't read unless we really need MORE
		 * INPUT!  MORE INPUT! */
		if (rzleft)
			goto shovel;

		if (FD_ISSET(0, &fds2)) {
			rr = read(0, bigbuf_in, BIGSIZ);
			if (rr <= 0) {
				FD_CLR(0, &fds1);	/* disable and close */
				close(0);
			} else {
				rzleft = rr;
				zp = bigbuf_in;
				if (!Single) {
					insaved = rr;
					FD_CLR(0, &fds1);
					close(0);
				}
			}
		}
shovel:
		/* sanity check.  Works because they're both unsigned... */
		if ((rzleft > 8200) || (rnleft > 8200)) {
			rzleft = rnleft = 0;
		}
		/* net write retries sometimes happen on UDP connections */
		if (!wretry) {	/* is something hung? */
			nlog(0, "too many output retries");
			return (1);
		}
		if (rnleft) {
			rr = write(1, np, rnleft);
			if (rr > 0) {
				if (o_wfile)
					oprint(1, np, rr);
				np += rr;
				rnleft -= rr;
				wrote_out += rr;	/* global count */
			}
		}
		if (rzleft) {
			if (o_interval)
				rr = findline(zp, rzleft);
			else
				rr = rzleft;
			rr = write(fd, zp, rr);	
			if (rr > 0) {
				if (o_wfile)
					oprint(0, zp, rr);
				zp += rr;
				rzleft -= rr;
				wrote_net += rr;
			}
		}
		if (o_interval) {
			sleep(o_interval);
			continue;
		}
		if ((rzleft) || (rnleft)) {	
			wretry--;
			goto shovel;
		}
	}

	close(fd);
	return (0);
}

/* main :
   now we pull it all together... */
int
main(argc, argv)
	int     argc;
	char  **argv;
{
	int x, ch;
	char *cp;
	struct host_info   *gp;
	struct host_info   *whereto = NULL;
	struct host_info   *wherefrom = NULL;
	struct in_addr     *ouraddr = NULL;
	struct in_addr     *themaddr = NULL;
	u_short  o_lport = 0;
	u_short  ourport = 0;
	u_short  loport = 0;	/* for scanning stuff */
	u_short  hiport = 0;
	u_short  curport = 0;
	char   *randports = NULL;

	res_init();
	
	lclend = (struct sockaddr_in *) calloc(1, sizeof(struct sockaddr));
	remend = (struct sockaddr_in *) calloc(1, sizeof(struct sockaddr));
	bigbuf_in = calloc(1, BIGSIZ);
	bigbuf_net = calloc(1, BIGSIZ);
	pinfo= (struct port_info *) calloc(1, sizeof(struct port_info));

	gatesptr = 4;

	/*
	 * We want to catch a few of these signals. 
	 * Others we disgard.
	 */
	signal(SIGINT, catch);
	signal(SIGQUIT, catch);
	signal(SIGTERM, catch);
	signal(SIGURG, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);	/* important! */

	/* 
	 * If no args given at all, get 'em from stdin, construct an argv, 
	 * and hand anything left over to readwrite().
	 */
	if (argc == 1) {
		/* Loop until we get a command to try */
		for (;;) {
			cp = argv[0];
			argv = (char **) calloc(1, 128 * sizeof(char *));
			argv[0] = cp;	/* leave old prog name intact */
			cp = calloc(1, BIGSIZ);
			argv[1] = cp;	/* head of new arg block */
			fprintf(stderr, "Cmd line: ");
			fflush(stderr);	/* I dont care if it's unbuffered or not! */
			insaved = read(0, cp, BIGSIZ-1); /* we're gonna fake fgets()
				 			  * here */
			cp[BIGSIZ-1] = '\0';
			if (*cp != '\n' && *cp != '\t')
				break;
		}
		if (insaved <= 0)
			nlog(1, "wrong");
		x = findline(cp, insaved);
		if (x)
			insaved -= x;	/* remaining chunk size to be sent */
		if (insaved)	/* which might be zero... */
			memcpy(bigbuf_in, &cp[x], insaved);
		cp = strchr(argv[1], '\n');
		if (cp)
			*cp = '\0';
		cp = strchr(argv[1], '\r');	/* look for ^M too */
		if (cp)
			*cp = '\0';

		/*
		 * Find and stash pointers to remaining new "args"
		 */
		cp = argv[1];
		cp++;		/* skip past first char */
		x = 2;		/* we know argv 0 and 1 already */
		for (; *cp != '\0'; cp++) {
			if (*cp == ' ') {
				*cp = '\0';	/* smash all spaces */
				continue;
			} else {
				if (*(cp - 1) == '\0') {
					argv[x] = cp;
					x++;
				}
			}	/* if space */
		}		/* for cp */
		argc = x;
	}

	while ((ch = getopt(argc, argv, "g:G:hi:lno:p:rs:tuvw:z")) != -1) {
		switch (ch) {
		case 'G':			/* srcrt gateways pointer val */
			x = atoi(optarg);
			/* Mask of bits */
			if ((x) && (x == (x & 0x1c)))
				gatesptr = x;
			else
				nlog(1, "invalid hop pointer %d, must be multiple of 4 <= 28", x);
			break;
		case 'g':			/* srcroute hop[s] */
			if (gatesidx > 8)
				nlog(1, "Too many -g hops!");
			if (gates == NULL)
				gates = (struct host_info **) calloc(1, 
						sizeof(struct host_info *) * 10);
			gp = gethinfo(optarg, o_nflag);
			if (gp)
				gates[gatesidx] = gp;
			gatesidx++;
			break;
		case 'h':
			help();
			break;
		case 'i':			/* line-interval time */
			o_interval = atoi(optarg) & 0xffff;
			if (!o_interval)
				nlog(1, "invalid interval time %s", optarg);
			break;
		case 'l':			/* listen mode */
			o_listen++;
			break;
		case 'n':			/* numeric-only, no DNS lookups */
			o_nflag++;
			break;
		case 'o':			/* hexdump log */
			stage = (unsigned char *) optarg;
			o_wfile++;
			break;
		case 'p':			/* local source port */
			o_lport = getpinfo(optarg, 0);
			if (o_lport == 0)
				nlog(1, "invalid local port %s", optarg);
			break;
		case 'r':			/* randomize various things */
			o_random++;
			break;
		/* 
		 * Do a full lookup [since everything else goes through the same 
		 * mill], unless -n was previously specified. In fact, careful
		 * placement of -n can be useful, so we'll still pass o_nflag
		 * here instead of forcing numeric.
		 */
		case 's':			/* local source address */
			wherefrom = gethinfo(optarg, o_nflag);
			ouraddr = &wherefrom->iaddrs[0];
			break;
#ifdef TELNET
		case 't':			/* do telnet fakeout */
			o_tn++;
			break;
#endif
		case 'u':			/* use UDP */
			o_udpmode++;
			break;
		case 'v':			/* verbose */
			o_verbose++;
			break;
		case 'w':			/* wait time */
			o_wait = atoi(optarg);
			if (o_wait <= 0)
				nlog(1, "invalid wait-time %s", optarg);
			timer1.tv_sec = o_wait;
			timer1.tv_usec = 0;
			break;
		case 'z':			/* little or no data xfer */
			o_zero++;
			break;
		default:
			usage(1);
		}
	}

	/* other misc initialization */
	FD_SET(0, &fds1);	/* stdin *is* initially open */
	if (o_random) {
		randports = calloc(1, 65536);   /* big flag array for ports */
	}
	if (o_wfile) {
		ofd = open(stage, O_WRONLY | O_CREAT | O_TRUNC, 0664);
		if (ofd <= 0)	/* must be > extant 0/1/2 */
			nlog(1, "%s: ", stage);
		stage = (unsigned char *) calloc(1, 100);
	}
	/* optind is now index of first non -x arg */
	if (argv[optind])
		whereto = gethinfo(argv[optind], o_nflag);
	if (whereto && whereto->iaddrs)
		themaddr = &whereto->iaddrs[0];
	if (themaddr)
		optind++;	/* skip past valid host lookup */

	/*
	 * Handle listen mode here, and exit afterward.  Only does one connect;
	 * this is arguably the right thing to do.  A "persistent listen-and-fork"
	 * mode a la inetd has been thought about, but not implemented.  A tiny
 	 * wrapper script can handle such things.
	 */
	if (o_listen) {
		curport = 0;
		if (argv[optind]) {
			curport = getpinfo(argv[optind], 0);
			if (curport == 0)
				nlog(1, "invalid port %s", argv[optind]);
		}
		netfd = dolisten(themaddr, curport, ouraddr, o_lport);
		if (netfd > 0) {
			x = readwrite(netfd);
			if (o_verbose)
				nlog(0, "Sent %i Rcvd %i", wrote_net, wrote_out);
			exit(x);
		} else	
			nlog(1, "no connection");
	}
	/* fall thru to outbound connects.  Now we're more picky about args... */
	if (!themaddr)
		nlog(1, "no destination");
	if (argv[optind] == NULL)
		nlog(1, "no port[s] to connect to");
	if (argv[optind + 1])
		Single = 0;
	ourport = o_lport;

	while (argv[optind]) {
		hiport = loport = 0;
		if (cp = strchr(argv[optind], '-')) {
			*cp = '\0';
			cp++;
			hiport = getpinfo(cp, 0);
			if (hiport == 0)
				nlog(1, "invalid port %s", cp);
		}
		loport = getpinfo(argv[optind], 0);
		if (loport == 0)
			nlog(1, "invalid port %s", argv[optind]);
		if (hiport > loport) {
			Single = 0;
			if (o_random) {	
				loadports(randports, loport, hiport);
				curport = nextport(randports);
			} else
				curport = hiport;
		} else
			curport = loport;
	  	    /*
		     * Now start connecting to these things.  
		     * curport is already preloaded.
		     */
		    while (loport <= curport) {
			curport = getpinfo(NULL, curport);
			netfd = doconnect(themaddr, curport, ouraddr, ourport);
			if (netfd > 0)
				if (o_zero && o_udpmode)
					netfd = udptest(netfd, themaddr);
			if (netfd > 0) {
				x = errno = 0;
				if (o_verbose) {
					nlog(0, "%s [%s] %d (%s) open",
				    		whereto->name, 
						whereto->addrs[0], curport, 
						pinfo->name);
				}
				if (!o_zero)
					x = readwrite(netfd);
			} else {
				x = 1;
				if ((Single || (o_verbose > 1)) 
				   || (errno != ECONNREFUSED)) {
					nlog(0, "%s [%s] %d (%s)",
					     whereto->name, whereto->addrs[0], 
					     curport, pinfo->name);
				}
			}
			close(netfd);
			if (o_interval)
				sleep(o_interval);
			if (o_random)
				curport = nextport(randports);
			else
				curport--;
		    }
		optind++;
	}
	errno = 0;
	if (o_verbose > 1)
		nlog(0, "Sent %i Rcvd %i", wrote_net, wrote_out);
	if (Single)
		exit(x);
	exit(0);
}

/*
 * nlog:
 * dual purpose function, does both warn() and err()
 * and pays attention to o_verbose.
 */
void
nlog(doexit, fmt)
	int doexit;
	char *fmt;
{
	va_list args;

	if (o_verbose || doexit) {
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		if (h_errno)
                        herror(NULL);
		else if (errno)
			fprintf(stderr, "%s\n", strerror(errno));
		else
			putc('\n', stderr);
		va_end(args);
	}
 
	if (doexit)
		exit(1);

	h_errno = errno = 0;
}

void
usage(doexit)
	int doexit;
{
	fprintf(stderr, "netcat - [v1.10]\n");
	fprintf(stderr, "nc [-lnrtuvz] [-e command] [-g intermediates]\n");
	fprintf(stderr, "   [-G hopcount] [-i interval] [-o filename] [-p source port]\n");
	fprintf(stderr, "   [-s ip address] [-w timeout] [hostname] [port[s...]]\n");
	if (doexit)
		exit(1);
}

void
help()
{
	usage(0);
	fprintf(stderr, "\tCommand Summary:\n\
	 \t-g gateway	source-routing hop point[s], up to 8\n\
  	 \t-G num\t	source-routing pointer: 4, 8, 12, ...\n\
	 \t-h		this help text\n\
	 \t-i secs\t	delay interval for lines sent, ports scanned\n\
	 \t-l		listen mode, for inbound connects\n\
	 \t-n		numeric-only IP addresses, no DNS\n\
	 \t-o file\t	hex dump of traffic\n\
	 \t-r		randomize local and remote ports\n\
	 \t-s addr\t	local source address\n");
#ifdef TELNET
	 fprintf(stderr, "\t\t-t		answer TELNET negotiation\n");
#endif
	 fprintf(stderr, "\t\t-u		UDP mode\n\
	 \t-v		verbose [use twice to be more verbose]\n\
	 \t-w secs\t	timeout for connects and final net reads\n\
	 \t-z		zero-I/O mode [used for scanning]\n\
	 Port numbers can be individual or ranges: lo-hi [inclusive]\n");
	exit(1);
}
