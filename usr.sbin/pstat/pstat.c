/*	$OpenBSD: pstat.c,v 1.56 2005/12/28 20:48:18 pedro Exp $	*/
/*	$NetBSD: pstat.c,v 1.27 1996/10/23 22:50:06 cgd Exp $	*/

/*-
 * Copyright (c) 1980, 1991, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1980, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
from: static char sccsid[] = "@(#)pstat.c	8.9 (Berkeley) 2/16/94";
#else
static char *rcsid = "$OpenBSD: pstat.c,v 1.56 2005/12/28 20:48:18 pedro Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/ucred.h>
#define _KERNEL
#include <sys/file.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#define NFS
#include <sys/mount.h>
#undef NFS
#undef _KERNEL
#include <sys/stat.h>
#include <nfs/nfsproto.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsnode.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/swap.h>

#include <sys/sysctl.h>

#include <err.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct nlist nl[] = {
#define	FNL_NFILE	0		/* sysctl */
	{"_nfiles"},
#define FNL_MAXFILE	1		/* sysctl */
	{"_maxfiles"},
#define TTY_NTTY	2		/* sysctl */
	{"_tty_count"},
#define V_NUMV		3		/* sysctl */
	{ "_numvnodes" },
#define TTY_TTYLIST	4		/* sysctl */
	{"_ttylist"},
#define	V_MOUNTLIST	5		/* no sysctl */
	{ "_mountlist" },
	{ "" }
};

int	usenumflag;
int	totalflag;
int	kflag;
char	*nlistf	= NULL;
char	*memf	= NULL;
kvm_t	*kd = NULL;

#define	SVAR(var) __STRING(var)	/* to force expansion */
#define	KGET(idx, var)							\
	KGET1(idx, &var, sizeof(var), SVAR(var))
#define	KGET1(idx, p, s, msg)						\
	KGET2(nl[idx].n_value, p, s, msg)
#define	KGET2(addr, p, s, msg)						\
	if (kvm_read(kd, (u_long)(addr), p, s) != s)			\
		warnx("cannot read %s: %s", msg, kvm_geterr(kd))
#define	KGETRET(addr, p, s, msg)					\
	if (kvm_read(kd, (u_long)(addr), p, s) != s) {			\
		warnx("cannot read %s: %s", msg, kvm_geterr(kd));	\
		return (0);						\
	}

void	filemode(void);
int	getfiles(char **, size_t *);
struct mount *
	getmnt(struct mount *);
struct e_vnode *
	kinfo_vnodes(int *);
struct e_vnode *
	loadvnodes(int *);
void	mount_print(struct mount *);
void	nfs_header(void);
int	nfs_print(struct vnode *);
void	swapmode(void);
void	ttymode(void);
void	ttyprt(struct itty *);
void	ufs_header(void);
int	ufs_print(struct vnode *);
void	ext2fs_header(void);
int	ext2fs_print(struct vnode *);
void	usage(void);
void	vnode_header(void);
void	vnode_print(struct vnode *, struct vnode *);
void	vnodemode(void);

int
main(int argc, char *argv[])
{
	int fileflag = 0, swapflag = 0, ttyflag = 0, vnodeflag = 0;
	char buf[_POSIX2_LINE_MAX];
	int ch;
	extern char *optarg;
	extern int optind;
	gid_t gid;

	while ((ch = getopt(argc, argv, "TM:N:fiknstv")) != -1)
		switch (ch) {
		case 'f':
			fileflag = 1;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			usenumflag = 1;
			break;
		case 's':
			swapflag = 1;
			break;
		case 'T':
			totalflag = 1;
			break;
		case 't':
			ttyflag = 1;
			break;
		case 'k':
			kflag = 1;
			break;
		case 'v':
		case 'i':		/* Backward compatibility. */
			vnodeflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	gid = getgid();
	if (nlistf != NULL || memf != NULL)
		if (setresgid(gid, gid, gid) == -1)
			err(1, "setresgid");

	if (vnodeflag)
		if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf)) == 0)
			errx(1, "kvm_openfiles: %s", buf);

	if (nlistf == NULL && memf == NULL)
		if (setresgid(gid, gid, gid) == -1)
			err(1, "setresgid");

	if (vnodeflag)
		if (kvm_nlist(kd, nl) == -1)
			errx(1, "kvm_nlist: %s", kvm_geterr(kd));

	if (!(fileflag | vnodeflag | ttyflag | swapflag | totalflag))
		usage();
	if (fileflag || totalflag)
		filemode();
	if (vnodeflag || totalflag)
		vnodemode();
	if (ttyflag)
		ttymode();
	if (swapflag || totalflag)
		swapmode();
	exit(0);
}

