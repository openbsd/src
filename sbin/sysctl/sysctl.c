/*	$OpenBSD: sysctl.c,v 1.116 2004/09/14 22:09:38 deraadt Exp $	*/
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)sysctl.c	8.5 (Berkeley) 5/9/95";
#else
static const char rcsid[] = "$OpenBSD: sysctl.c,v 1.116 2004/09/14 22:09:38 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/gmon.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/dkstat.h>
#include <sys/uio.h>
#include <sys/tty.h>
#include <sys/namei.h>
#include <sys/sensors.h>
#include <machine/cpu.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_ipip.h>
#include <netinet/ip_ether.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/ip_gre.h>
#include <netinet/ip_ipcomp.h>
#include <netinet/ip_carp.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/pim6_var.h>
#endif

#include <uvm/uvm_swap_encrypt.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <netipx/ipx.h>
#include <netipx/ipx_var.h>
#include <netipx/spx_var.h>
#include <ddb/db_var.h>
#include <dev/rndvar.h>

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
struct ctlname kernmallocname[] = CTL_KERN_MALLOC_NAMES;
struct ctlname forkstatname[] = CTL_KERN_FORKSTAT_NAMES;
struct ctlname nchstatsname[] = CTL_KERN_NCHSTATS_NAMES;
struct ctlname ttysname[] = CTL_KERN_TTY_NAMES;
struct ctlname semname[] = CTL_KERN_SEMINFO_NAMES;
struct ctlname shmname[] = CTL_KERN_SHMINFO_NAMES;
struct ctlname watchdogname[] = CTL_KERN_WATCHDOG_NAMES;
struct ctlname tcname[] = CTL_KERN_TIMECOUNTER_NAMES;
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

int	Aflag, aflag, nflag, qflag;

/*
 * Variables requiring special processing.
 */
#define	CLOCK		0x00000001
#define	BOOTTIME	0x00000002
#define	CHRDEV		0x00000004
#define	BLKDEV		0x00000008
#define	RNDSTATS	0x00000010
#define	BADDYNAMIC	0x00000020
#define	BIOSGEO		0x00000040
#define	BIOSDEV		0x00000080
#define	MAJ2DEV		0x00000100
#define	UNSIGNED	0x00000200
#define	KMEMBUCKETS	0x00000400
#define	LONGARRAY	0x00000800
#define	KMEMSTATS	0x00001000
#define	SENSORS		0x00002000

/* prototypes */
void debuginit(void);
void listall(char *, struct list *);
void parse(char *, int);
void parse_baddynamic(int *, size_t, char *, void **, size_t *, int, int);
void usage(void);
int findname(char *, char *, char **, struct list *);
int sysctl_inet(char *, char **, int *, int, int *);
#ifdef INET6
int sysctl_inet6(char *, char **, int *, int, int *);
#endif
int sysctl_bpf(char *, char **, int *, int, int *);
int sysctl_ipx(char *, char **, int *, int, int *);
int sysctl_fs(char *, char **, int *, int, int *);
static int sysctl_vfs(char *, char **, int[], int, int *);
static int sysctl_vfsgen(char *, char **, int[], int, int *);
int sysctl_bios(char *, char **, int *, int, int *);
int sysctl_swpenc(char *, char **, int *, int, int *);
int sysctl_forkstat(char *, char **, int *, int, int *);
int sysctl_tty(char *, char **, int *, int, int *);
int sysctl_nchstats(char *, char **, int *, int, int *);
int sysctl_malloc(char *, char **, int *, int, int *);
int sysctl_seminfo(char *, char **, int *, int, int *);
int sysctl_shminfo(char *, char **, int *, int, int *);
int sysctl_watchdog(char *, char **, int *, int, int *);
int sysctl_tc(char *, char **, int *, int, int *);
int sysctl_sensors(char *, char **, int *, int, int *);
int sysctl_emul(char *, char *, int);
#ifdef CPU_CHIPSET
int sysctl_chipset(char *, char **, int *, int, int *);
#endif
void vfsinit(void);

char *equ = "=";

int
main(int argc, char *argv[])
{
	int ch, lvl1;

	while ((ch = getopt(argc, argv, "Aanqw")) != -1) {
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

		case 'q':
			qflag = 1;
			break;

		case 'w':
			/* flag no longer needed; var=value implies write */
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
		return (0);
	}
	if (argc == 0)
		usage();
	for (; *argv != NULL; ++argv)
		parse(*argv, 1);
	return (0);
}

/*
 * List all variables known to the system.
 */
void
listall(char *prefix, struct list *lp)
{
	char *cp, name[BUFSIZ];
	int lvl2, len;

	if (lp->list == NULL)
		return;
	if ((len = strlcpy(name, prefix, sizeof(name))) >= sizeof(name))
		warn("%s: name too long", prefix);
	cp = name + len++;
	*cp++ = '.';
	for (lvl2 = 0; lvl2 < lp->size; lvl2++) {
		if (lp->list[lvl2].ctl_name == NULL)
			continue;
		if (strlcpy(cp, lp->list[lvl2].ctl_name,
		    sizeof(name) - len) >= sizeof(name) - len)
			warn("%s: name too long", lp->list[lvl2].ctl_name);
		parse(name, Aflag);
	}
}

/*
 * Parse a name into a MIB entry.
 * Lookup and print out the MIB entry if it exists.
 * Set a new value if requested.
 */
