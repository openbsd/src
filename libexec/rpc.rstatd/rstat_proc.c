/*	$OpenBSD: rstat_proc.c,v 1.26 2004/09/15 19:05:35 deraadt Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#ifndef lint
/*static char sccsid[] = "from: @(#)rpc.rstatd.c 1.1 86/09/25 Copyr 1984 Sun Micro";*/
/*static char sccsid[] = "from: @(#)rstat_proc.c	2.2 88/08/01 4.0 RPCSRC";*/
static char rcsid[] = "$OpenBSD: rstat_proc.c,v 1.26 2004/09/15 19:05:35 deraadt Exp $";
#endif

/*
 * rstat service:  built with rstat.x and derived from rpc.rstatd.c
 *
 * Copyright (c) 1984 by Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/vmmeter.h>
#include <sys/dkstat.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <uvm/uvm_extern.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <ifaddrs.h>
#include "dkstats.h"

#undef FSHIFT			 /* Use protocol's shift and scale values */
#undef FSCALE
#undef DK_NDRIVE
#undef CPUSTATES
#undef if_ipackets
#undef if_ierrors
#undef if_opackets
#undef if_oerrors
#undef if_collisions
#include <rpcsvc/rstat.h>

int	cp_xlat[CPUSTATES] = { CP_USER, CP_NICE, CP_SYS, CP_IDLE };

extern int dk_ndrive;		/* from dkstats.c */
extern struct _disk cur, last;
char *memf = NULL, *nlistf = NULL;
int hz;

extern int from_inetd;
int sincelastreq = 0;		/* number of alarms since last request */
extern int closedown;

union {
	struct stats s1;
	struct statsswtch s2;
	struct statstime s3;
} stats_all;

void	updatestat(void);
void	updatestatsig(int sig);
void	setup(void);

volatile sig_atomic_t wantupdatestat;

static int stat_is_init = 0;

#ifndef FSCALE
#define FSCALE (1 << 8)
#endif

static void
stat_init(void)
{
	stat_is_init = 1;
	setup();
	updatestat();
	(void) signal(SIGALRM, updatestatsig);
	alarm(1);
}

statstime *
rstatproc_stats_3_svc(void *arg, struct svc_req *rqstp)
{
	if (!stat_is_init)
		stat_init();
	sincelastreq = 0;
	return (&stats_all.s3);
}

statsswtch *
rstatproc_stats_2_svc(void *arg, struct svc_req *rqstp)
{
	if (!stat_is_init)
		stat_init();
	sincelastreq = 0;
	return (&stats_all.s2);
}

stats *
rstatproc_stats_1_svc(void *arg, struct svc_req *rqstp)
{
	if (!stat_is_init)
		stat_init();
	sincelastreq = 0;
	return (&stats_all.s1);
}

u_int *
rstatproc_havedisk_3_svc(void *arg, struct svc_req *rqstp)
{
	static u_int have;

	if (!stat_is_init)
		stat_init();
	sincelastreq = 0;
	have = dk_ndrive != 0;
	return (&have);
}

u_int *
rstatproc_havedisk_2_svc(void *arg, struct svc_req *rqstp)
{
	return (rstatproc_havedisk_3_svc(arg, rqstp));
}

u_int *
rstatproc_havedisk_1_svc(void *arg, struct svc_req *rqstp)
{
	return (rstatproc_havedisk_3_svc(arg, rqstp));
}

/* ARGSUSED */
void
updatestatsig(int sig)
{
	wantupdatestat = 1;
}