void
vnodemode(void)
{
	struct e_vnode *e_vnodebase, *endvnode, *evp;
	struct vnode *vp;
	struct mount *maddr, *mp = NULL;
	int numvnodes;

	e_vnodebase = loadvnodes(&numvnodes);
	if (totalflag) {
		(void)printf("%7d vnodes\n", numvnodes);
		return;
	}
	endvnode = e_vnodebase + numvnodes;
	(void)printf("%d active vnodes\n", numvnodes);

	maddr = NULL;
	for (evp = e_vnodebase; evp < endvnode; evp++) {
		vp = &evp->vnode;
		if (vp->v_mount != maddr) {
			/*
			 * New filesystem
			 */
			if ((mp = getmnt(vp->v_mount)) == NULL)
				continue;
			maddr = vp->v_mount;
			mount_print(mp);
			vnode_header();
			if (!strncmp(mp->mnt_stat.f_fstypename, MOUNT_FFS, MFSNAMELEN) ||
			    !strncmp(mp->mnt_stat.f_fstypename, MOUNT_MFS, MFSNAMELEN)) {
				ufs_header();
			} else if (!strncmp(mp->mnt_stat.f_fstypename, MOUNT_NFS,
			    MFSNAMELEN)) {
				nfs_header();
			} else if (!strncmp(mp->mnt_stat.f_fstypename, MOUNT_EXT2FS,
			    MFSNAMELEN)) {
				ext2fs_header();
			}
			(void)printf("\n");
		}
		vnode_print(evp->vptr, vp);
		if (!strncmp(mp->mnt_stat.f_fstypename, MOUNT_FFS, MFSNAMELEN) ||
		    !strncmp(mp->mnt_stat.f_fstypename, MOUNT_MFS, MFSNAMELEN)) {
			ufs_print(vp);
		} else if (!strncmp(mp->mnt_stat.f_fstypename, MOUNT_NFS, MFSNAMELEN)) {
			nfs_print(vp);
		} else if (!strncmp(mp->mnt_stat.f_fstypename, MOUNT_EXT2FS,
		    MFSNAMELEN)) {
			ext2fs_print(vp);
		}
		(void)printf("\n");
	}
	free(e_vnodebase);
}

void
vnode_header(void)
{
	(void)printf("ADDR     TYP VFLAG  USE HOLD");
}

void
vnode_print(struct vnode *avnode, struct vnode *vp)
{
	char *type, flags[16];
	char *fp = flags;
	int flag;

	/*
	 * set type
	 */
	switch (vp->v_type) {
	case VNON:
		type = "non"; break;
	case VREG:
		type = "reg"; break;
	case VDIR:
		type = "dir"; break;
	case VBLK:
		type = "blk"; break;
	case VCHR:
		type = "chr"; break;
	case VLNK:
		type = "lnk"; break;
	case VSOCK:
		type = "soc"; break;
	case VFIFO:
		type = "fif"; break;
	case VBAD:
		type = "bad"; break;
	default:
		type = "unk"; break;
	}
	/*
	 * gather flags
	 */
	flag = vp->v_flag;
	if (flag & VROOT)
		*fp++ = 'R';
	if (flag & VTEXT)
		*fp++ = 'T';
	if (flag & VSYSTEM)
		*fp++ = 'S';
	if (flag & VISTTY)
		*fp++ = 'I';
	if (flag & VXLOCK)
		*fp++ = 'L';
	if (flag & VXWANT)
		*fp++ = 'W';
	if (vp->v_bioflag & VBIOWAIT)
		*fp++ = 'B';
	if (flag & VALIASED)
		*fp++ = 'A';
	if (vp->v_bioflag & VBIOONFREELIST)
		*fp++ = 'F';
	if (flag & VLOCKSWORK)
		*fp++ = 'l';
	if (vp->v_bioflag & VBIOONSYNCLIST)
		*fp++ = 's';
	if (flag == 0)
		*fp++ = '-';
	*fp = '\0';
	(void)printf("%8lx %s %5s %4d %4u",
	    (long)avnode, type, flags, vp->v_usecount, vp->v_holdcnt);
}