void
parse(char *string, int flags)
{
	int indx, type, state, intval, len;
	size_t size, newsize = 0;
	int lal = 0, special = 0;
	void *newval = 0;
	int64_t quadval;
	struct list *lp;
	int mib[CTL_MAXNAME];
	char *cp, *bufp, buf[BUFSIZ];

	(void)strlcpy(buf, string, sizeof(buf));
	bufp = buf;
	if ((cp = strchr(string, '=')) != NULL) {
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
		case KERN_FORKSTAT:
			sysctl_forkstat(string, &bufp, mib, flags, &type);
			return;
		case KERN_TTY:
			len = sysctl_tty(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;
			newsize = 0;
			break;
		case KERN_NCHSTATS:
			sysctl_nchstats(string, &bufp, mib, flags, &type);
			return;
		case KERN_MALLOCSTATS:
			len = sysctl_malloc(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;
			if (mib[2] == KERN_MALLOC_BUCKET)
				special |= KMEMBUCKETS;
			if (mib[2] == KERN_MALLOC_KMEMSTATS)
				special |= KMEMSTATS;
			newsize = 0;
			break;
		case KERN_MBSTAT:
			if (flags == 0)
				return;
			warnx("use netstat to view %s", string);
			return;
		case KERN_MSGBUF:
			if (flags == 0)
				return;
			warnx("use dmesg to view %s", string);
			return;
		case KERN_VNODE:
		case KERN_FILE:
			if (flags == 0)
				return;
			warnx("use pstat to view %s information", string);
			return;
		case KERN_PROC:
		case KERN_PROC2:
			if (flags == 0)
				return;
			warnx("use ps to view %s information", string);
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
		case KERN_HOSTID:
		case KERN_ARND:
			special |= UNSIGNED;
			break;
		case KERN_CPTIME:
			special |= LONGARRAY;
			lal = CPUSTATES;
			break;
		case KERN_SEMINFO:
			len = sysctl_seminfo(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;
			break;
		case KERN_SHMINFO:
			len = sysctl_shminfo(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;
			break;
		case KERN_WATCHDOG:
			len = sysctl_watchdog(string, &bufp, mib, flags,
			    &type);
			if (len < 0)
				return;
			break;
		case KERN_TIMECOUNTER:
			len = sysctl_tc(string, &bufp, mib, flags,
			    &type);
			if (len < 0)
				return;
			break;
		case KERN_EMUL:
			sysctl_emul(string, newval, flags);
			return;
		}
		break;

	case CTL_HW:
		switch (mib[1]) {
		case HW_DISKSTATS:
			/*
			 * Only complain if someone asks explicitly for this,
			 * otherwise "fail" silently.
			 */
			if (flags)
				warnx("use vmstat to view %s information",
				    string);
			return;
		case HW_SENSORS:
			special |= SENSORS;
			len = sysctl_sensors(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;
			break;
		}
		break;

	case CTL_VM:
		if (mib[1] == VM_LOADAVG) {
			double loads[3];

			getloadavg(loads, 3);
			if (!nflag)
				(void)printf("%s%s", string, equ);
			(void)printf("%.2f %.2f %.2f\n", loads[0],
			    loads[1], loads[2]);
			return;
		} else if (mib[1] == VM_PSSTRINGS) {
			struct _ps_strings _ps;

			size = sizeof(_ps);
			if (sysctl(mib, 2, &_ps, &size, NULL, 0) == -1) {
				if (flags == 0)
					return;
				if (!nflag)
					(void)printf("%s: ", string);
				(void)puts("can't find ps strings");
				return;
			}
			if (!nflag)
				(void)printf("%s%s", string, equ);
			(void)printf("%p\n", _ps.val);
			return;
		} else if (mib[1] == VM_SWAPENCRYPT) {
			len = sysctl_swpenc(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;

			break;
		} else if (mib[1] == VM_NKMEMPAGES ||
		    mib[1] == VM_ANONMIN ||
		    mib[1] == VM_VTEXTMIN ||
		    mib[1] == VM_VNODEMIN) {
			break;
		}
		if (flags == 0)
			return;
		warnx("use vmstat or systat to view %s information", string);
		return;

		break;

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
#ifdef INET6
		if (mib[1] == PF_INET6) {
			len = sysctl_inet6(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;

			break;
		}
#endif
		if (mib[1] == PF_IPX) {
			len = sysctl_ipx(string, &bufp, mib, flags, &type);
			if (len >= 0)
				break;
			return;
		}
		if (mib[1] == PF_BPF) {
			len = sysctl_bpf(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;
			break;
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
#ifdef CPU_CHIPSET
		if (mib[1] == CPU_CHIPSET) {
			len = sysctl_chipset(string, &bufp, mib, flags, &type);
			if (len < 0)
				return;
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
		if (mib[1])
			len = sysctl_vfs(string, &bufp, mib, flags, &type);
		else
			len = sysctl_vfsgen(string, &bufp, mib, flags, &type);
		if (len >= 0) {
			if (type == CTLTYPE_STRUCT) {
				if (flags)
					warnx("use nfsstat to view %s information",
					    MOUNT_NFS);
				return;
			} else
				break;
		}
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
			errno = 0;
			if (special & UNSIGNED)
				intval = strtoul(newval, &cp, 10);
			else
				intval = strtol(newval, &cp, 10);
			if (*cp != '\0') {
				warnx("%s: illegal value: %s", string,
				    (char *)newval);
				return;
			}
			if (errno == ERANGE) {
				warnx("%s: value %s out of range", string,
				    (char *)newval);
				return;
			}
			newval = &intval;
			newsize = sizeof(intval);
			break;

		case CTLTYPE_QUAD:
			/* XXX - assumes sizeof(long long) == sizeof(quad_t) */
			(void)sscanf(newval, "%lld", (long long *)&quadval);
			newval = &quadval;
			newsize = sizeof(quadval);
			break;
		}
	}
	size = BUFSIZ;
	if (sysctl(mib, len, buf, &size, newval, newsize) == -1) {
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
			warn("%s", string);
			return;
		}
	}
	if (special & KMEMBUCKETS) {
		struct kmembuckets *kb = (struct kmembuckets *)buf;
		if (!nflag)
			(void)printf("%s%s", string, equ);
		printf("(");
		printf("calls = %llu ", (long long)kb->kb_calls);
		printf("total_allocated = %llu ", (long long)kb->kb_total);
		printf("total_free = %lld ", (long long)kb->kb_totalfree);
		printf("elements = %lld ", (long long)kb->kb_elmpercl);
		printf("high watermark = %lld ", (long long)kb->kb_highwat);
		printf("could_free = %lld", (long long)kb->kb_couldfree);
		printf(")\n");
		return;
	}
	if (special & KMEMSTATS) {
		struct kmemstats *km = (struct kmemstats *)buf;
		int j, first = 1;

		if (!nflag)
			(void)printf("%s%s", string, equ);
		(void)printf("(inuse = %ld, calls = %ld, memuse = %ldK, "
		    "limblocks = %d, mapblocks = %d, maxused = %ldK, "
		    "limit = %ldK, spare = %ld, sizes = (",
		    km->ks_inuse, km->ks_calls,
		    (km->ks_memuse + 1023) / 1024, km->ks_limblocks,
		    km->ks_mapblocks, (km->ks_maxused + 1023) / 1024,
		    (km->ks_limit + 1023) / 1024, km->ks_spare);
		for (j = 1 << MINBUCKET; j < 1 << (MINBUCKET + 16); j <<= 1) {
			if ((km->ks_size & j ) == 0)
				continue;
			if (first)
				(void)printf("%d", j);
			else
				(void)printf(",%d", j);
			first = 0;
		}
		if (first)
			(void)printf("none");
		(void)printf("))\n");
		return;
	}
	if (special & CLOCK) {
		struct clockinfo *clkp = (struct clockinfo *)buf;

		if (!nflag)
			(void)printf("%s%s", string, equ);
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
			(void)printf("%s%s%s", string, equ, ctime(&boottime));
		} else
			(void)printf("%ld\n", btp->tv_sec);
		return;
	}
	if (special & BLKDEV) {
		dev_t dev = *(dev_t *)buf;

		if (!nflag)
			(void)printf("%s%s%s\n", string, equ,
			    devname(dev, S_IFBLK));
		else
			(void)printf("0x%x\n", dev);
		return;
	}
	if (special & CHRDEV) {
		dev_t dev = *(dev_t *)buf;

		if (!nflag)
			(void)printf("%s%s%s\n", string, equ,
			    devname(dev, S_IFCHR));
		else
			(void)printf("0x%x\n", dev);
		return;
	}
#ifdef CPU_BIOS
	if (special & BIOSGEO) {
		bios_diskinfo_t *pdi = (bios_diskinfo_t *)buf;

		if (!nflag)
			(void)printf("%s%s", string, equ);
		(void)printf("bootdev = 0x%x, "
		    "cylinders = %u, heads = %u, sectors = %u\n",
		    pdi->bsd_dev, pdi->bios_cylinders,
		    pdi->bios_heads, pdi->bios_sectors);
		return;
	}
	if (special & BIOSDEV) {
		int dev = *(int*)buf;

		if (!nflag)
			(void)printf("%s%s", string, equ);
		(void) printf("0x%02x\n", dev);
		return;
	}
#endif
	if (special & UNSIGNED) {
		if (newsize == 0) {
			if (!nflag)
				(void)printf("%s%s", string, equ);
			(void)printf("%u\n", *(u_int *)buf);
		} else {
			if (!qflag) {
				if (!nflag)
					(void)printf("%s: %u -> ", string,
					    *(u_int *)buf);
				(void)printf("%u\n", *(u_int *)newval);
			}
		}
		return;
	}
	if (special & RNDSTATS) {
		struct rndstats *rndstats = (struct rndstats *)buf;
		int i;

		if (!nflag)
			(void)printf("%s%s", string, equ);
		(void)printf(
		"%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
		    (unsigned long long)rndstats->rnd_total,
		    (unsigned long long)rndstats->rnd_used,
		    (unsigned long long)rndstats->rnd_reads,
		    (unsigned long long)rndstats->arc4_reads,
		    (unsigned long long)rndstats->arc4_nstirs,
		    (unsigned long long)rndstats->arc4_stirs,
		    (unsigned long long)rndstats->rnd_pad[0],
		    (unsigned long long)rndstats->rnd_pad[1],
		    (unsigned long long)rndstats->rnd_pad[2],
		    (unsigned long long)rndstats->rnd_pad[3],
		    (unsigned long long)rndstats->rnd_pad[4],
		    (unsigned long long)rndstats->rnd_waits,
		    (unsigned long long)rndstats->rnd_enqs,
		    (unsigned long long)rndstats->rnd_deqs,
		    (unsigned long long)rndstats->rnd_drops,
		    (unsigned long long)rndstats->rnd_drople);
		for (i = 0; i < sizeof(rndstats->rnd_ed)/sizeof(rndstats->rnd_ed[0]);
		    i++)
			(void)printf(" %llu", (unsigned long long)rndstats->rnd_ed[i]);
		for (i = 0; i < sizeof(rndstats->rnd_sc)/sizeof(rndstats->rnd_sc[0]);
		    i++)
			(void)printf(" %llu", (unsigned long long)rndstats->rnd_sc[i]);
		for (i = 0; i < sizeof(rndstats->rnd_sb)/sizeof(rndstats->rnd_sb[0]);
		    i++)
			(void)printf(" %llu", (unsigned long long)rndstats->rnd_sb[i]);
		printf("\n");
		return;
	}
	if (special & BADDYNAMIC) {
		in_port_t port, lastport;
		u_int32_t *baddynamic = (u_int32_t *)buf;

		if (!qflag) {
			if (!nflag)
				(void)printf("%s%s", string,
				    newsize ? ": " : equ);
			lastport = 0;
			for (port = IPPORT_RESERVED/2; port < IPPORT_RESERVED;
			    port++)
				if (DP_ISSET(baddynamic, port)) {
					(void)printf("%s%hd",
					    lastport ? "," : "", port);
					lastport = port;
				}
			if (newsize != 0) {
				if (!nflag)
					fputs(" -> ", stdout);
				baddynamic = (u_int32_t *)newval;
				lastport = 0;
				for (port = IPPORT_RESERVED/2;
				    port < IPPORT_RESERVED; port++)
					if (DP_ISSET(baddynamic, port)) {
						(void)printf("%s%hd",
						    lastport ? "," : "", port);
						lastport = port;
					}
			}
			(void)putchar('\n');
		}
		return;
	}
	if (special & LONGARRAY) {
		long *la = (long *)buf;
		if (!nflag)
			printf("%s%s", string, equ);
		while (lal--)
			printf("%ld%s", *la++, lal? ",":"");
		putchar('\n');
		return;
	}
	if (special & SENSORS) {
		struct sensor *s = (struct sensor *)buf;

		if (size > 0 && (s->flags & SENSOR_FINVALID) == 0) {
			if (!nflag)
				printf("%s%s", string, equ);
			printf("%s, %s, ", s->device, s->desc);
			switch (s->type) {
			case SENSOR_TEMP:
				printf("temp, %.2f degC / %.2f degF",
				    (s->value - 273150000) / 1000000.0,
				    (s->value - 273150000) / 1000000.0 * 9 / 5 +
				    32);
				break;
			case SENSOR_FANRPM:
				printf("fanrpm, %lld RPM", s->value);
				break;
			case SENSOR_VOLTS_DC:
				printf("volts_dc, %.2f V",
				    s->value / 1000000.0);
				break;
			default:
				printf("unknown");
			}
			printf("\n");
		}
		return;
	}

	switch (type) {
	case CTLTYPE_INT:
		if (newsize == 0) {
			if (!nflag)
				(void)printf("%s%s", string, equ);
			(void)printf("%d\n", *(int *)buf);
		} else {
			if (!qflag) {
				if (!nflag)
					(void)printf("%s: %d -> ", string,
					    *(int *)buf);
				(void)printf("%d\n", *(int *)newval);
			}
		}
		return;

	case CTLTYPE_STRING:
		if (newval == NULL) {
			if (!nflag)
				(void)printf("%s%s", string, equ);
			(void)puts(buf);
		} else {
			if (!qflag) {
				if (!nflag)
					(void)printf("%s: %s -> ", string, buf);
				(void)puts((char *)newval);
			}
		}
		return;

	case CTLTYPE_QUAD:
		if (newsize == 0) {
			long long tmp = *(quad_t *)buf;

			if (!nflag)
				(void)printf("%s%s", string, equ);
			(void)printf("%lld\n", tmp);
		} else {
			long long tmp = *(quad_t *)buf;

			if (!qflag) {
				if (!nflag)
					(void)printf("%s: %lld -> ",
					    string, tmp);
				tmp = *(quad_t *)newval;
				(void)printf("%qd\n", tmp);
			}
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
parse_baddynamic(int mib[], size_t len, char *string, void **newvalp,
    size_t *newsizep, int flags, int nflag)
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
debuginit(void)
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

struct ctlname vfsgennames[] = CTL_VFSGENCTL_NAMES;
struct ctlname ffsname[] = FFS_NAMES;
struct ctlname nfsname[] = FS_NFS_NAMES;
struct list *vfsvars;
int *vfs_typenums;

/*
 * Initialize the set of filesystem names
 */
void
vfsinit(void)
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
	maxtypenum++;	/* + generic */
	if ((vfs_typenums = malloc(maxtypenum * sizeof(int))) == NULL)
		return;
	memset(vfs_typenums, 0, maxtypenum * sizeof(int));
	if ((vfsvars = malloc(maxtypenum * sizeof(*vfsvars))) == NULL) {
		free(vfs_typenums);
		return;
	}
	memset(vfsvars, 0, maxtypenum * sizeof(*vfsvars));
	if ((vfsname = malloc(maxtypenum * sizeof(*vfsname))) == NULL) {
		free(vfs_typenums);
		free(vfsvars);
		return;
	}
	memset(vfsname, 0, maxtypenum * sizeof(*vfsname));
	mib[2] = VFS_CONF;
	buflen = sizeof vfc;
	for (loc = lastused, cnt = 1; cnt < maxtypenum; cnt++) {
		mib[3] = cnt - 1;
		if (sysctl(mib, 4, &vfc, &buflen, (void *)0, (size_t)0) < 0) {
			if (errno == EOPNOTSUPP)
				continue;
			warn("vfsinit");
			free(vfsname);
			return;
		}
		if (!strcmp(vfc.vfc_name, MOUNT_FFS)) {
			vfsvars[cnt].list = ffsname;
			vfsvars[cnt].size = FFS_MAXID;
		}
		if (!strcmp(vfc.vfc_name, MOUNT_NFS)) {
			vfsvars[cnt].list = nfsname;
			vfsvars[cnt].size = NFS_MAXID;
		}
		vfs_typenums[cnt] = vfc.vfc_typenum;
		strlcat(&names[loc], vfc.vfc_name, sizeof names - loc);
		vfsname[cnt].ctl_name = &names[loc];
		vfsname[cnt].ctl_type = CTLTYPE_NODE;
		size = strlen(vfc.vfc_name) + 1;
		loc += size;
	}
	lastused = loc;

	vfsname[0].ctl_name = "mounts";
	vfsname[0].ctl_type = CTLTYPE_NODE;
	vfsvars[0].list = vfsname + 1;
	vfsvars[0].size = maxtypenum - 1;

	secondlevel[CTL_VFS].list = vfsname;
	secondlevel[CTL_VFS].size = maxtypenum;
	return;
}

int
sysctl_vfsgen(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	int indx;
	size_t size;
	struct vfsconf vfc;

	if (*bufpp == NULL) {
		listall(string, vfsvars);
		return (-1);
	}

	if ((indx = findname(string, "third", bufpp, vfsvars)) == -1)
		return (-1);

	mib[1] = VFS_GENERIC;
	mib[2] = VFS_CONF;
	mib[3] = indx;
	size = sizeof vfc;
	if (sysctl(mib, 4, &vfc, &size, (void *)0, (size_t)0) < 0) {
		if (errno != EOPNOTSUPP)
			warn("vfs print");
		return -1;
	}
	if (flags == 0 && vfc.vfc_refcount == 0)
		return -1;
	if (!nflag)
		fprintf(stdout, "%s has %d mounted instance%s\n",
		    string, vfc.vfc_refcount,
		    vfc.vfc_refcount != 1 ? "s" : "");
	else
		fprintf(stdout, "%d\n", vfc.vfc_refcount);

	return -1;
}

int
sysctl_vfs(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	struct list *lp = &vfsvars[mib[1]];
	int indx;

	if (lp->list == NULL) {
		if (flags)
			warnx("No variables defined for file system %s", string);
		return (-1);
	}
	if (*bufpp == NULL) {
		listall(string, lp);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, lp)) == -1)
		return (-1);

	mib[1] = vfs_typenums[mib[1]];
	mib[2] = indx;
	*typep = lp->list[indx].ctl_type;
	return (3);
}

struct ctlname posixname[] = CTL_FS_POSIX_NAMES;
struct list fslist = { posixname, FS_POSIX_MAXID };

/*
 * handle file system requests
 */
int
sysctl_fs(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	int indx;

	if (*bufpp == NULL) {
		listall(string, &fslist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &fslist)) == -1)
		return (-1);
	mib[2] = indx;
	*typep = fslist.list[indx].ctl_type;
	return (3);
}

#ifdef CPU_BIOS
struct ctlname biosname[] = CTL_BIOS_NAMES;
struct list bioslist = { biosname, BIOS_MAXID };

/*
 * handle BIOS requests
 */
int
sysctl_bios(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	char *name;
	int indx;

	if (*bufpp == NULL) {
		listall(string, &bioslist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &bioslist)) == -1)
		return (-1);
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
			return (-1);
		}
		if ((name = strsep(bufpp, ".")) == NULL) {
			warnx("%s: incomplete specification", string);
			return (-1);
		}
		mib[3] = atoi(name);
		*typep = CTLTYPE_STRUCT;
		return (4);
	} else {
		*typep = bioslist.list[indx].ctl_type;
		return (3);
	}
}
#endif

struct ctlname swpencname[] = CTL_SWPENC_NAMES;
struct list swpenclist = { swpencname, SWPENC_MAXID };

/*
 * handle swap encrypt requests
 */
int
sysctl_swpenc(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	int indx;

	if (*bufpp == NULL) {
		listall(string, &swpenclist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &swpenclist)) == -1)
		return (-1);
	mib[2] = indx;
	*typep = swpenclist.list[indx].ctl_type;
	return (3);
}

struct ctlname inetname[] = CTL_IPPROTO_NAMES;
struct ctlname ipname[] = IPCTL_NAMES;
struct ctlname icmpname[] = ICMPCTL_NAMES;
struct ctlname ipipname[] = IPIPCTL_NAMES;
struct ctlname tcpname[] = TCPCTL_NAMES;
struct ctlname udpname[] = UDPCTL_NAMES;
struct ctlname espname[] = ESPCTL_NAMES;
struct ctlname ahname[] = AHCTL_NAMES;
struct ctlname etheripname[] = ETHERIPCTL_NAMES;
struct ctlname grename[] = GRECTL_NAMES;
struct ctlname mobileipname[] = MOBILEIPCTL_NAMES;
struct ctlname ipcompname[] = IPCOMPCTL_NAMES;
struct ctlname carpname[] = CARPCTL_NAMES;
struct ctlname bpfname[] = CTL_NET_BPF_NAMES;
struct list inetlist = { inetname, IPPROTO_MAXID };
struct list inetvars[] = {
	{ ipname, IPCTL_MAXID },	/* ip */
	{ icmpname, ICMPCTL_MAXID },	/* icmp */
	{ 0, 0 },			/* igmp */
	{ 0, 0 },			/* ggmp */
	{ ipipname, IPIPCTL_MAXID },	/* ipencap */
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
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ grename, GRECTL_MAXID }, /* GRE */
	{ 0, 0 },
	{ 0, 0 },
	{ espname, ESPCTL_MAXID },	/* esp */
	{ ahname, AHCTL_MAXID },	/* ah */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ mobileipname, MOBILEIPCTL_MAXID }, /* mobileip */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ etheripname, ETHERIPCTL_MAXID },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ ipcompname, IPCOMPCTL_MAXID },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ carpname, CARPCTL_MAXID },
};
struct list bpflist = { bpfname, NET_BPF_MAXID };

