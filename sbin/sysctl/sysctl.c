/*	$OpenBSD: sysctl.c,v 1.37 1999/02/25 21:59:50 deraadt Exp $	*/
/*	$NetBSD: sysctl.c,v 1.9 1995/09/30 07:12:50 thorpej Exp $	*/

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)sysctl.c	8.5 (Berkeley) 5/9/95";
#else
static char *rcsid = "$OpenBSD: sysctl.c,v 1.37 1999/02/25 21:59:50 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/gmon.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <vm/vm_param.h>
#include <machine/cpu.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#include <netipx/ipx.h>
#include <netipx/ipx_var.h>
#include <netipx/spx_var.h>
#include <ddb/db_var.h>
#include <dev/rndvar.h>
#include <net/pfkeyv2.h>
#include <netinet/ip_ipsp.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef CPU_BIOS
#include <machine/biosvar.h>
#endif

struct ctlname topname[] = CTL_NAMES;
struct ctlname kernname[] = CTL_KERN_NAMES;
struct ctlname vmname[] = CTL_VM_NAMES;
struct ctlname fsname[] = CTL_FS_NAMES;
struct ctlname netname[] = CTL_NET_NAMES;
struct ctlname hwname[] = CTL_HW_NAMES;
struct ctlname username[] = CTL_USER_NAMES;
struct ctlname debugname[CTL_DEBUG_MAXID];
struct ctlname *vfsname;
#ifdef CTL_MACHDEP_NAMES
struct ctlname machdepname[] = CTL_MACHDEP_NAMES;
#endif
struct ctlname ddbname[] = CTL_DDB_NAMES;
char names[BUFSIZ];
int lastused;

struct list {
	struct	ctlname *list;
	int	size;
};
struct list toplist = { topname, CTL_MAXID };
struct list secondlevel[] = {
	{ 0, 0 },			/* CTL_UNSPEC */
	{ kernname, KERN_MAXID },	/* CTL_KERN */
	{ vmname, VM_MAXID },		/* CTL_VM */
	{ fsname, FS_MAXID },		/* CTL_FS */
	{ netname, NET_MAXID },		/* CTL_NET */
	{ 0, CTL_DEBUG_MAXID },		/* CTL_DEBUG */
	{ hwname, HW_MAXID },		/* CTL_HW */
#ifdef CTL_MACHDEP_NAMES
	{ machdepname, CPU_MAXID },	/* CTL_MACHDEP */
#else
	{ 0, 0 },			/* CTL_MACHDEP */
#endif
	{ username, USER_MAXID },	/* CTL_USER_NAMES */
	{ ddbname, DBCTL_MAXID },	/* CTL_DDB_NAMES */
	{ 0, 0 },			/* CTL_VFS */
};

int	Aflag, aflag, nflag, wflag;

/*
 * Variables requiring special processing.
 */
#define	CLOCK		0x00000001
#define	BOOTTIME	0x00000002
#define	CHRDEV		0x00000004
#define	BLKDEV		0x00000008
#define RNDSTATS	0x00000010
#define BADDYNAMIC	0x00000020
#define BIOSGEO		0x00000040
#define BIOSDEV		0x00000080
#define	MAJ2DEV		0x00000100

/* prototypes */
void debuginit __P((void));
void listall __P((char *, struct list *));
void parse __P((char *, int));
void parse_baddynamic __P((int *, size_t, char *, void **, size_t *, int, int));
void usage __P((void));
int findname __P((char *, char *, char **, struct list *));
int sysctl_inet __P((char *, char **, int *, int, int *));
int sysctl_ipsec __P((char *, char **, int *, int, int *));
int sysctl_ipx __P((char *, char **, int *, int, int *));
int sysctl_fs __P((char *, char **, int *, int, int *));
int sysctl_bios __P((char *, char **, int *, int, int *));
void vfsinit __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, lvl1;

	while ((ch = getopt(argc, argv, "Aanw")) != -1) {
		switch (ch) {

		case 'A':
			Aflag = 1;
			break;

		case 'a':
			aflag = 1;
			break;

		case 'n':
			nflag = 1;
			break;

		case 'w':
			wflag = 1;
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0 && (Aflag || aflag)) {
		debuginit();
		vfsinit();
		for (lvl1 = 1; lvl1 < CTL_MAXID; lvl1++)
			listall(topname[lvl1].ctl_name, &secondlevel[lvl1]);
		exit(0);
	}
	if (argc == 0)
		usage();
	for (; *argv != NULL; ++argv)
		parse(*argv, 1);
	exit(0);
}