void
ufs_header(void)
{
	(void)printf(" FILEID IFLAG RDEV|SZ");
}

int
ufs_print(struct vnode *vp)
{
	int flag;
	struct inode inode, *ip = &inode;
	struct ufs1_dinode di1;
	char flagbuf[16], *flags = flagbuf;
	char *name;
	mode_t type;

	KGETRET(VTOI(vp), &inode, sizeof(struct inode), "vnode's inode");
	KGETRET(inode.i_din1, &di1, sizeof(struct ufs1_dinode),
	    "vnode's dinode");

	inode.i_din1 = &di1;
	flag = ip->i_flag;
#if 0
	if (flag & IN_LOCKED)
		*flags++ = 'L';
	if (flag & IN_WANTED)
		*flags++ = 'W';
	if (flag & IN_LWAIT)
		*flags++ = 'Z';
#endif
	if (flag & IN_ACCESS)
		*flags++ = 'A';
	if (flag & IN_CHANGE)
		*flags++ = 'C';
	if (flag & IN_UPDATE)
		*flags++ = 'U';
	if (flag & IN_MODIFIED)
		*flags++ = 'M';
	if (flag & IN_RENAME)
		*flags++ = 'R';
	if (flag & IN_SHLOCK)
		*flags++ = 'S';
	if (flag & IN_EXLOCK)
		*flags++ = 'E';
	if (flag == 0)
		*flags++ = '-';
	*flags = '\0';

	(void)printf(" %6d %5s", ip->i_number, flagbuf);
	type = ip->i_ffs1_mode & S_IFMT;
	if (S_ISCHR(ip->i_ffs1_mode) || S_ISBLK(ip->i_ffs1_mode))
		if (usenumflag ||
		    ((name = devname(ip->i_ffs1_rdev, type)) == NULL))
			(void)printf("   %2d,%-2d",
			    major(ip->i_ffs1_rdev), minor(ip->i_ffs1_rdev));
		else
			(void)printf(" %7s", name);
	else
		(void)printf(" %7qd", ip->i_ffs1_size);
	return (0);
}

void
ext2fs_header(void)
{
	(void)printf(" FILEID IFLAG SZ");
}

int
ext2fs_print(struct vnode *vp)
{
	int flag;
	struct inode inode, *ip = &inode;
	struct ext2fs_dinode di;
	char flagbuf[16], *flags = flagbuf;

	KGETRET(VTOI(vp), &inode, sizeof(struct inode), "vnode's inode");
	KGETRET(inode.i_e2din, &di, sizeof(struct ext2fs_dinode),
	    "vnode's dinode");

	inode.i_e2din = &di;
	flag = ip->i_flag;

#if 0
	if (flag & IN_LOCKED)
		*flags++ = 'L';
	if (flag & IN_WANTED)
		*flags++ = 'W';
	if (flag & IN_LWAIT)
		*flags++ = 'Z';
#endif
	if (flag & IN_ACCESS)
		*flags++ = 'A';
	if (flag & IN_CHANGE)
		*flags++ = 'C';
	if (flag & IN_UPDATE)
		*flags++ = 'U';
	if (flag & IN_MODIFIED)
		*flags++ = 'M';
	if (flag & IN_RENAME)
		*flags++ = 'R';
	if (flag & IN_SHLOCK)
		*flags++ = 'S';
	if (flag & IN_EXLOCK)
		*flags++ = 'E';
	if (flag == 0)
		*flags++ = '-';
	*flags = '\0';

	(void)printf(" %6d %5s %2d", ip->i_number, flagbuf, ip->i_e2fs_size);
	return (0);
}

void
nfs_header(void)
{
	(void)printf(" FILEID NFLAG RDEV|SZ");
}