struct list kernmalloclist = { kernmallocname, KERN_MALLOC_MAXID };
struct list forkstatlist = { forkstatname, KERN_FORKSTAT_MAXID };
struct list nchstatslist = { nchstatsname, KERN_NCHSTATS_MAXID };
struct list ttylist = { ttysname, KERN_TTY_MAXID };
struct list semlist = { semname, KERN_SEMINFO_MAXID };
struct list shmlist = { shmname, KERN_SHMINFO_MAXID };
struct list watchdoglist = { watchdogname, KERN_WATCHDOG_MAXID };
struct list tclist = { tcname, KERN_TIMECOUNTER_MAXID };

/*
 * handle vfs namei cache statistics
 */
int
sysctl_nchstats(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	static struct nchstats nch;
	int indx;
	size_t size;
	static int keepvalue = 0;

	if (*bufpp == NULL) {
		bzero(&nch, sizeof(struct nchstats));
		listall(string, &nchstatslist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &nchstatslist)) == -1)
		return (-1);
	mib[2] = indx;
	if (*bufpp != NULL) {
		warnx("fourth level name in %s is invalid", string);
		return (-1);
	}
	if (keepvalue == 0) {
		size = sizeof(struct nchstats);
		if (sysctl(mib, 2, &nch, &size, NULL, 0) < 0)
			return (-1);
		keepvalue = 1;
	}
	if (!nflag)
		(void)printf("%s%s", string, equ);
	switch (indx) {
	case KERN_NCHSTATS_GOODHITS:
		(void)printf("%ld\n", nch.ncs_goodhits);
		break;
	case KERN_NCHSTATS_NEGHITS:
		(void)printf("%ld\n", nch.ncs_neghits);
		break;
	case KERN_NCHSTATS_BADHITS:
		(void)printf("%ld\n", nch.ncs_badhits);
		break;
	case KERN_NCHSTATS_FALSEHITS:
		(void)printf("%ld\n", nch.ncs_falsehits);
		break;
	case KERN_NCHSTATS_MISS:
		(void)printf("%ld\n", nch.ncs_miss);
		break;
	case KERN_NCHSTATS_LONG:
		(void)printf("%ld\n", nch.ncs_long);
		break;
	case KERN_NCHSTATS_PASS2:
		(void)printf("%ld\n", nch.ncs_pass2);
		break;
	case KERN_NCHSTATS_2PASSES:
		(void)printf("%ld\n", nch.ncs_2passes);
		break;
	}
	return (-1);
}

