/*	$OpenBSD: timed.c,v 1.29 2009/10/27 23:59:57 deraadt Exp $	*/

/*-
 * Copyright (c) 1985, 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define TSPTYPES
#include "globals.h"
#include <net/if.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <setjmp.h>
#include "pathnames.h"
#include <math.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/times.h>
#include <netgroup.h>
#include <err.h>
#include <ifaddrs.h>

int trace = 0;
int sock, sock_raw = -1;
int status = 0;
u_short sequence;			/* sequence number */
long delay2;

int nslavenets;				/* nets were I could be a slave */
int nmasternets;			/* nets were I could be a master */
int nignorednets;			/* ignored nets */
int nnets;				/* nets I am connected to */

FILE *fd;				/* trace file FD */

jmp_buf jmpenv;

volatile sig_atomic_t gotintr;

struct netinfo *nettab = 0;
struct netinfo *slavenet;
int Mflag;
int justquit = 0;
int debug;

struct nets {
	char name[1024];
	in_addr_t net;
	TAILQ_ENTRY(nets) next;
};
static TAILQ_HEAD(, nets) nets;

struct hosttbl hosttbl[NHOSTS+1];	/* known hosts */

/* List of hosts we trust */
struct goodhost {
	char	name[MAXHOSTNAMELEN];
	int 	perm;
	TAILQ_ENTRY(goodhost) next;
};
static TAILQ_HEAD(, goodhost) goodhosts;

static char *goodgroup;			/* net group of trusted hosts */

/* prototypes */
static void addnetname(const char *);
static void checkignorednets(void);
static void pickslavenet(struct netinfo *);
static void add_good_host(const char *, int);
static void usage(void);

/*
 * The timedaemons synchronize the clocks of hosts in a local area network.
 * One daemon runs as master, all the others as slaves. The master
 * performs the task of computing clock differences and sends correction
 * values to the slaves.
 * Slaves start an election to choose a new master when the latter disappears
 * because of a machine crash, network partition, or when killed.
 * A resolution protocol is used to kill all but one of the masters
 * that happen to exist in segments of a partitioned network when the
 * network partition is fixed.
 *
 * Authors: Riccardo Gusella & Stefano Zatti
 *
 * overhauled at Silicon Graphics
 */