int
nfs_print(struct vnode *vp)
{
	struct nfsnode nfsnode, *np = &nfsnode;
	char flagbuf[16], *flags = flagbuf;
	int flag;
	char *name;
	mode_t type;

	KGETRET(VTONFS(vp), &nfsnode, sizeof(nfsnode), "vnode's nfsnode");
	flag = np->n_flag;
	if (flag & NFLUSHWANT)
		*flags++ = 'W';
	if (flag & NFLUSHINPROG)
		*flags++ = 'P';
	if (flag & NMODIFIED)
		*flags++ = 'M';
	if (flag & NWRITEERR)
		*flags++ = 'E';
	if (flag & NACC)
		*flags++ = 'A';
	if (flag & NUPD)
		*flags++ = 'U';
	if (flag & NCHG)
		*flags++ = 'C';
	if (flag == 0)
		*flags++ = '-';
	*flags = '\0';

	(void)printf(" %6ld %5s", np->n_vattr.va_fileid, flagbuf);
	type = np->n_vattr.va_mode & S_IFMT;
	if (S_ISCHR(np->n_vattr.va_mode) || S_ISBLK(np->n_vattr.va_mode))
		if (usenumflag ||
		    ((name = devname(np->n_vattr.va_rdev, type)) == NULL))
			(void)printf("   %2d,%-2d", major(np->n_vattr.va_rdev),
			    minor(np->n_vattr.va_rdev));
		else
			(void)printf(" %7s", name);
	else
		(void)printf(" %7qd", np->n_size);
	return (0);
}

/*
 * Given a pointer to a mount structure in kernel space,
 * read it in and return a usable pointer to it.
 */
struct mount *
getmnt(struct mount *maddr)
{
	static struct mtab {
		struct mtab *next;
		struct mount *maddr;
		struct mount mount;
	} *mhead = NULL;
	struct mtab *mt;

	for (mt = mhead; mt != NULL; mt = mt->next)
		if (maddr == mt->maddr)
			return (&mt->mount);
	if ((mt = malloc(sizeof(struct mtab))) == NULL)
		err(1, "malloc: mount table");
	KGETRET(maddr, &mt->mount, sizeof(struct mount), "mount table");
	mt->maddr = maddr;
	mt->next = mhead;
	mhead = mt;
	return (&mt->mount);
}

