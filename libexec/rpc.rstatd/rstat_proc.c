/*	$OpenBSD: rstat_proc.c,v 1.10 1998/07/10 08:06:10 deraadt Exp $	*/

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
static char rcsid[] = "$OpenBSD: rstat_proc.c,v 1.10 1998/07/10 08:06:10 deraadt Exp $";
#endif

/*
 * rstat service:  built with rstat.x and derived from rpc.rstatd.c
 *
 * Copyright (c) 1984 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <nlist.h>
#include <syslog.h>
#include <sys/errno.h>
#include <sys/param.h>
#ifdef BSD
#include <sys/vmmeter.h>
#include <sys/dkstat.h>
#include "dkstats.h"
#else
#include <sys/dk.h>
#endif
#include <net/if.h>

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

#ifdef BSD
#define BSD_CPUSTATES	5	/* Use protocol's idea of CPU states */
int	cp_xlat[CPUSTATES] = { CP_USER, CP_NICE, CP_SYS, CP_IDLE };
#endif

struct nlist nl[] = {
#define	X_CNT		0
	{ "_cnt" },
#define	X_IFNET		1
	{ "_ifnet" },
#define	X_BOOTTIME	2
	{ "_boottime" },
#ifndef BSD
#define	X_HZ		3
	{ "_hz" },
#define	X_CPTIME	4
	{ "_cp_time" },
#define	X_DKXFER	5
	{ "_dk_xfer" },
#endif
	{ NULL },
};

#ifdef BSD
extern int dk_ndrive;		/* from dkstats.c */
extern struct _disk cur, last;
char *memf = NULL, *nlistf = NULL;
#endif
int hz;

struct ifnet_head ifnetq;	/* chain of ethernet interfaces */
int numintfs;
int stats_service();

extern int from_inetd;
int sincelastreq = 0;		/* number of alarms since last request */
extern int closedown;
kvm_t *kfd;

union {
	struct stats s1;
	struct statsswtch s2;
	struct statstime s3;
} stats_all;

void updatestat();
static stat_is_init = 0;
extern int errno;

#ifndef FSCALE
#define FSCALE (1 << 8)
#endif

stat_init()
{
	stat_is_init = 1;
	setup();
	updatestat();
	(void) signal(SIGALRM, updatestat);
	alarm(1);
}

statstime *
rstatproc_stats_3_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	if (!stat_is_init)
		stat_init();
	sincelastreq = 0;
	return (&stats_all.s3);
}

statsswtch *
rstatproc_stats_2_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	if (!stat_is_init)
		stat_init();
	sincelastreq = 0;
	return (&stats_all.s2);
}

stats *
rstatproc_stats_1_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	if (!stat_is_init)
		stat_init();
	sincelastreq = 0;
	return (&stats_all.s1);
}

u_int *
rstatproc_havedisk_3_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	static u_int have;

	if (!stat_is_init)
		stat_init();
	sincelastreq = 0;
	have = havedisk();
	return (&have);
}

u_int *
rstatproc_havedisk_2_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	return (rstatproc_havedisk_3_svc(arg, rqstp));
}

u_int *
rstatproc_havedisk_1_svc(arg, rqstp)
	void *arg;
	struct svc_req *rqstp;
{
	return (rstatproc_havedisk_3_svc(arg, rqstp));
}