int
main(int argc, char **argv)
{
	int on;
	int ret;
	int nflag, iflag;
	struct timeval ntime;
	struct servent *srvp;
	struct netinfo *ntp;
	struct netinfo *ntip;
	struct netinfo *savefromnet;
	struct nets *nt;
	struct sockaddr_in server;
	u_short port;
	int ch;
	struct ifaddrs *ifap, *ifa;

	ntip = NULL;

	on = 1;
	nflag = 0;
	iflag = 0;

	TAILQ_INIT(&nets);
	TAILQ_INIT(&goodhosts);

	opterr = 0;
	while ((ch = getopt(argc, argv, "F:G:Mdi:n:t")) != -1) {
		switch (ch) {
		case 'F':
			add_good_host(optarg, 1);
			while (optind < argc && argv[optind][0] != '-')
				add_good_host(argv[optind++], 1);
			break;
		case 'G':
			if (goodgroup != NULL) {
				fprintf(stderr,"timed: only one net group\n");
				exit(1);
			}
			goodgroup = optarg;
			break;
		case 'M':
			Mflag = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'i':
			iflag = 1;
			addnetname(optarg);
			break;
		case 'n':
			nflag = 1;
			addnetname(optarg);
			break;
		case 't':
			trace = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (optind < argc)
		usage();

	if (nflag && iflag) {
		fprintf(stderr, "timed: -i and -n make no sense together\n");
		exit(1);
	}

	/*
	 * If we care about which machine is the master, then we must be
	 * willing to be a master as well.
	 */
	if ((goodgroup != NULL) || !TAILQ_EMPTY(&goodhosts))
		Mflag = 1;

	if (gethostname(hostname, sizeof(hostname)) < 0) {
		perror("gethostname");
		exit(1);
	}
	self.l_bak = &self;
	self.l_fwd = &self;
	self.h_bak = &self;
	self.h_fwd = &self;
	self.head = 1;
	self.good = 1;

	/* Add ourselves to the list of trusted hosts */
	if (!TAILQ_EMPTY(&goodhosts))
		add_good_host(hostname, 1);

	if ((srvp = getservbyname("timed", "udp")) == NULL) {
		fprintf(stderr, "unknown service 'timed/udp'\n");
		exit(1);
	}
	port = srvp->s_port;
	bzero(&server, sizeof(server));
	server.sin_port = srvp->s_port;
	server.sin_family = AF_INET;
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&on,
							sizeof(on)) < 0) {
		perror("setsockopt");
		exit(1);
	}
	if (bind(sock, (struct sockaddr*)&server, sizeof(server))) {
		if (errno == EADDRINUSE)
			fprintf(stderr,"timed: time daemon already running\n");
		else
			perror("bind");
		exit(1);
	}

	sequence = arc4random();     /* initial seq number */

	gettimeofday(&ntime, 0);
	/* rounds kernel variable time to multiple of 5 ms. */
	ntime.tv_sec = 0;
	ntime.tv_usec = -((ntime.tv_usec/1000) % 5) * 1000;
	(void)adjtime(&ntime, (struct timeval *)0);

	TAILQ_FOREACH(nt, &nets, next) {
		struct netent *nentp;

		nentp = getnetbyname(nt->name);
		if (nentp == 0) {
			nt->net = inet_network(nt->name);
			if (nt->net != INADDR_NONE)
				nentp = getnetbyaddr(nt->net, AF_INET);
		}
		if (nentp != 0) {
			nt->net = nentp->n_net;
		} else if (nt->net == INADDR_NONE) {
			fprintf(stderr, "timed: unknown net %s\n", nt->name);
			exit(1);
		} else if (nt->net == INADDR_ANY) {
			fprintf(stderr, "timed: bad net %s\n", nt->name);
			exit(1);
		} else {
			fprintf(stderr,
				"timed: warning: %s unknown in /etc/networks\n",
				nt->name);
		}

		if (0 == (nt->net & 0xff000000))
		    nt->net <<= 8;
		if (0 == (nt->net & 0xff000000))
		    nt->net <<= 8;
		if (0 == (nt->net & 0xff000000))
		    nt->net <<= 8;
	}

	if (getifaddrs(&ifap) != 0) {
		perror("timed: get interface configuration");
		exit(1);
	}

	ntp = NULL;
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		if (!ntp)
			ntp = (struct netinfo*)malloc(sizeof(struct netinfo));
		bzero(ntp, sizeof(*ntp));
		ntp->my_addr=((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
		ntp->status = NOMASTER;

		if ((ifa->ifa_flags & IFF_UP) == 0)
			continue;
		if ((ifa->ifa_flags & IFF_BROADCAST) == 0 &&
		    (ifa->ifa_flags & IFF_POINTOPOINT) == 0) {
			continue;
		}

		((struct sockaddr_in *)ifa->ifa_addr)->sin_addr = ntp->my_addr;
		ntp->mask = ((struct sockaddr_in *)
			ifa->ifa_netmask)->sin_addr.s_addr;

		if (ifa->ifa_flags & IFF_BROADCAST) {
			ntp->dest_addr = *(struct sockaddr_in *)ifa->ifa_broadaddr;
			/* What if the broadcast address is all ones?
			 * So we cannot just mask ntp->dest_addr.  */
			ntp->net = ntp->my_addr;
			ntp->net.s_addr &= ntp->mask;
		} else {
			ntp->dest_addr = *(struct sockaddr_in *)ifa->ifa_dstaddr;
			ntp->net = ntp->dest_addr.sin_addr;
		}

		ntp->dest_addr.sin_port = port;

		TAILQ_FOREACH(nt, &nets, next) {
			if (ntohl(ntp->net.s_addr) == nt->net)
				break;
		}
		if ((nflag && !nt) || (iflag && nt))
			continue;

		ntp->next = NULL;
		if (nettab == NULL) {
			nettab = ntp;
		} else {
			ntip->next = ntp;
		}
		ntip = ntp;
		ntp = NULL;
	}

	if (ntp)
		(void) free((char *)ntp);
	if (nettab == NULL) {
		fprintf(stderr, "timed: no network usable\n");
		exit(1);
	}
	freeifaddrs(ifap);

	/* election timer delay in secs. */
	delay2 = casual(MINTOUT, MAXTOUT);

	if (!debug)
		daemon(debug, 0);

	if (trace)
		traceon();
	openlog("timed", LOG_CONS|LOG_PID, LOG_DAEMON);

	/*
	 * keep returning here
	 */
	ret = setjmp(jmpenv);
	savefromnet = fromnet;
	setstatus();

	if (Mflag) {
		switch (ret) {

		case 0:
			checkignorednets();
			pickslavenet(0);
			break;
		case 1:
			/* Just lost our master */
			if (slavenet != 0)
				slavenet->status = election(slavenet);
			if (!slavenet || slavenet->status == MASTER) {
				checkignorednets();
				pickslavenet(0);
			} else {
				makeslave(slavenet);	/* prune extras */
			}
			break;

		case 2:
			/* Just been told to quit */
			justquit = 1;
			pickslavenet(savefromnet);
			break;
		}

		setstatus();
		if (!(status & MASTER) && sock_raw != -1) {
			/* sock_raw is not being used now */
			(void)close(sock_raw);
			sock_raw = -1;
		}

		if (status == MASTER)
			master();
		else
			slave();

	} else {
		if (sock_raw != -1) {
			(void)close(sock_raw);
			sock_raw = -1;
		}

		if (ret) {
			/* we just lost our master or were told to quit */
			justquit = 1;
		}
		for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
			if (ntp->status == MASTER) {
				rmnetmachs(ntp);
				ntp->status = NOMASTER;
			}
		}
		checkignorednets();
		pickslavenet(0);
		setstatus();

		slave();
	}
	/* NOTREACHED */
	return(0);
}