void
mount_print(struct mount *mp)
{
	int flags;

	(void)printf("*** MOUNT ");
	(void)printf("%.*s %s on %s", MFSNAMELEN,
	    mp->mnt_stat.f_fstypename, mp->mnt_stat.f_mntfromname,
	    mp->mnt_stat.f_mntonname);
	if ((flags = mp->mnt_flag)) {
		char *comma = "(";

		putchar(' ');
		/* user visible flags */
		if (flags & MNT_RDONLY) {
			(void)printf("%srdonly", comma);
			flags &= ~MNT_RDONLY;
			comma = ",";
		}
		if (flags & MNT_SYNCHRONOUS) {
			(void)printf("%ssynchronous", comma);
			flags &= ~MNT_SYNCHRONOUS;
			comma = ",";
		}
		if (flags & MNT_NOEXEC) {
			(void)printf("%snoexec", comma);
			flags &= ~MNT_NOEXEC;
			comma = ",";
		}
		if (flags & MNT_NOSUID) {
			(void)printf("%snosuid", comma);
			flags &= ~MNT_NOSUID;
			comma = ",";
		}
		if (flags & MNT_NODEV) {
			(void)printf("%snodev", comma);
			flags &= ~MNT_NODEV;
			comma = ",";
		}
		if (flags & MNT_ASYNC) {
			(void)printf("%sasync", comma);
			flags &= ~MNT_ASYNC;
			comma = ",";
		}
		if (flags & MNT_EXRDONLY) {
			(void)printf("%sexrdonly", comma);
			flags &= ~MNT_EXRDONLY;
			comma = ",";
		}
		if (flags & MNT_EXPORTED) {
			(void)printf("%sexport", comma);
			flags &= ~MNT_EXPORTED;
			comma = ",";
		}
		if (flags & MNT_DEFEXPORTED) {
			(void)printf("%sdefdexported", comma);
			flags &= ~MNT_DEFEXPORTED;
			comma = ",";
		}
		if (flags & MNT_EXPORTANON) {
			(void)printf("%sexportanon", comma);
			flags &= ~MNT_EXPORTANON;
			comma = ",";
		}
		if (flags & MNT_EXKERB) {
			(void)printf("%sexkerb", comma);
			flags &= ~MNT_EXKERB;
			comma = ",";
		}
		if (flags & MNT_LOCAL) {
			(void)printf("%slocal", comma);
			flags &= ~MNT_LOCAL;
			comma = ",";
		}
		if (flags & MNT_QUOTA) {
			(void)printf("%squota", comma);
			flags &= ~MNT_QUOTA;
			comma = ",";
		}
		if (flags & MNT_ROOTFS) {
			(void)printf("%srootfs", comma);
			flags &= ~MNT_ROOTFS;
			comma = ",";
		}
		if (flags & MNT_NOATIME) {
			(void)printf("%snoatime", comma);
			flags &= ~MNT_NOATIME;
			comma = ",";
		}
		/* filesystem control flags */
		if (flags & MNT_UPDATE) {
			(void)printf("%supdate", comma);
			flags &= ~MNT_UPDATE;
			comma = ",";
		}
		if (flags & MNT_DELEXPORT) {
			(void)printf("%sdelexport", comma);
			flags &= ~MNT_DELEXPORT;
			comma = ",";
		}
		if (flags & MNT_RELOAD) {
			(void)printf("%sreload", comma);
			flags &= ~MNT_RELOAD;
			comma = ",";
		}
		if (flags & MNT_FORCE) {
			(void)printf("%sforce", comma);
			flags &= ~MNT_FORCE;
			comma = ",";
		}
		if (flags & MNT_WANTRDWR) {
			(void)printf("%swantrdwr", comma);
			flags &= ~MNT_WANTRDWR;
			comma = ",";
		}
		if (flags & MNT_SOFTDEP) {
			(void)printf("%ssoftdep", comma);
			flags &= ~MNT_SOFTDEP;
			comma = ",";
		}
		if (flags)
			(void)printf("%sunknown_flags:%x", comma, flags);
		(void)printf(")");
	}
	(void)printf("\n");
}

struct e_vnode *
loadvnodes(int *avnodes)
{
	int mib[2];
	size_t copysize;
	struct e_vnode *vnodebase;

	if (memf != NULL) {
		/*
		 * do it by hand
		 */
		return (kinfo_vnodes(avnodes));
	}
	mib[0] = CTL_KERN;
	mib[1] = KERN_VNODE;
	if (sysctl(mib, 2, NULL, &copysize, NULL, 0) == -1)
		err(1, "sysctl: KERN_VNODE");
	if ((vnodebase = malloc(copysize)) == NULL)
		err(1, "malloc: vnode table");
	if (sysctl(mib, 2, vnodebase, &copysize, NULL, 0) == -1)
		err(1, "sysctl: KERN_VNODE");
	if (copysize % sizeof(struct e_vnode))
		errx(1, "vnode size mismatch");
	*avnodes = copysize / sizeof(struct e_vnode);

	return (vnodebase);
}

/*
 * simulate what a running kernel does in in kinfo_vnode
 */