/*
 * handle tty statistics
 */
int
sysctl_tty(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	int indx;

	if (*bufpp == NULL) {
		listall(string, &ttylist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &ttylist)) == -1)
		return (-1);
	mib[2] = indx;

	if ((*typep = ttylist.list[indx].ctl_type) == CTLTYPE_STRUCT) {
		if (flags)
			warnx("use pstat -t to view %s information",
			    string);
		return (-1);
	}
	return (3);
}

/*
 * handle fork statistics
 */
int
sysctl_forkstat(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	static struct forkstat fks;
	static int keepvalue = 0;
	int indx;
	size_t size;

	if (*bufpp == NULL) {
		bzero(&fks, sizeof(struct forkstat));
		listall(string, &forkstatlist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &forkstatlist)) == -1)
		return (-1);
	if (*bufpp != NULL) {
		warnx("fourth level name in %s is invalid", string);
		return (-1);
	}
	if (keepvalue == 0) {
		size = sizeof(struct forkstat);
		if (sysctl(mib, 2, &fks, &size, NULL, 0) < 0)
			return (-1);
		keepvalue = 1;
	}
	if (!nflag)
		(void)printf("%s%s", string, equ);
	switch (indx)	{
	case KERN_FORKSTAT_FORK:
		(void)printf("%d\n", fks.cntfork);
		break;
	case KERN_FORKSTAT_VFORK:
		(void)printf("%d\n", fks.cntvfork);
		break;
	case KERN_FORKSTAT_RFORK:
		(void)printf("%d\n", fks.cntrfork);
		break;
	case KERN_FORKSTAT_KTHREAD:
		(void)printf("%d\n", fks.cntkthread);
		break;
	case KERN_FORKSTAT_SIZFORK:
		(void)printf("%d\n", fks.sizfork);
		break;
	case KERN_FORKSTAT_SIZVFORK:
		(void)printf("%d\n", fks.sizvfork);
		break;
	case KERN_FORKSTAT_SIZRFORK:
		(void)printf("%d\n", fks.sizrfork);
		break;
	case KERN_FORKSTAT_SIZKTHREAD:
		(void)printf("%d\n", fks.sizkthread);
		break;
	}
	return (-1);
}