/*
 * suppress an upstart, untrustworthy, self-appointed master
 */
void
suppress(struct sockaddr_in *addr, const char *name, struct netinfo *net)
{
	struct sockaddr_in tgt;
	char tname[MAXHOSTNAMELEN];
	struct tsp msg;
	static struct timeval wait;

	if (trace)
		fprintf(fd, "suppress: %s\n", name);
	tgt = *addr;
	strlcpy(tname, name, sizeof(tname));

	while (readmsg(TSP_ANY, ANYADDR, &wait, net) != NULL) {
		if (trace)
			fprintf(fd, "suppress:\tdiscarded packet from %s\n",
				    name);
	}

	syslog(LOG_NOTICE, "suppressing false master %s", tname);

	memset(&msg, 0, sizeof(msg));
	msg.tsp_type = TSP_QUIT;
	strlcpy(msg.tsp_name, hostname, sizeof msg.tsp_name);

	(void)acksend(&msg, &tgt, tname, TSP_ACK, 0, 1);
}

void
lookformaster(struct netinfo *ntp)
{
	struct tsp resp, conflict, *answer;
	struct timeval ntime;
	char mastername[MAXHOSTNAMELEN];
	struct sockaddr_in masteraddr;

	get_goodgroup(0);
	ntp->status = SLAVE;

	/* look for master */
	memset(&resp, 0, sizeof(resp));
	resp.tsp_type = TSP_MASTERREQ;
	strlcpy(resp.tsp_name, hostname, sizeof(resp.tsp_name));

	answer = acksend(&resp, &ntp->dest_addr, ANYADDR,
			 TSP_MASTERACK, ntp, 0);
	if ((answer != NULL) && !good_host_name(answer->tsp_name)) {
		suppress(&from, answer->tsp_name, ntp);
		ntp->status = NOMASTER;
		answer = 0;
	}

	if (answer == NULL) {
		/*
		 * Various conditions can cause conflict: races between
		 * two just started timedaemons when no master is
		 * present, or timedaemons started during an election.
		 * A conservative approach is taken.  Give up and became a
		 * slave, postponing election of a master until first
		 * timer expires.
		 */
		ntime.tv_sec = ntime.tv_usec = 0;
		answer = readmsg(TSP_MASTERREQ, ANYADDR, &ntime, ntp);
		if (answer != NULL) {
			if (!good_host_name(answer->tsp_name)) {
				suppress(&from, answer->tsp_name, ntp);
				ntp->status = NOMASTER;
			}
			return;
		}

		ntime.tv_sec = ntime.tv_usec = 0;
		answer = readmsg(TSP_MASTERUP, ANYADDR, &ntime, ntp);
		if (answer != NULL) {
			if (!good_host_name(answer->tsp_name)) {
				suppress(&from, answer->tsp_name, ntp);
				ntp->status = NOMASTER;
			}
			return;
		}

		ntime.tv_sec = ntime.tv_usec = 0;
		answer = readmsg(TSP_ELECTION, ANYADDR, &ntime, ntp);
		if (answer != NULL) {
			if (!good_host_name(answer->tsp_name)) {
				suppress(&from, answer->tsp_name, ntp);
				ntp->status = NOMASTER;
			}
			return;
		}

		if (Mflag)
			ntp->status = MASTER;
		else
			ntp->status = NOMASTER;
		return;
	}

	ntp->status = SLAVE;
	strlcpy(mastername, answer->tsp_name, sizeof mastername);
	masteraddr = from;

	/*
	 * If network has been partitioned, there might be other
	 * masters; tell the one we have just acknowledged that
	 * it has to gain control over the others.
	 */
	ntime.tv_sec = 0;
	ntime.tv_usec = 300000;
	answer = readmsg(TSP_MASTERACK, ANYADDR, &ntime, ntp);

	/*
	 * checking also not to send CONFLICT to ack'ed master
	 * due to duplicated MASTERACKs
	 */
	if (answer != NULL &&
	    strcmp(answer->tsp_name, mastername) != 0) {
		conflict.tsp_type = TSP_CONFLICT;
		strlcpy(conflict.tsp_name, hostname, sizeof conflict.tsp_name);
		if (!acksend(&conflict, &masteraddr, mastername,
			     TSP_ACK, 0, 0)) {
			syslog(LOG_ERR,
			       "error on sending TSP_CONFLICT");
		}
	}
}