struct e_vnode *
kinfo_vnodes(int *avnodes)
{
	struct mntlist mountlist;
	struct mount *mp, mount;
	struct vnode *vp, vnode;
	char *vbuf, *evbuf, *bp;
	int mib[2], numvnodes;
	size_t num;

	if (kd == 0) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_NUMVNODES;
		num = sizeof(numvnodes);
		if (sysctl(mib, 2, &numvnodes, &num, NULL, 0) < 0)
			err(1, "sysctl(KERN_NUMVNODES) failed");
	} else
		KGET(V_NUMV, numvnodes);
	if ((vbuf = malloc((numvnodes + 20) *
	    (sizeof(struct vnode *) + sizeof(struct vnode)))) == NULL)
		err(1, "malloc: vnode buffer");
	bp = vbuf;
	evbuf = vbuf + (numvnodes + 20) *
	    (sizeof(struct vnode *) + sizeof(struct vnode));
	KGET(V_MOUNTLIST, mountlist);
	for (num = 0, mp = CIRCLEQ_FIRST(&mountlist); ;
	    mp = CIRCLEQ_NEXT(&mount, mnt_list)) {
		KGET2(mp, &mount, sizeof(mount), "mount entry");
		for (vp = LIST_FIRST(&mount.mnt_vnodelist);
		    vp != NULL; vp = LIST_NEXT(&vnode, v_mntvnodes)) {
			KGET2(vp, &vnode, sizeof(vnode), "vnode");
			if ((bp + sizeof(struct vnode *) +
			    sizeof(struct vnode)) > evbuf)
				/* XXX - should realloc */
				errx(1, "no more room for vnodes");
			memmove(bp, &vp, sizeof(struct vnode *));
			bp += sizeof(struct vnode *);
			memmove(bp, &vnode, sizeof(struct vnode));
			bp += sizeof(struct vnode);
			num++;
		}
		if (mp == CIRCLEQ_LAST(&mountlist))
			break;
	}
	*avnodes = num;
	return ((struct e_vnode *)vbuf);
}

const char hdr[] =
"   LINE RAW  CAN  OUT  HWT LWT    COL STATE      SESS  PGID DISC\n";

void
tty2itty(struct tty *tp, struct itty *itp)
{
	itp->t_dev = tp->t_dev;
	itp->t_rawq_c_cc = tp->t_rawq.c_cc;
	itp->t_canq_c_cc = tp->t_canq.c_cc;
	itp->t_outq_c_cc = tp->t_outq.c_cc;
	itp->t_hiwat = tp->t_hiwat;
	itp->t_lowat = tp->t_lowat;
	itp->t_column = tp->t_column;
	itp->t_state = tp->t_state;
	itp->t_session = tp->t_session;
	if (tp->t_pgrp != NULL)
		KGET2(&tp->t_pgrp->pg_id, &itp->t_pgrp_pg_id, sizeof(pid_t), "pgid");
	itp->t_line = tp->t_line;
}

void
ttymode(void)
{
	struct ttylist_head tty_head;
	struct tty *tp, tty;
	int mib[3], ntty, i;
	struct itty itty, *itp;
	size_t nlen;

	if (kd == 0) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_TTYCOUNT;
		nlen = sizeof(ntty);
		if (sysctl(mib, 2, &ntty, &nlen, NULL, 0) < 0)
			err(1, "sysctl(KERN_TTYCOUNT) failed");
	} else
		KGET(TTY_NTTY, ntty);
	(void)printf("%d terminal device%s\n", ntty, ntty == 1 ? "" : "s");
	(void)printf("%s", hdr);
	if (kd == 0) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_TTY;
		mib[2] = KERN_TTY_INFO;
		nlen = ntty * sizeof(struct itty);
		if ((itp = malloc(nlen)) == NULL)
			err(1, "malloc");
		if (sysctl(mib, 3, itp, &nlen, NULL, 0) < 0)
			err(1, "sysctl(KERN_TTY_INFO) failed");
		for (i = 0; i < ntty; i++)
			ttyprt(&itp[i]);
		free(itp);
	} else {
		KGET(TTY_TTYLIST, tty_head);
		for (tp = TAILQ_FIRST(&tty_head); tp;
		    tp = TAILQ_NEXT(&tty, tty_link)) {
			KGET2(tp, &tty, sizeof tty, "tty struct");
			tty2itty(&tty, &itty);
			ttyprt(&itty);
		}
	}
}

struct {
	int flag;
	char val;
} ttystates[] = {
	{ TS_WOPEN,	'W'},
	{ TS_ISOPEN,	'O'},
	{ TS_CARR_ON,	'C'},
	{ TS_TIMEOUT,	'T'},
	{ TS_FLUSH,	'F'},
	{ TS_BUSY,	'B'},
	{ TS_ASLEEP,	'A'},
	{ TS_XCLUDE,	'X'},
	{ TS_TTSTOP,	'S'},
	{ TS_TBLOCK,	'K'},
	{ TS_ASYNC,	'Y'},
	{ TS_BKSL,	'D'},
	{ TS_ERASE,	'E'},
	{ TS_LNCH,	'L'},
	{ TS_TYPEN,	'P'},
	{ TS_CNTTB,	'N'},
	{ 0,		'\0'},
};