/*
 * handle malloc statistics
 */
int
sysctl_malloc(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	int indx, stor, i;
	char *name, bufp[BUFSIZ], *buf, *ptr;
	struct list lp;
	size_t size;

	if (*bufpp == NULL) {
		listall(string, &kernmalloclist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &kernmalloclist)) == -1)
		return (-1);
	mib[2] = indx;
	if (mib[2] == KERN_MALLOC_BUCKET) {
		if ((name = strsep(bufpp, ".")) == NULL) {
			size = BUFSIZ;
			stor = mib[2];
			mib[2] = KERN_MALLOC_BUCKETS;
			buf = bufp;
			if (sysctl(mib, 3, buf, &size, NULL, 0) < 0)
				return (-1);
			mib[2] = stor;
			for (stor = 0, i = 0; i < size; i++)
				if (buf[i] == ',')
					stor++;
			lp.list = calloc(stor + 2, sizeof(struct ctlname));
			if (lp.list == NULL)
				return (-1);
			lp.size = stor + 2;
			for (i = 1;
			    (lp.list[i].ctl_name = strsep(&buf, ",")) != NULL;
			    i++) {
				lp.list[i].ctl_type = CTLTYPE_STRUCT;
			}
			lp.list[i].ctl_name = buf;
			lp.list[i].ctl_type = CTLTYPE_STRUCT;
			listall(string, &lp);
			free(lp.list);
			return (-1);
		}
		mib[3] = atoi(name);
		return (4);
	} else if (mib[2] == KERN_MALLOC_BUCKETS) {
		*typep = CTLTYPE_STRING;
		return (3);
	} else if (mib[2] == KERN_MALLOC_KMEMSTATS) {
		size = BUFSIZ;
		stor = mib[2];
		mib[2] = KERN_MALLOC_KMEMNAMES;
		buf = bufp;
		if (sysctl(mib, 3, buf, &size, NULL, 0) < 0)
			return (-1);
		mib[2] = stor;
		if ((name = strsep(bufpp, ".")) == NULL) {
			for (stor = 0, i = 0; i < size; i++)
				if (buf[i] == ',')
					stor++;
			lp.list = calloc(stor + 2, sizeof(struct ctlname));
			if (lp.list == NULL)
				return (-1);
			lp.size = stor + 2;
			for (i = 1;
			    (lp.list[i].ctl_name = strsep(&buf, ",")) != NULL; i++) {
				if (lp.list[i].ctl_name[0] == '\0') {
					i--;
					continue;
				}
				lp.list[i].ctl_type = CTLTYPE_STRUCT;
			}
			lp.list[i].ctl_name = buf;
			lp.list[i].ctl_type = CTLTYPE_STRUCT;
			listall(string, &lp);
			free(lp.list);
			return (-1);
		}
		ptr = strstr(buf, name);
 tryagain:
		if (ptr == NULL) {
			warnx("fourth level name %s in %s is invalid", name,
			    string);
			return (-1);
		}
		if ((*(ptr + strlen(name)) != ',') &&
		    (*(ptr + strlen(name)) != '\0')) {
			ptr = strstr(ptr + 1, name); /* retry */
			goto tryagain;
		}
		if ((ptr != buf) && (*(ptr - 1) != ',')) {
			ptr = strstr(ptr + 1, name); /* retry */
			goto tryagain;
		}
		for (i = 0, stor = 0; buf + i < ptr; i++)
			if (buf[i] == ',')
				stor++;
		mib[3] = stor;
		return (4);
	} else if (mib[2] == KERN_MALLOC_KMEMNAMES) {
		*typep = CTLTYPE_STRING;
		return (3);
	}
	return (-1);
}