/*
 * List all variables known to the system.
 */
void
listall(prefix, lp)
	char *prefix;
	struct list *lp;
{
	int lvl2;
	char *cp, name[BUFSIZ];

	if (lp->list == NULL)
		return;
	(void)strncpy(name, prefix, BUFSIZ-1);
	name[BUFSIZ-1] = '\0';
	cp = &name[strlen(name)];
	*cp++ = '.';
	for (lvl2 = 0; lvl2 < lp->size; lvl2++) {
		if (lp->list[lvl2].ctl_name == NULL)
			continue;
		(void)strcpy(cp, lp->list[lvl2].ctl_name);
		parse(name, Aflag);
	}
}

/*
 * Parse a name into a MIB entry.
 * Lookup and print out the MIB entry if it exists.
 * Set a new value if requested.
 */
void
parse(string, flags)
	char *string;
	int flags;
{
	int indx, type, state, intval;
	size_t size, len,  newsize = 0;
	int special = 0;
	void *newval = 0;
	quad_t quadval;
	struct list *lp;
	struct vfsconf vfc;
	int mib[CTL_MAXNAME];
	char *cp, *bufp, buf[BUFSIZ];

	(void)strncpy(buf, string, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	bufp = buf;
	if ((cp = strchr(string, '=')) != NULL) {
		if (!wflag)
			errx(2, "must specify -w to set variables");
		*strchr(buf, '=') = '\0';
		*cp++ = '\0';
		while (isspace(*cp))
			cp++;
		newval = cp;
		newsize = strlen(cp);
	}
	if ((indx = findname(string, "top", &bufp, &toplist)) == -1)
		return;
	mib[0] = indx;
	if (indx == CTL_VFS)
		vfsinit();
	if (indx == CTL_DEBUG)
		debuginit();
	lp = &secondlevel[indx];
	if (lp->list == 0) {
		warnx("%s: class is not implemented", topname[indx].ctl_name);
		return;
	}
	if (bufp == NULL) {
		listall(topname[indx].ctl_name, lp);
		return;
	}
	if ((indx = findname(string, "second", &bufp, lp)) == -1)
		return;
	mib[1] = indx;
	type = lp->list[indx].ctl_type;
	len = 2;
	switch (mib[0]) {

	case CTL_KERN:
		switch (mib[1]) {
		case KERN_PROF:
			mib[2] = GPROF_STATE;
			size = sizeof(state);
			if (sysctl(mib, 3, &state, &size, NULL, 0) == -1) {
				if (flags == 0)
					return;
				if (!nflag)
					(void)printf("%s: ", string);
				(void)puts("kernel is not compiled for profiling");
				return;
			}
			if (!nflag)
				(void)printf("%s = %s\n", string,
				    state == GMON_PROF_OFF ? "off" : "running");
			return;
		case KERN_VNODE:
		case KERN_FILE:
			if (flags == 0)
				return;
			warnx("use pstat to view %s information", string);
			return;
		case KERN_PROC:
			if (flags == 0)
				return;
			warnx("use ps to view %s information", string);
			return;
		case KERN_NTPTIME:
			if (flags == 0)
				return;
			warnx("use xntpdc to view %s information", string);
			return;
		case KERN_CLOCKRATE:
			special |= CLOCK;
			break;
		case KERN_BOOTTIME:
			special |= BOOTTIME;
			break;
		case KERN_RND:
			special |= RNDSTATS;
			break;
		}
		break;

	case CTL_HW:
		break;

	case CTL_VM:
		if (mib[1] == VM_LOADAVG) {
			double loads[3];

			getloadavg(loads, 3);
			if (!nflag)
				(void)printf("%s = ", string);
			(void)printf("%.2f %.2f %.2f\n", loads[0],
			    loads[1], loads[2]);
			return;
		} else if (mib[1] == VM_PSSTRINGS) {
			struct _ps_strings _ps;

			len = sizeof(_ps);
			if (sysctl(mib, 2, &_ps, &len, NULL, 0) == -1) {
				if (flags == 0)
					return;
				if (!nflag)
					(void)printf("%s: ", string);
				(void)puts("can't find ps strings");
				return;
			}
			if (!nflag)
				(void)printf("%s = ", string);
			(void)printf("%p\n", _ps.val);
			return;
		}
		if (flags == 0)
			return;
		warnx("use vmstat or systat to view %s information", string);
		return;

	case CTL_NET:
		if (mib[1] == PF_INET) {
			len = sysctl_inet(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;

			if ((mib[2] == IPPROTO_TCP &&
			     mib[3] == TCPCTL_BADDYNAMIC) ||
			    (mib[2] == IPPROTO_UDP &&
			     mib[3] == UDPCTL_BADDYNAMIC)) {

				special |= BADDYNAMIC;

				if (newval != NULL)
					parse_baddynamic(mib, len, string,
					    &newval, &newsize, flags, nflag);
			}
			break;
		}
		if (mib[1] == PF_IPX) {
			len = sysctl_ipx(string, &bufp, mib, flags, &type);
			if (len >= 0)
				break;
			return;
		}
		if (mib[1] == PF_KEY) {
			len = sysctl_ipsec(string, &bufp, mib, flags, &type);
			if (len >= 0)
				break;
			return;
		}
		if (flags == 0)
			return;
		warnx("use netstat to view %s information", string);
		return;

	case CTL_DEBUG:
		mib[2] = CTL_DEBUG_VALUE;
		len = 3;
		break;

	case CTL_MACHDEP:
#ifdef CPU_CONSDEV
		if (mib[1] == CPU_CONSDEV)
			special |= CHRDEV;
#endif
#ifdef CPU_BLK2CHR
		if (mib[1] == CPU_BLK2CHR) {
			if (bufp == NULL)
				return;
			mib[2] = makedev(atoi(bufp),0);
			bufp = NULL;
			len = 3;
			special |= CHRDEV;
			break;
		}
#endif
#ifdef CPU_CHR2BLK
		if (mib[1] == CPU_CHR2BLK) {
			if (bufp == NULL)
				return;
			mib[2] = makedev(atoi(bufp),0);
			bufp = NULL;
			len = 3;
			special |= BLKDEV;
			break;
		}
#endif
#ifdef CPU_BIOS
		if (mib[1] == CPU_BIOS) {
			len = sysctl_bios(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;
			if (mib[2] == BIOS_DEV)
				special |= BIOSDEV;
			if (mib[2] == BIOS_DISKINFO)
				special |= BIOSGEO;
			break;
		}
#endif
		break;

	case CTL_FS:
		len = sysctl_fs(string, &bufp, mib, flags, &type);
		if (len >= 0)
			break;
		return;

	case CTL_VFS:
		mib[3] = mib[1];
		mib[1] = VFS_GENERIC;
		mib[2] = VFS_CONF;
		len = 4;
		size = sizeof vfc;
		if (sysctl(mib, 4, &vfc, &size, (void *)0, (size_t)0) < 0) {
			if (errno != EOPNOTSUPP)
				perror("vfs print");
			return;
		}
		if (flags == 0 && vfc.vfc_refcount == 0)
			return;
		if (!nflag)
			fprintf(stdout, "%s has %d mounted instance%s\n",
			    string, vfc.vfc_refcount,
			    vfc.vfc_refcount != 1 ? "s" : "");
		else
			fprintf(stdout, "%d\n", vfc.vfc_refcount);
		return;

	case CTL_USER:
	case CTL_DDB:
		break;

	default:
		warnx("illegal top level value: %d", mib[0]);
		return;
	
	}
	if (bufp) {
		warnx("name %s in %s is unknown", bufp, string);
		return;
	}
	if (newsize > 0) {
		switch (type) {
		case CTLTYPE_INT:
			intval = atoi(newval);
			newval = &intval;
			newsize = sizeof(intval);
			break;

		case CTLTYPE_QUAD:
			(void)sscanf(newval, "%qd", &quadval);
			newval = &quadval;
			newsize = sizeof(quadval);
			break;
		}
	}
	size = BUFSIZ;
	if (sysctl(mib, len, buf, &size, newsize ? newval : 0, newsize) == -1) {
		if (flags == 0)
			return;
		switch (errno) {
		case EOPNOTSUPP:
			warnx("%s: value is not available", string);
			return;
		case ENOTDIR:
			warnx("%s: specification is incomplete", string);
			return;
		case ENOMEM:
			warnx("%s: type is unknown to this program", string);
			return;
		case ENXIO:
			if (special & BIOSGEO)
				return;
		default:
			warn(string);
			return;
		}
	}
	if (special & CLOCK) {
		struct clockinfo *clkp = (struct clockinfo *)buf;

		if (!nflag)
			(void)printf("%s = ", string);
		(void)printf(
		    "tick = %d, tickadj = %d, hz = %d, profhz = %d, stathz = %d\n",
		    clkp->tick, clkp->tickadj, clkp->hz, clkp->profhz, clkp->stathz);
		return;
	}
	if (special & BOOTTIME) {
		struct timeval *btp = (struct timeval *)buf;
		time_t boottime;

		if (!nflag) {
			boottime = btp->tv_sec;
			(void)printf("%s = %s", string, ctime(&boottime));
		} else
			(void)printf("%ld\n", btp->tv_sec);
		return;
	}
	if (special & BLKDEV) {
		dev_t dev = *(dev_t *)buf;

		if (!nflag)
			(void)printf("%s = %s\n", string,
			    devname(dev, S_IFBLK));
		else
			(void)printf("0x%x\n", dev);
		return;
	}
	if (special & CHRDEV) {
		dev_t dev = *(dev_t *)buf;

		if (!nflag)
			(void)printf("%s = %s\n", string,
			    devname(dev, S_IFCHR));
		else
			(void)printf("0x%x\n", dev);
		return;
	}
#ifdef CPU_BIOS
	if (special & BIOSGEO) {
		bios_diskinfo_t *pdi = (bios_diskinfo_t *)buf;

		if (!nflag)
			(void)printf("%s = ", string);
		(void)printf("bootdev = 0x%x, "
			     "cylinders = %u, heads = %u, sectors = %u\n",
			     pdi->bsd_dev, pdi->bios_cylinders, pdi->bios_heads,
			     pdi->bios_sectors);
		return;
	}
	if (special & BIOSDEV) {
		int dev = *(int*)buf;

		if (!nflag)
			(void)printf("%s = ", string);
		(void) printf("0x%02x\n", dev);
		return;
	}
#endif
	if (special & RNDSTATS) {
		struct rndstats *rndstats = (struct rndstats *)buf;

		if (!nflag)
			(void)printf("%s = ", string);
		(void)printf(
		    "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
		    rndstats->rnd_total, rndstats->rnd_used,
		    rndstats->arc4_reads, rndstats->rnd_timer,
		    rndstats->rnd_mouse, rndstats->rnd_tty,
		    rndstats->rnd_disk, rndstats->rnd_net,
		    rndstats->rnd_reads, rndstats->rnd_waits,
		    rndstats->rnd_enqs, rndstats->rnd_deqs,
		    rndstats->rnd_drops, rndstats->rnd_drople,
		    rndstats->rnd_asleep, rndstats->rnd_queued);
		return;
	}
	if (special & BADDYNAMIC) {
		in_port_t port, lastport;
		u_int32_t *baddynamic = (u_int32_t *)buf;

		if (!nflag)
			(void)printf("%s%s", string, newsize ? ": " : " = ");
		lastport = 0;
		for (port = IPPORT_RESERVED/2; port < IPPORT_RESERVED; port++)
			if (DP_ISSET(baddynamic, port)) {
				(void)printf("%s%hd", lastport ? "," : "",
				    port);
				lastport = port;
			}
		if (newsize != 0) {
			if (!nflag)
				fputs(" -> ", stdout);
			baddynamic = (u_int32_t *)newval;
			lastport = 0;
			for (port = IPPORT_RESERVED/2; port < IPPORT_RESERVED;
			    port++)
				if (DP_ISSET(baddynamic, port)) {
					(void)printf("%s%hd",
					    lastport ? "," : "", port);
					lastport = port;
				}
		}
		(void)putchar('\n');
		return;
	}
	switch (type) {
	case CTLTYPE_INT:
		if (newsize == 0) {
			if (!nflag)
				(void)printf("%s = ", string);
			(void)printf("%d\n", *(int *)buf);
		} else {
			if (!nflag)
				(void)printf("%s: %d -> ", string,
				    *(int *)buf);
			(void)printf("%d\n", *(int *)newval);
		}
		return;

	case CTLTYPE_STRING:
		if (newsize == 0) {
			if (!nflag)
				(void)printf("%s = ", string);
			(void)puts(buf);
		} else {
			if (!nflag)
				(void)printf("%s: %s -> ", string, buf);
			(void)puts((char *)newval);
		}
		return;

	case CTLTYPE_QUAD:
		if (newsize == 0) {
			if (!nflag)
				(void)printf("%s = ", string);
			(void)printf("%qd\n", *(quad_t *)buf);
		} else {
			if (!nflag)
				(void)printf("%s: %qd -> ", string,
				    *(quad_t *)buf);
			(void)printf("%qd\n", *(quad_t *)newval);
		}
		return;

	case CTLTYPE_STRUCT:
		warnx("%s: unknown structure returned", string);
		return;

	default:
	case CTLTYPE_NODE:
		warnx("%s: unknown type returned", string);
		return;
	}
}

void
parse_baddynamic(mib, len, string, newvalp, newsizep, flags, nflag)
	int mib[];
	size_t len;
	char *string;
	void **newvalp;
	size_t *newsizep;
	int flags;
	int nflag;
{
	static u_int32_t newbaddynamic[DP_MAPSIZE];
	in_port_t port;
	size_t size;
	char action, *cp;

	if (strchr((char *)*newvalp, '+') || strchr((char *)*newvalp, '-')) {
		size = sizeof(newbaddynamic);
		if (sysctl(mib, len, newbaddynamic, &size, 0, 0) == -1) {
			if (flags == 0)
				return;
			if (!nflag)
				(void)printf("%s: ", string);
			(void)puts("kernel does contain bad dynamic port tables");
			return;
		}

		while (*newvalp && (cp = strsep((char **)newvalp, ", \t")) && *cp) {
			if (*cp != '+' && *cp != '-')
				errx(1, "cannot mix +/- with full list");
			action = *cp++;
			port = atoi(cp);
			if (port < IPPORT_RESERVED/2 || port >= IPPORT_RESERVED)
				errx(1, "invalid port, range is %d to %d",
				    IPPORT_RESERVED/2, IPPORT_RESERVED-1);
			if (action == '+')
				DP_SET(newbaddynamic, port);
			else
				DP_CLR(newbaddynamic, port);
		}
	} else {
		(void)memset((void *)newbaddynamic, 0, sizeof(newbaddynamic));
		while (*newvalp && (cp = strsep((char **)newvalp, ", \t")) && *cp) {
			port = atoi(cp);
			if (port < IPPORT_RESERVED/2 || port >= IPPORT_RESERVED)
				errx(1, "invalid port, range is %d to %d",
				    IPPORT_RESERVED/2, IPPORT_RESERVED-1);
			DP_SET(newbaddynamic, port);
		}
	}

	*newvalp = (void *)newbaddynamic;
	*newsizep = sizeof(newbaddynamic);
}

/*
 * Initialize the set of debugging names
 */
void
debuginit()
{
	int mib[3], loc, i;
	size_t size;

	if (secondlevel[CTL_DEBUG].list != 0)
		return;
	secondlevel[CTL_DEBUG].list = debugname;
	mib[0] = CTL_DEBUG;
	mib[2] = CTL_DEBUG_NAME;
	for (loc = lastused, i = 0; i < CTL_DEBUG_MAXID; i++) {
		mib[1] = i;
		size = BUFSIZ - loc;
		if (sysctl(mib, 3, &names[loc], &size, NULL, 0) == -1)
			continue;
		debugname[i].ctl_name = &names[loc];
		debugname[i].ctl_type = CTLTYPE_INT;
		loc += size;
	}
	lastused = loc;
}

/*
 * Initialize the set of filesystem names
 */
void
vfsinit()
{
	int mib[4], maxtypenum, cnt, loc, size;
	struct vfsconf vfc;
	size_t buflen;

	if (secondlevel[CTL_VFS].list != 0)
		return;
	mib[0] = CTL_VFS;
	mib[1] = VFS_GENERIC;
	mib[2] = VFS_MAXTYPENUM;
	buflen = 4;
	if (sysctl(mib, 3, &maxtypenum, &buflen, (void *)0, (size_t)0) < 0)
		return;
	if ((vfsname = malloc(maxtypenum * sizeof(*vfsname))) == 0)
		return;
	memset(vfsname, 0, maxtypenum * sizeof(*vfsname));
	mib[2] = VFS_CONF;
	buflen = sizeof vfc;
	for (loc = lastused, cnt = 0; cnt < maxtypenum; cnt++) {
		mib[3] = cnt;
		if (sysctl(mib, 4, &vfc, &buflen, (void *)0, (size_t)0) < 0) {
			if (errno == EOPNOTSUPP)
				continue;
			perror("vfsinit");
			free(vfsname);
			return;
		}
		strcat(&names[loc], vfc.vfc_name);
		vfsname[cnt].ctl_name = &names[loc];
		vfsname[cnt].ctl_type = CTLTYPE_INT;
		size = strlen(vfc.vfc_name) + 1;
		loc += size;
	}
	lastused = loc;
	secondlevel[CTL_VFS].list = vfsname;
	secondlevel[CTL_VFS].size = maxtypenum;
	return;
}

struct ctlname posixname[] = CTL_FS_POSIX_NAMES;
struct list fslist = { posixname, FS_POSIX_MAXID };

/*
 * handle file system requests
 */
int
sysctl_fs(string, bufpp, mib, flags, typep)
	char *string;
	char **bufpp;
	int mib[];
	int flags;
	int *typep;
{
	int indx;

	if (*bufpp == NULL) {
		listall(string, &fslist);
		return(-1);
	}
	if ((indx = findname(string, "third", bufpp, &fslist)) == -1)
		return(-1);
	mib[2] = indx;
	*typep = fslist.list[indx].ctl_type;
	return(3);
}

#ifdef CPU_BIOS
struct ctlname biosname[] = CTL_BIOS_NAMES;
struct list bioslist = { biosname, BIOS_MAXID };

/*
 * handle BIOS requests
 */
int
sysctl_bios(string, bufpp, mib, flags, typep)
	char *string;
	char **bufpp;
	int mib[];
	int flags;
	int *typep;
{
	char *name;
	int indx;

	if (*bufpp == NULL) {
		listall(string, &bioslist);
		return(-1);
	}
	if ((indx = findname(string, "third", bufpp, &bioslist)) == -1)
		return(-1);
	mib[2] = indx;
	if (indx == BIOS_DISKINFO) {
		if (*bufpp == NULL) {
			char name[BUFSIZ];

			/* scan all the bios devices */
			for (indx = 0; indx < 256; indx++) {
				snprintf(name, sizeof(name), "%s.%u",
					 string, indx);
				parse(name, 1);
			}
			return(-1);
		}
		if ((name = strsep(bufpp, ".")) == NULL) {
			warnx("%s: incomplete specification", string);
			return(-1);
		}
		mib[3] = atoi(name);
		*typep = CTLTYPE_STRUCT;
		return 4;
	} else {
		*typep = bioslist.list[indx].ctl_type;
		return(3);
	}
}
#endif

struct ctlname encapname[] = PFKEYCTL_NAMES;
struct ctlname ipsecname[] = CTL_IPSEC_NAMES;
struct list ipseclist = { ipsecname, IPSECCTL_MAXID };
struct list ipsecvars[] = {
	{ encapname, IPSECCTL_MAXID }, 
};

/*
 * handle ipsec requests
 */
int
sysctl_ipsec(string, bufpp, mib, flags, typep)
	char *string;
	char **bufpp;
	int mib[];
	int flags;
	int *typep;
{
	struct list *lp;
	int indx;

	if (*bufpp == NULL) {
		listall(string, &ipseclist);
		return(-1);
	}
	if ((indx = findname(string, "third", bufpp, &ipseclist)) == -1)
		return(-1);
	mib[2] = indx;
	if (indx <= IPSECCTL_MAXID && ipsecvars[indx].list != NULL)
		lp = &ipsecvars[indx];
	else if (!flags)
		return(-1);
	else {
		warnx("%s: no variables defined for this protocol", string);
		return(-1);
	}
	if (*bufpp == NULL) {
		listall(string, lp);
		return(-1);
	}
	if ((indx = findname(string, "fourth", bufpp, lp)) == -1)
		return(-1);
	mib[3] = indx;
	*typep = lp->list[indx].ctl_type;
	return(4);
}

struct ctlname inetname[] = CTL_IPPROTO_NAMES;
struct ctlname ipname[] = IPCTL_NAMES;
struct ctlname icmpname[] = ICMPCTL_NAMES;
struct ctlname tcpname[] = TCPCTL_NAMES;
struct ctlname udpname[] = UDPCTL_NAMES;
struct list inetlist = { inetname, IPPROTO_MAXID };
struct list inetvars[] = {
	{ ipname, IPCTL_MAXID },	/* ip */
	{ icmpname, ICMPCTL_MAXID },	/* icmp */
	{ 0, 0 },			/* igmp */
	{ 0, 0 },			/* ggmp */
	{ 0, 0 },
	{ 0, 0 },
	{ tcpname, TCPCTL_MAXID },	/* tcp */
	{ 0, 0 },
	{ 0, 0 },			/* egp */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },			/* pup */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ udpname, UDPCTL_MAXID },	/* udp */
};

/*
 * handle internet requests
 */
int
sysctl_inet(string, bufpp, mib, flags, typep)
	char *string;
	char **bufpp;
	int mib[];
	int flags;
	int *typep;
{
	struct list *lp;
	int indx;

	if (*bufpp == NULL) {
		listall(string, &inetlist);
		return(-1);
	}
	if ((indx = findname(string, "third", bufpp, &inetlist)) == -1)
		return(-1);
	mib[2] = indx;
	if (indx <= IPPROTO_UDP && inetvars[indx].list != NULL)
		lp = &inetvars[indx];
	else if (!flags)
		return(-1);
	else {
		warnx("%s: no variables defined for this protocol", string);
		return(-1);
	}
	if (*bufpp == NULL) {
		listall(string, lp);
		return(-1);
	}
	if ((indx = findname(string, "fourth", bufpp, lp)) == -1)
		return(-1);
	mib[3] = indx;
	*typep = lp->list[indx].ctl_type;
	return(4);
}

struct ctlname ipxname[] = CTL_IPXPROTO_NAMES;
struct ctlname ipxpname[] = IPXCTL_NAMES;
struct ctlname spxpname[] = SPXCTL_NAMES;
struct list ipxlist = { ipxname, IPXCTL_MAXID };
struct list ipxvars[] = {
	{ ipxpname, IPXCTL_MAXID },	/* ipx */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ spxpname, SPXCTL_MAXID },
};