void
ttyprt(struct itty *tp)
{
	char *name, state[20];
	int i, j;

	if (usenumflag || (name = devname(tp->t_dev, S_IFCHR)) == NULL)
		(void)printf("%2d,%-3d   ", major(tp->t_dev), minor(tp->t_dev));
	else
		(void)printf("%7s ", name);
	(void)printf("%3d %4d ", tp->t_rawq_c_cc, tp->t_canq_c_cc);
	(void)printf("%4d %4d %3d %6d ", tp->t_outq_c_cc,
		tp->t_hiwat, tp->t_lowat, tp->t_column);
	for (i = j = 0; ttystates[i].flag; i++)
		if (tp->t_state&ttystates[i].flag)
			state[j++] = ttystates[i].val;
	if (j == 0)
		state[j++] = '-';
	state[j] = '\0';
	(void)printf("%-6s %8lx", state, (u_long)tp->t_session & ~KERNBASE);
	(void)printf("%6d ", tp->t_pgrp_pg_id);
	switch (tp->t_line) {
	case TTYDISC:
		(void)printf("term\n");
		break;
	case TABLDISC:
		(void)printf("tab\n");
		break;
	case SLIPDISC:
		(void)printf("slip\n");
		break;
	case PPPDISC:
		(void)printf("ppp\n");
		break;
	case STRIPDISC:
		(void)printf("strip\n");
		break;
	default:
		(void)printf("%d\n", tp->t_line);
		break;
	}
}

void
filemode(void)
{
	struct file fp, *ffp, *addr;
	char *buf, flagbuf[16], *fbp;
	static char *dtypes[] = { "???", "inode", "socket" };
	int mib[2], maxfile, nfile;
	size_t len;

	if (kd == 0) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_MAXFILES;
		len = sizeof(maxfile);
		if (sysctl(mib, 2, &maxfile, &len, NULL, 0) < 0)
			err(1, "sysctl(KERN_MAXFILES) failed");
		if (totalflag) {
			mib[0] = CTL_KERN;
			mib[1] = KERN_NFILES;
			len = sizeof(nfile);
			if (sysctl(mib, 2, &nfile, &len, NULL, 0) < 0)
				err(1, "sysctl(KERN_NFILES) failed");
		}
	} else {
		KGET(FNL_MAXFILE, maxfile);
		if (totalflag) {
			KGET(FNL_NFILE, nfile);
			(void)printf("%3d/%3d files\n", nfile, maxfile);
			return;
		}
	}

	if (getfiles(&buf, &len) == -1)
		return;
	/*
	 * Getfiles returns in malloc'd memory a pointer to the first file
	 * structure, and then an array of file structs (whose addresses are
	 * derivable from the previous entry).
	 */
	addr = LIST_FIRST((struct filelist *)buf);
	ffp = (struct file *)(buf + sizeof(struct filelist));
	nfile = (len - sizeof(struct filelist)) / sizeof(struct file);

	(void)printf("%d/%d open files\n", nfile, maxfile);

	(void)printf("%*s TYPE       FLG  CNT  MSG  %*s  OFFSET\n",
	    8, "LOC", 8, "DATA");
	for (; (char *)ffp < buf + len; addr = LIST_NEXT(ffp, f_list), ffp++) {
		memmove(&fp, ffp, sizeof fp);
		if ((unsigned)fp.f_type > DTYPE_SOCKET)
			continue;
		(void)printf("%lx ", (long)addr);
		(void)printf("%-8.8s", dtypes[fp.f_type]);
		fbp = flagbuf;
		if (fp.f_flag & FREAD)
			*fbp++ = 'R';
		if (fp.f_flag & FWRITE)
			*fbp++ = 'W';
		if (fp.f_flag & FAPPEND)
			*fbp++ = 'A';
		if (fp.f_flag & FHASLOCK)
			*fbp++ = 'L';
		if (fp.f_flag & FASYNC)
			*fbp++ = 'I';
		*fbp = '\0';
		(void)printf("%6s  %3ld", flagbuf, fp.f_count);
		(void)printf("  %3ld", fp.f_msgcount);
		(void)printf("  %8.1lx", (long)fp.f_data);

		if (fp.f_offset < 0)
			(void)printf("  %llx\n", (long long)fp.f_offset);
		else
			(void)printf("  %lld\n", (long long)fp.f_offset);
	}
	free(buf);
}