#ifdef CPU_CHIPSET
/*
 * handle machdep.chipset requests
 */
struct ctlname chipsetname[] = CTL_CHIPSET_NAMES;
struct list chipsetlist = { chipsetname, CPU_CHIPSET_MAXID };

int
sysctl_chipset(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	int indx, bwx;
	static void *q;
	size_t len;
	char *p;

	if (*bufpp == NULL) {
		listall(string, &chipsetlist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &chipsetlist)) == -1)
		return (-1);
	mib[2] = indx;
	if (!nflag)
		printf("%s%s", string, equ);
	switch(mib[2]) {
	case CPU_CHIPSET_MEM:
	case CPU_CHIPSET_DENSE:
	case CPU_CHIPSET_PORTS:
	case CPU_CHIPSET_HAE_MASK:
		len = sizeof(void *);
		if (sysctl(mib, 3, &q, &len, NULL, 0) < 0)
			return (-1);
		printf("%p\n", q);
		break;
	case CPU_CHIPSET_BWX:
		len = sizeof(int);
		if (sysctl(mib, 3, &bwx, &len, NULL, 0) < 0)
			return (-1);
		printf("%d\n", bwx);
		break;
	case CPU_CHIPSET_TYPE:
		if (sysctl(mib, 3, NULL, &len, NULL, 0) < 0)
			return (-1);
		p = malloc(len + 1);
		if (p == NULL)
			return (-1);
		if (sysctl(mib, 3, p, &len, NULL, 0) < 0)
			return (-1);
		p[len] = '\0';
		printf("%s\n", p);
		break;
	}
	return (-1);
}
#endif
/*
 * handle internet requests
 */