/*
 * Handle internet requests
 */
int
sysctl_ipx(string, bufpp, mib, flags, typep)
	char *string;
	char **bufpp;
	int mib[];
	int flags;
	int *typep;
{
	struct list *lp;
	int indx;

	if (*bufpp == NULL) {
		listall(string, &ipxlist);
		return(-1);
	}
	if ((indx = findname(string, "third", bufpp, &ipxlist)) == -1)
		return(-1);
	mib[2] = indx;
	if (indx <= IPXPROTO_SPX && ipxvars[indx].list != NULL)
		lp = &ipxvars[indx];
	else if (!flags)
		return(-1);
	else {
		warnx("%s: no variables defined for this protocol", string);
		return(-1);
	}
	if (*bufpp == NULL) {
		listall(string, lp);
		return(-1);
	}
	if ((indx = findname(string, "fourth", bufpp, lp)) == -1)
		return(-1);
	mib[3] = indx;
	*typep = lp->list[indx].ctl_type;
	return(4);
}

/*
 * Scan a list of names searching for a particular name.
 */
int
findname(string, level, bufp, namelist)
	char *string;
	char *level;
	char **bufp;
	struct list *namelist;
{
	char *name;
	int i;

	if (namelist->list == 0 || (name = strsep(bufp, ".")) == NULL) {
		warnx("%s: incomplete specification", string);
		return(-1);
	}
	for (i = 0; i < namelist->size; i++)
		if (namelist->list[i].ctl_name != NULL &&
		    strcmp(name, namelist->list[i].ctl_name) == 0)
			break;
	if (i == namelist->size) {
		warnx("%s level name %s in %s is invalid", level, name, string);
		return(-1);
	}
	return(i);
}

void
usage()
{

	(void)fprintf(stderr, "usage:\t%s\n\t%s\n\t%s\n\t%s\n",
	    "sysctl [-n] variable ...", "sysctl [-n] -w variable=value ...",
	    "sysctl [-n] -a", "sysctl [-n] -A");
	exit(1);
}