/*
 * based on the current network configuration, set the status, and count
 * networks;
 */
void
setstatus(void)
{
	struct netinfo *ntp;

	status = 0;
	nmasternets = nslavenets = nnets = nignorednets = 0;
	if (trace)
		fprintf(fd, "Net status:\n");
	for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		switch ((int)ntp->status) {
		case MASTER:
			nmasternets++;
			break;
		case SLAVE:
			nslavenets++;
			break;
		case NOMASTER:
		case IGNORE:
			nignorednets++;
			break;
		}
		if (trace) {
			fprintf(fd, "\t%-16s", inet_ntoa(ntp->net));
			switch ((int)ntp->status) {
			case NOMASTER:
				fprintf(fd, "NOMASTER\n");
				break;
			case MASTER:
				fprintf(fd, "MASTER\n");
				break;
			case SLAVE:
				fprintf(fd, "SLAVE\n");
				break;
			case IGNORE:
				fprintf(fd, "IGNORE\n");
				break;
			default:
				fprintf(fd, "invalid state %d\n",
					(int)ntp->status);
				break;
			}
		}
		nnets++;
		status |= ntp->status;
	}
	status &= ~IGNORE;
	if (trace)
		fprintf(fd,
		    "\tnets=%d masters=%d slaves=%d ignored=%d delay2=%ld\n",
		    nnets, nmasternets, nslavenets, nignorednets, delay2);
}

void
makeslave(struct netinfo *net)
{
	struct netinfo *ntp;

	for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		if (ntp->status == SLAVE && ntp != net)
			ntp->status = IGNORE;
	}
	slavenet = net;
}

/*
 * Try to become master over ignored nets..
 */
static void
checkignorednets(void)
{
	struct netinfo *ntp;

	for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		if (!Mflag && ntp->status == SLAVE)
			break;

		if (ntp->status == IGNORE || ntp->status == NOMASTER) {
			lookformaster(ntp);
			if (!Mflag && ntp->status == SLAVE)
				break;
		}
	}
}

/*
 * choose a good network on which to be a slave
 *	The ignored networks must have already been checked.
 *	Take a hint about for a good network.
 */