int
sysctl_inet(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	struct list *lp;
	int indx;

	if (*bufpp == NULL) {
		listall(string, &inetlist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &inetlist)) == -1)
		return (-1);
	mib[2] = indx;
	if (indx < IPPROTO_MAXID && inetvars[indx].list != NULL)
		lp = &inetvars[indx];
	else if (!flags)
		return (-1);
	else {
		warnx("%s: no variables defined for this protocol", string);
		return (-1);
	}
	if (*bufpp == NULL) {
		listall(string, lp);
		return (-1);
	}
	if ((indx = findname(string, "fourth", bufpp, lp)) == -1)
		return (-1);
	mib[3] = indx;
	*typep = lp->list[indx].ctl_type;
	return (4);
}

#ifdef INET6
struct ctlname inet6name[] = CTL_IPV6PROTO_NAMES;
struct ctlname ip6name[] = IPV6CTL_NAMES;
struct ctlname icmp6name[] = ICMPV6CTL_NAMES;
struct ctlname pim6name[] = PIM6CTL_NAMES;
struct list inet6list = { inet6name, IPV6PROTO_MAXID };
struct list inet6vars[] = {
/*0*/	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
/*10*/	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
/*20*/	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
/*30*/	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
/*40*/	{ 0, 0 },
	{ ip6name, IPV6CTL_MAXID },	/* ipv6 */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
/*50*/	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ icmp6name, ICMPV6CTL_MAXID },	/* icmp6 */
	{ 0, 0 },
/*60*/	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
/*70*/	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
/*80*/	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
/*90*/	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
/*100*/	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ pim6name, PIM6CTL_MAXID },	/* pim6 */
};

/*
 * handle internet6 requests
 */
int
sysctl_inet6(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	struct list *lp;
	int indx;

	if (*bufpp == NULL) {
		listall(string, &inet6list);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &inet6list)) == -1)
		return (-1);
	mib[2] = indx;
	if (indx < IPV6PROTO_MAXID && inet6vars[indx].list != NULL)
		lp = &inet6vars[indx];
	else if (!flags)
		return (-1);
	else {
		warnx("%s: no variables defined for this protocol", string);
		return (-1);
	}
	if (*bufpp == NULL) {
		listall(string, lp);
		return (-1);
	}
	if ((indx = findname(string, "fourth", bufpp, lp)) == -1)
		return (-1);
	mib[3] = indx;
	*typep = lp->list[indx].ctl_type;
	return (4);
}
#endif

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
sysctl_ipx(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	struct list *lp;
	int indx;

	if (*bufpp == NULL) {
		listall(string, &ipxlist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &ipxlist)) == -1)
		return (-1);
	mib[2] = indx;
	if (indx <= IPXPROTO_SPX && ipxvars[indx].list != NULL)
		lp = &ipxvars[indx];
	else if (!flags)
		return (-1);
	else {
		warnx("%s: no variables defined for this protocol", string);
		return (-1);
	}
	if (*bufpp == NULL) {
		listall(string, lp);
		return (-1);
	}
	if ((indx = findname(string, "fourth", bufpp, lp)) == -1)
		return (-1);
	mib[3] = indx;
	*typep = lp->list[indx].ctl_type;
	return (4);
}

/* handle bpf requests */
int
sysctl_bpf(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	int indx;

	if (*bufpp == NULL) {
		listall(string, &bpflist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &bpflist)) == -1)
		return (-1);
	mib[2] = indx;
	*typep = CTLTYPE_INT;
	return (3);
}

/*
 * Handle SysV semaphore info requests
 */
int
sysctl_seminfo(string, bufpp, mib, flags, typep)
	char *string;
	char **bufpp;
	int mib[];
	int flags;
	int *typep;
{
	int indx;

	if (*bufpp == NULL) {
		listall(string, &semlist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &semlist)) == -1)
		return (-1);
	mib[2] = indx;
	*typep = CTLTYPE_INT;
	return (3);
}

/*
 * Handle SysV shared memory info requests
 */
int
sysctl_shminfo(string, bufpp, mib, flags, typep)
	char *string;
	char **bufpp;
	int mib[];
	int flags;
	int *typep;
{
	int indx;

	if (*bufpp == NULL) {
		listall(string, &shmlist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &shmlist)) == -1)
		return (-1);
	mib[2] = indx;
	*typep = CTLTYPE_INT;
	return (3);
}

/*
 * Handle watchdog support
 */
int
sysctl_watchdog(char *string, char **bufpp, int mib[], int flags,
    int *typep)
{
	int indx;

	if (*bufpp == NULL) {
		listall(string, &watchdoglist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &watchdoglist)) == -1)
		return (-1);
	mib[2] = indx;
	*typep = watchdoglist.list[indx].ctl_type;
	return (3);
}

/*
 * Handle timecounter support
 */
int
sysctl_tc(char *string, char **bufpp, int mib[], int flags,
    int *typep)
{
	int indx;

	if (*bufpp == NULL) {
		listall(string, &tclist);
		return (-1);
	}
	if ((indx = findname(string, "third", bufpp, &tclist)) == -1)
		return (-1);
	mib[2] = indx;
	*typep = tclist.list[indx].ctl_type;
	return (3);
}