int
getfiles(char **abuf, size_t *alen)
{
	size_t len;
	int mib[2];
	char *buf;

	/*
	 * XXX
	 * Add emulation of KINFO_FILE here.
	 */
	if (memf != NULL)
		errx(1, "files on dead kernel, not implemented");

	mib[0] = CTL_KERN;
	mib[1] = KERN_FILE;
	if (sysctl(mib, 2, NULL, &len, NULL, 0) == -1) {
		warn("sysctl: KERN_FILE");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL)
		err(1, "malloc: KERN_FILE");
	if (sysctl(mib, 2, buf, &len, NULL, 0) == -1) {
		warn("sysctl: KERN_FILE");
		free(buf);
		return (-1);
	}
	*abuf = buf;
	*alen = len;
	return (0);
}

/*
 * swapmode is based on a program called swapinfo written
 * by Kevin Lahey <kml@rokkaku.atl.ga.us>.
 */
void
swapmode(void)
{
	char *header;
	int hlen = 10, nswap;
	int div, i, avail, nfree, npfree, used;
	long blocksize;
	struct swapent *swdev;

	if (kflag) {
		header = "1K-blocks";
		blocksize = 1024;
		hlen = strlen(header);
	} else
		header = getbsize(&hlen, &blocksize);

	nswap = swapctl(SWAP_NSWAP, 0, 0);
	if (nswap == 0) {
		if (!totalflag)
			(void)printf("%-11s %*s %8s %8s %8s  %s\n",
			    "Device", hlen, header,
			    "Used", "Avail", "Capacity", "Priority");
		(void)printf("%-11s %*d %8d %8d %5.0f%%\n",
		    "Total", hlen, 0, 0, 0, 0.0);
		return;
	}
	if ((swdev = malloc(nswap * sizeof(*swdev))) == NULL)
		err(1, "malloc");
	if (swapctl(SWAP_STATS, swdev, nswap) == -1)
		err(1, "swapctl");

	if (!totalflag)
		(void)printf("%-11s %*s %8s %8s %8s  %s\n",
		    "Device", hlen, header,
		    "Used", "Avail", "Capacity", "Priority");

	/* Run through swap list, doing the funky monkey. */
	div = blocksize / DEV_BSIZE;
	avail = nfree = npfree = 0;
	for (i = 0; i < nswap; i++) {
		int xsize, xfree;

		if (!(swdev[i].se_flags & SWF_ENABLE))
			continue;

		if (!totalflag) {
			if (usenumflag)
				(void)printf("%2d,%-2d       %*d ",
				    major(swdev[i].se_dev),
				    minor(swdev[i].se_dev),
				    hlen, swdev[i].se_nblks / div);
			else
				(void)printf("%-11s %*d ", swdev[i].se_path,
				    hlen, swdev[i].se_nblks / div);
		}

		xsize = swdev[i].se_nblks;
		used = swdev[i].se_inuse;
		xfree = xsize - used;
		nfree += (xsize - used);
		npfree++;
		avail += xsize;
		if (totalflag)
			continue;
		(void)printf("%8d %8d %5.0f%%    %d\n",
		    used / div, xfree / div,
		    (double)used / (double)xsize * 100.0,
		    swdev[i].se_priority);
	}
	free(swdev);

	/*
	 * If only one partition has been set up via swapon(8), we don't
	 * need to bother with totals.
	 */
	used = avail - nfree;
	if (totalflag) {
		(void)printf("%dM/%dM swap space\n",
		    used / (1048576 / DEV_BSIZE),
		    avail / (1048576 / DEV_BSIZE));
		return;
	}
	if (npfree > 1) {
		(void)printf("%-11s %*d %8d %8d %5.0f%%\n",
		    "Total", hlen, avail / div, used / div, nfree / div,
		    (double)used / (double)avail * 100.0);
	}
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: pstat [-fknsTtv] [-M core] [-N system]\n");
	exit(1);
}