void
updatestat(void)
{
	int i, mib[2], save_errno = errno;
	struct uvmexp uvmexp;
	size_t len;
	struct if_data *ifdp;
	struct ifaddrs *ifaddrs, *ifa;
	double avrun[3];
	struct timeval tm, btm;
	long *cp_time = cur.cp_time;

#ifdef DEBUG
	syslog(LOG_DEBUG, "entering updatestat");
#endif
	if (sincelastreq >= closedown) {
#ifdef DEBUG
		syslog(LOG_DEBUG, "about to closedown");
#endif
		if (from_inetd)
			_exit(0);
		else {
			stat_is_init = 0;
			errno = save_errno;
			return;
		}
	}
	sincelastreq++;

	/*
	 * dkreadstats reads in the "disk_count" as well as the "disklist"
	 * statistics.  It also retrieves "hz" and the "cp_time" array.
	 */
	dkreadstats();
	memset(stats_all.s1.dk_xfer, '\0', sizeof(stats_all.s1.dk_xfer));
	for (i = 0; i < dk_ndrive && i < DK_NDRIVE; i++)
		stats_all.s1.dk_xfer[i] = cur.dk_rxfer[i] + cur.dk_wxfer[i];

	for (i = 0; i < CPUSTATES; i++)
		stats_all.s1.cp_time[i] = cp_time[cp_xlat[i]];
	(void)getloadavg(avrun, sizeof(avrun) / sizeof(avrun[0]));
	stats_all.s2.avenrun[0] = avrun[0] * FSCALE;
	stats_all.s2.avenrun[1] = avrun[1] * FSCALE;
	stats_all.s2.avenrun[2] = avrun[2] * FSCALE;
	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	len = sizeof(btm);
	if (sysctl(mib, 2, &btm, &len, NULL, 0) < 0) {
		syslog(LOG_ERR, "can't sysctl kern.boottime: %m");
		_exit(1);
	}
	stats_all.s2.boottime.tv_sec = btm.tv_sec;
	stats_all.s2.boottime.tv_usec = btm.tv_usec;


#ifdef DEBUG
	syslog(LOG_DEBUG, "%d %d %d %d", stats_all.s1.cp_time[0],
	    stats_all.s1.cp_time[1], stats_all.s1.cp_time[2],
	    stats_all.s1.cp_time[3]);
#endif

	mib[0] = CTL_VM;
	mib[1] = VM_UVMEXP;
	len = sizeof(uvmexp);
	if (sysctl(mib, 2, &uvmexp, &len, NULL, 0) < 0) {
		syslog(LOG_ERR, "can't sysctl vm.uvmexp: %m");
		_exit(1);
	}
	stats_all.s1.v_pgpgin = uvmexp.fltanget;
	stats_all.s1.v_pgpgout = uvmexp.pdpageouts;
	stats_all.s1.v_pswpin = uvmexp.swapins;
	stats_all.s1.v_pswpout = uvmexp.swapouts;
	stats_all.s1.v_intr = uvmexp.intrs;
	stats_all.s2.v_swtch = uvmexp.swtch;
	gettimeofday(&tm, (struct timezone *) 0);
	stats_all.s1.v_intr -= hz*(tm.tv_sec - btm.tv_sec) +
	    hz*(tm.tv_usec - btm.tv_usec)/1000000;
	stats_all.s1.if_ipackets = 0;
	stats_all.s1.if_opackets = 0;
	stats_all.s1.if_ierrors = 0;
	stats_all.s1.if_oerrors = 0;
	stats_all.s1.if_collisions = 0;
	if (getifaddrs(&ifaddrs) == -1) {
		syslog(LOG_ERR, "can't getifaddrs: %m");
		_exit(1);
	}
	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		ifdp = (struct if_data *)ifa->ifa_data;
		stats_all.s1.if_ipackets += ifdp->ifi_ipackets;
		stats_all.s1.if_opackets += ifdp->ifi_opackets;
		stats_all.s1.if_ierrors += ifdp->ifi_ierrors;
		stats_all.s1.if_oerrors += ifdp->ifi_oerrors;
		stats_all.s1.if_collisions += ifdp->ifi_collisions;
	}
	freeifaddrs(ifaddrs);
	stats_all.s3.curtime.tv_sec = tm.tv_sec;
	stats_all.s3.curtime.tv_usec = tm.tv_usec;

	alarm(1);
	errno = save_errno;
}

void
setup(void)
{
	dkinit(0);
}

void	rstat_service(struct svc_req *, SVCXPRT *);

void
rstat_service(struct svc_req *rqstp, SVCXPRT *transp)
{
	char *(*local)(void *, struct svc_req *);
	xdrproc_t xdr_argument, xdr_result;
	union {
		int fill;
	} argument;
	char *result;

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		return;

	case RSTATPROC_STATS:
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_statstime;
		switch (rqstp->rq_vers) {
		case RSTATVERS_ORIG:
			local = (char *(*)(void *, struct svc_req *))
				rstatproc_stats_1_svc;
			break;
		case RSTATVERS_SWTCH:
			local = (char *(*)(void *, struct svc_req *))
				rstatproc_stats_2_svc;
			break;
		case RSTATVERS_TIME:
			local = (char *(*)(void *, struct svc_req *))
				rstatproc_stats_3_svc;
			break;
		default:
			svcerr_progvers(transp, RSTATVERS_ORIG, RSTATVERS_TIME);
			return;
		}
		break;

	case RSTATPROC_HAVEDISK:
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_u_int;
		switch (rqstp->rq_vers) {
		case RSTATVERS_ORIG:
			local = (char *(*)(void *, struct svc_req *))
				rstatproc_havedisk_1_svc;
			break;
		case RSTATVERS_SWTCH:
			local = (char *(*)(void *, struct svc_req *))
				rstatproc_havedisk_2_svc;
			break;
		case RSTATVERS_TIME:
			local = (char *(*)(void *, struct svc_req *))
				rstatproc_havedisk_3_svc;
			break;
		default:
			svcerr_progvers(transp, RSTATVERS_ORIG, RSTATVERS_TIME);
			return;
		}
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
}