static void
pickslavenet(struct netinfo *ntp)
{
	if (slavenet != 0 && slavenet->status == SLAVE) {
		makeslave(slavenet);		/* prune extras */
		return;
	}

	if (ntp == 0 || ntp->status != SLAVE) {
		for (ntp = nettab; ntp != 0; ntp = ntp->next) {
			if (ntp->status == SLAVE)
				break;
		}
	}
	makeslave(ntp);
}

/*
 * returns a random number in the range [inf, sup]
 */
long
casual(long inf, long sup)
{
	return (inf + random() % (sup - inf + 1));
}

char *
date(void)
{
	struct	timeval tv;
	time_t t;

	(void)gettimeofday(&tv, (struct timezone *)0);
	t = tv.tv_sec;
	return (ctime(&t));
}

void
addnetname(const char *name)
{
	struct nets *netlist;

	if ((netlist = (struct nets *)calloc(1, sizeof(*netlist))) == NULL)
		err(1, "malloc");
	strlcpy(netlist->name, name, sizeof(netlist->name));
	TAILQ_INSERT_TAIL(&nets, netlist, next);
}

/*
 * add_good_host() -
 *
 * Add a host to our list of trusted hosts.
 */
static void
add_good_host(const char *name, int perm)
{
	struct goodhost *ghp;
	struct hostent *hentp;

	if ((ghp = (struct goodhost *)calloc(1, sizeof(*ghp))) == NULL)
		err(1, "malloc");
	strlcpy(ghp->name, name, sizeof(ghp->name));
	ghp->perm = perm;
	TAILQ_INSERT_TAIL(&goodhosts, ghp, next);

	if ((hentp = gethostbyname(name)) == NULL && perm)
		(void)fprintf(stderr, "unknown host %s\n", name);
}


/* update our image of the net-group of trustworthy hosts
 */
void
get_goodgroup(int force)
{
# define NG_DELAY (30*60*CLK_TCK)	/* 30 minutes */
	static unsigned long last_update = -NG_DELAY;
	unsigned long new_update;
	struct hosttbl *htp;
	struct goodhost *ghp, *nxt;
	const char *mach, *usr, *dom;
	struct tms tm;


	/* if no netgroup, then we are finished */
	if (goodgroup == 0 || !Mflag)
		return;

	/* Do not chatter with the netgroup master too often.
	 */
	new_update = times(&tm);
	if (new_update < last_update + NG_DELAY
	    && !force)
		return;
	last_update = new_update;

	/* forget the old temporary entries */
	for (ghp = TAILQ_FIRST(&goodhosts); ghp != NULL; ghp = nxt) {
		nxt = TAILQ_NEXT(ghp, next);

		if (!ghp->perm) {
			TAILQ_REMOVE(&goodhosts, ghp, next);
			free(ghp);
		}
	}

	/* quit now if we are not one of the trusted masters
	 */
	if (!innetgr(goodgroup, &hostname[0], 0,0)) {
		if (trace)
			(void)fprintf(fd, "get_goodgroup: %s not in %s\n",
				      &hostname[0], goodgroup);
		return;
	}
	if (trace)
		(void)fprintf(fd, "get_goodgroup: %s in %s\n",
				  &hostname[0], goodgroup);

	/* mark the entire netgroup as trusted */
	(void)setnetgrent(goodgroup);
	while (getnetgrent(&mach,&usr,&dom)) {
		if (0 != mach)
			add_good_host(mach,0);
	}
	(void)endnetgrent();

	/* update list of slaves */
	for (htp = self.l_fwd; htp != &self; htp = htp->l_fwd) {
		htp->good = good_host_name(&htp->name[0]);
	}
}


/* see if a machine is trustworthy
 */
int					/* 1=trust hp to change our date */
good_host_name(const char *name)
{
	struct goodhost *ghp;

	if (TAILQ_EMPTY(&goodhosts) || !Mflag)
		return (1);

	TAILQ_FOREACH(ghp, &goodhosts, next) {
		if (strcasecmp(ghp->name, name) == 0)
			return (1);
	}

	/* XXX - Should be no need for this since we already added ourselves */
	if (strcasecmp(name, hostname) == 0)
		return (1);

	return (0);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: timed [-dMt] [-F host ...] [-G netgroup] "
	    "[-i network | -n network]\n");
	exit(1);
}