void
updatestat()
{
	long off;
	int i, save_errno = errno;
	struct vmmeter cnt;
	struct ifnet ifnet;
	double avrun[3];
	struct timeval tm, btm;
#ifdef BSD
	long *cp_time = cur.cp_time;
#endif

#ifdef DEBUG
	syslog(LOG_DEBUG, "entering updatestat");
#endif
	if (sincelastreq >= closedown) {
#ifdef DEBUG
		syslog(LOG_DEBUG, "about to closedown");
#endif
		if (from_inetd)
			exit(0);
		else {
			stat_is_init = 0;
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
		stats_all.s1.dk_xfer[i] = cur.dk_xfer[i];
	
#ifdef BSD
	for (i = 0; i < CPUSTATES; i++)
		stats_all.s1.cp_time[i] = cp_time[cp_xlat[i]];
#else
	if (kvm_read(kfd, (long)nl[X_HZ].n_value, (char *)&hz, sizeof hz) !=
	    sizeof hz) {
		syslog(LOG_ERR, "can't read hz from kmem");
		exit(1);
	}
 	if (kvm_read(kfd, (long)nl[X_CPTIME].n_value,
	    (char *)stats_all.s1.cp_time, sizeof (stats_all.s1.cp_time))
	    != sizeof (stats_all.s1.cp_time)) {
		syslog(LOG_ERR, "can't read cp_time from kmem");
		exit(1);
	}
#endif
#ifdef BSD
	(void)getloadavg(avrun, sizeof(avrun) / sizeof(avrun[0]));
#endif
	stats_all.s2.avenrun[0] = avrun[0] * FSCALE;
	stats_all.s2.avenrun[1] = avrun[1] * FSCALE;
	stats_all.s2.avenrun[2] = avrun[2] * FSCALE;
 	if (kvm_read(kfd, (long)nl[X_BOOTTIME].n_value,
	    (char *)&btm, sizeof (stats_all.s2.boottime))
	    != sizeof (stats_all.s2.boottime)) {
		syslog(LOG_ERR, "can't read boottime from kmem");
		exit(1);
	}
	stats_all.s2.boottime.tv_sec = btm.tv_sec;
	stats_all.s2.boottime.tv_usec = btm.tv_usec;


#ifdef DEBUG
	syslog(LOG_DEBUG, "%d %d %d %d", stats_all.s1.cp_time[0],
	    stats_all.s1.cp_time[1], stats_all.s1.cp_time[2],
	    stats_all.s1.cp_time[3]);
#endif

 	if (kvm_read(kfd, (long)nl[X_CNT].n_value, (char *)&cnt, sizeof cnt) !=
	    sizeof cnt) {
		syslog(LOG_ERR, "can't read cnt from kmem");
		exit(1);
	}
	stats_all.s1.v_pgpgin = cnt.v_pgpgin;
	stats_all.s1.v_pgpgout = cnt.v_pgpgout;
	stats_all.s1.v_pswpin = cnt.v_pswpin;
	stats_all.s1.v_pswpout = cnt.v_pswpout;
	stats_all.s1.v_intr = cnt.v_intr;
	gettimeofday(&tm, (struct timezone *) 0);
	stats_all.s1.v_intr -= hz*(tm.tv_sec - btm.tv_sec) +
	    hz*(tm.tv_usec - btm.tv_usec)/1000000;
	stats_all.s2.v_swtch = cnt.v_swtch;

#ifndef BSD
 	if (kvm_read(kfd, (long)nl[X_DKXFER].n_value,
	    (char *)stats_all.s1.dk_xfer, sizeof (stats_all.s1.dk_xfer))
	    != sizeof (stats_all.s1.dk_xfer)) {
		syslog(LOG_ERR, "can't read dk_xfer from kmem");
		exit(1);
	}
#endif

	stats_all.s1.if_ipackets = 0;
	stats_all.s1.if_opackets = 0;
	stats_all.s1.if_ierrors = 0;
	stats_all.s1.if_oerrors = 0;
	stats_all.s1.if_collisions = 0;
	for (off = (long)ifnetq.tqh_first, i = 0; off && i < numintfs; i++) {
		if (kvm_read(kfd, off, (char *)&ifnet, sizeof ifnet) !=
		    sizeof ifnet) {
			syslog(LOG_ERR, "can't read ifnet from kmem");
			exit(1);
		}
		stats_all.s1.if_ipackets += ifnet.if_data.ifi_ipackets;
		stats_all.s1.if_opackets += ifnet.if_data.ifi_opackets;
		stats_all.s1.if_ierrors += ifnet.if_data.ifi_ierrors;
		stats_all.s1.if_oerrors += ifnet.if_data.ifi_oerrors;
		stats_all.s1.if_collisions += ifnet.if_data.ifi_collisions;
		off = (long)ifnet.if_list.tqe_next;
	}
	gettimeofday((struct timeval *)&stats_all.s3.curtime,
		(struct timezone *) 0);
	alarm(1);
	errno = save_errno;
}

setup()
{
	struct ifnet ifnet;
	long off;
	char errbuf[_POSIX2_LINE_MAX];

	kfd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
	if (kfd == NULL) {
		syslog(LOG_ERR, "%s", errbuf);
		exit (1);
	}

	if (kvm_nlist(kfd, nl) != 0) {
		syslog(LOG_ERR, "can't get namelist");
		exit (1);
	}

	if (kvm_read(kfd, (long)nl[X_IFNET].n_value, &ifnetq,
	    sizeof ifnetq) != sizeof ifnetq) {
		syslog(LOG_ERR, "can't read ifnet queue head from kmem");
		exit(1);
	}

	numintfs = 0;
	for (off = (long)ifnetq.tqh_first; off;) {
		if (kvm_read(kfd, off, (char *)&ifnet, sizeof ifnet) !=
		    sizeof ifnet) {
			syslog(LOG_ERR, "can't read ifnet from kmem");
			exit(1);
		}
		numintfs++;
		off = (long)ifnet.if_list.tqe_next;
	}
#ifdef BSD
	dkinit(0);
#endif
}

/*
 * returns true if have a disk
 */
int
havedisk()
{
#ifdef BSD
	return dk_ndrive != 0;
#else
	int i, cnt;
	long  xfer[DK_NDRIVE];

	if (kvm_nlist(kfd, nl) != 0) {
		syslog(LOG_ERR, "can't get namelist");
		exit (1);
	}

	if (kvm_read(kfd, (long)nl[X_DKXFER].n_value,
		     (char *)xfer, sizeof xfer) != sizeof xfer) {
		syslog(LOG_ERR, "can't read dk_xfer from kmem");
		exit(1);
	}
	cnt = 0;
	for (i=0; i < DK_NDRIVE; i++)
		cnt += xfer[i];
	return (cnt != 0);
#endif
}

void
rstat_service(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		int fill;
	} argument;
	char *result;
	xdrproc_t xdr_argument, xdr_result;
	char *(*local) __P((void *, struct svc_req *));

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		return;

	case RSTATPROC_STATS:
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_statstime;
		switch (rqstp->rq_vers) {
		case RSTATVERS_ORIG:
			local = (char *(*) __P((void *, struct svc_req *)))
				rstatproc_stats_1_svc;
			break;
		case RSTATVERS_SWTCH:
			local = (char *(*) __P((void *, struct svc_req *)))
				rstatproc_stats_2_svc;
			break;
		case RSTATVERS_TIME:
			local = (char *(*) __P((void *, struct svc_req *)))
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
			local = (char *(*) __P((void *, struct svc_req *)))
				rstatproc_havedisk_1_svc;
			break;
		case RSTATVERS_SWTCH:
			local = (char *(*) __P((void *, struct svc_req *)))
				rstatproc_havedisk_2_svc;
			break;
		case RSTATVERS_TIME:
			local = (char *(*) __P((void *, struct svc_req *)))
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