/*
 * Handle hardware monitoring sensors support
 */
int
sysctl_sensors(char *string, char **bufpp, int mib[], int flags, int *typep)
{
	char *name;
	int indx;

	if (*bufpp == NULL) {
		char name[BUFSIZ];

		/* scan all sensors */
		for (indx = 0; indx < 256; indx++) {
			snprintf(name, sizeof(name), "%s.%u", string, indx);
			parse(name, 0);
		}
		return (-1);
	}
	if ((name = strsep(bufpp, ".")) == NULL) {
		warnx("%s: incomplete specification", string);
		return (-1);
	}
	mib[2] = atoi(name);
	*typep = CTLTYPE_STRUCT;
	return (3);
}

struct emulname {
	char *name;
	int index;
} *emul_names;
int	emul_num, nemuls;
int	emul_init(void);

int
sysctl_emul(char *string, char *newval, int flags)
{
	int mib[4], enabled, i, old, print, found = 0;
	char *head, *target;
	size_t len;

	if (emul_init() == -1) {
		warnx("emul_init: out of memory");
		return (1);
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_EMUL;
	mib[3] = KERN_EMUL_ENABLED;
	head = "kern.emul.";

	if (aflag || strcmp(string, "kern.emul") == 0) {
		if (newval) {
			warnx("%s: specification is incomplete", string);
			return (1);
		}
		if (nflag)
			printf("%d\n", nemuls);
		else
			printf("%snemuls%s%d\n", head, equ, nemuls);
		for (i = 0; i < emul_num; i++) {
			if (emul_names[i].name == NULL)
				break;
			if (i > 0 && strcmp(emul_names[i].name,
			    emul_names[i-1].name) == 0)
				continue;
			mib[2] = emul_names[i].index;
			len = sizeof(int);
			if (sysctl(mib, 4, &enabled, &len, NULL, 0) == -1) {
				warn("%s", string);
				continue;
			}
			if (nflag)
				printf("%d\n", enabled);
			else
				printf("%s%s%s%d\n", head, emul_names[i].name,
				    equ, enabled);
		}
		return (0);
	}
	/* User specified a third level name */
	target = strrchr(string, '.');
	target++;
	if (strcmp(target, "nemuls") == 0) {
		if (newval) {
			warnx("Operation not permitted");
			return (1);
		}
		if (nflag)
			printf("%d\n", nemuls);
		else
			printf("%snemuls = %d\n", head, nemuls);
		return (0);
	}
	print = 1;
	for (i = 0; i < emul_num; i++) {
		if (!emul_names[i].name || (strcmp(target, emul_names[i].name)))
			continue;
		found = 1;
		mib[2] = emul_names[i].index;
		len = sizeof(int);
		if (newval) {
			enabled = atoi(newval);
			if (sysctl(mib, 4, &old, &len, &enabled, len) == -1) {
				warn("%s", string);
				print = 0;
				continue;
			}
			if (print) {
				if (nflag)
					printf("%d\n", enabled);
				else
					printf("%s%s: %d -> %d\n", head,
					    target, old, enabled);
			}
		} else {
			if (sysctl(mib, 4, &enabled, &len, NULL, 0) == -1) {
				warn("%s", string);
				continue;
			}
			if (print) {
				if (nflag)
					printf("%d\n", enabled);
				else
					printf("%s%s = %d\n", head, target,
					    enabled);
			}
		}
		print = 0;
	}
	if (!found)
		warnx("third level name %s in kern.emul is invalid",
		    string);
	return (0);


}

int
emulcmp(const void *m, const void *n)
{
	const struct emulname *a = m, *b = n;

	if (!a || !a->name)
		return 1;
	if (!b || !b->name)
		return -1;
	return (strcmp(a->name, b->name));
}

int
emul_init(void)
{
	static int done;
	char string[16];
	int mib[4], i;
	size_t len;

	if (done)
		return (0);
	done = 1;

	mib[0] = CTL_KERN;
	mib[1] = KERN_EMUL;
	mib[2] = KERN_EMUL_NUM;
	len = sizeof(int);
	if (sysctl(mib, 3, &emul_num, &len, NULL, 0) == -1)
		return (-1);

	emul_names = calloc(emul_num, sizeof(*emul_names));
	if (emul_names == NULL)
		return (-1);

	nemuls = emul_num;
	for (i = 0; i < emul_num; i++) {
		emul_names[i].index = mib[2] = i + 1;
		mib[3] = KERN_EMUL_NAME;
		len = sizeof(string);
		if (sysctl(mib, 4, string, &len, NULL, 0) == -1)
			continue;
		if (strcmp(string, "native") == 0)
			continue;
		emul_names[i].name = strdup(string);
		if (emul_names[i].name == NULL) {
			free(emul_names);
			return (-1);
		}
	}
	qsort(emul_names, nemuls, sizeof(*emul_names), emulcmp);
	for (i = 0; i < emul_num; i++) {
		if (!emul_names[i].name || (i > 0 &&
		    strcmp(emul_names[i].name, emul_names[i - 1].name) == 0)) {

			nemuls--;
		}
	}
	return (0);
}

/*
 * Scan a list of names searching for a particular name.
 */
int
findname(char *string, char *level, char **bufp, struct list *namelist)
{
	char *name;
	int i;

	if (namelist->list == 0 || (name = strsep(bufp, ".")) == NULL) {
		warnx("%s: incomplete specification", string);
		return (-1);
	}
	for (i = 0; i < namelist->size; i++)
		if (namelist->list[i].ctl_name != NULL &&
		    strcmp(name, namelist->list[i].ctl_name) == 0)
			break;
	if (i == namelist->size) {
		warnx("%s level name %s in %s is invalid", level, name, string);
		return (-1);
	}
	return (i);
}

void
usage(void)
{

	(void)fprintf(stderr, "usage:\t%s\n\t%s\n\t%s\n",
	    "sysctl [-n] variable ...", "sysctl [-nqw] variable=value ...",
	    "sysctl [-n] -Aa");
	exit(1);
}
