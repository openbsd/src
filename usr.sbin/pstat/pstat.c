/*	$OpenBSD: pstat.c,v 1.22 1999/12/05 08:49:18 art Exp $	*/
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
"@(#) Copyright (c) 1980, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
from: static char sccsid[] = "@(#)pstat.c	8.9 (Berkeley) 2/16/94";
#else
static char *rcsid = "$OpenBSD: pstat.c,v 1.22 1999/12/05 08:49:18 art Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/map.h>
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
#define	V_MOUNTLIST	0
	{ "_mountlist" },	/* address of head of mount list. */
#define V_NUMV		1
	{ "_numvnodes" },
#define	FNL_NFILE	2
	{"_nfiles"},
#define FNL_MAXFILE	3
	{"_maxfiles"},
#define TTY_NTTY	4
	{"_tty_count"},
#define TTY_TTYLIST	5
	{"_ttylist"},
#define NLMANDATORY TTY_TTYLIST	/* names up to here are mandatory */
	{ "" }
};

int	usenumflag;
int	totalflag;
int	kflag;
char	*nlistf	= NULL;
char	*memf	= NULL;
kvm_t	*kd;

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

void	filemode __P((void));
int	getfiles __P((char **, int *));
struct mount *
	getmnt __P((struct mount *));
struct e_vnode *
	kinfo_vnodes __P((int *));
struct e_vnode *
	loadvnodes __P((int *));
void	mount_print __P((struct mount *));
void	nfs_header __P((void));
int	nfs_print __P((struct vnode *));
void	swapmode __P((void));
void	ttymode __P((void));
void	ttyprt __P((struct tty *));
void	ufs_header __P((void));
int	ufs_print __P((struct vnode *));
void	ext2fs_header __P((void));
int	ext2fs_print __P((struct vnode *));
void	usage __P((void));
void	vnode_header __P((void));
void	vnode_print __P((struct vnode *, struct vnode *));
void	vnodemode __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	int ch, ret;
	int fileflag, swapflag, ttyflag, vnodeflag;
	char buf[_POSIX2_LINE_MAX];

	fileflag = swapflag = ttyflag = vnodeflag = 0;
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
	if (nlistf != NULL || memf != NULL) {
		(void)setegid(getgid());
		(void)setgid(getgid());
	}

	if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf)) == 0)
		errx(1, "kvm_openfiles: %s", buf);

	(void)setegid(getgid());
	(void)setgid(getgid());

	if ((ret = kvm_nlist(kd, nl)) == -1)
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
	exit (0);
}

struct e_vnode {
	struct vnode *avnode;
	struct vnode vnode;
};

void
vnodemode()
{
	register struct e_vnode *e_vnodebase, *endvnode, *evp;
	register struct vnode *vp;
	register struct mount *maddr, *mp = NULL;
	int numvnodes;

	e_vnodebase = loadvnodes(&numvnodes);
	if (totalflag) {
		(void)printf("%7d vnodes\n", numvnodes);
		return;
	}
	endvnode = e_vnodebase + numvnodes;
	(void)printf("%d active vnodes\n", numvnodes);


#define ST	mp->mnt_stat
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
			if (!strncmp(ST.f_fstypename, MOUNT_FFS, MFSNAMELEN) ||
			    !strncmp(ST.f_fstypename, MOUNT_MFS, MFSNAMELEN)) {
				ufs_header();
			} else if (!strncmp(ST.f_fstypename, MOUNT_NFS,
			    MFSNAMELEN)) {
				nfs_header();
			} else if (!strncmp(ST.f_fstypename, MOUNT_EXT2FS,
			    MFSNAMELEN)) {
				ext2fs_header();
			}
			(void)printf("\n");
		}
		vnode_print(evp->avnode, vp);
		if (!strncmp(ST.f_fstypename, MOUNT_FFS, MFSNAMELEN) ||
		    !strncmp(ST.f_fstypename, MOUNT_MFS, MFSNAMELEN)) {
			ufs_print(vp);
		} else if (!strncmp(ST.f_fstypename, MOUNT_NFS, MFSNAMELEN)) {
			nfs_print(vp);
		} else if (!strncmp(ST.f_fstypename, MOUNT_EXT2FS,
		    MFSNAMELEN)) {
			ext2fs_print(vp);
		}
		(void)printf("\n");
	}
	free(e_vnodebase);
}

void
vnode_header()
{
	(void)printf("ADDR     TYP VFLAG  USE HOLD");
}

void
vnode_print(avnode, vp)
	struct vnode *avnode;
	struct vnode *vp;
{
	char *type, flags[16]; 
	char *fp = flags;
	register int flag;

	/*
	 * set type
	 */
	switch(vp->v_type) {
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
	if (flag & VBWAIT)
		*fp++ = 'B';
	if (flag & VALIASED)
		*fp++ = 'A';
	if (flag & VDIROP)
		*fp++ = 'D';
	if (flag == 0)
		*fp++ = '-';
	*fp = '\0';
	(void)printf("%8lx %s %5s %4d %4ld",
	    (long)avnode, type, flags, vp->v_usecount, vp->v_holdcnt);
}

void
ufs_header() 
{
	(void)printf(" FILEID IFLAG RDEV|SZ");
}

int
ufs_print(vp) 
	struct vnode *vp;
{
	register int flag;
	struct inode inode, *ip = &inode;
	char flagbuf[16], *flags = flagbuf;
	char *name;
	mode_t type;

	KGETRET(VTOI(vp), &inode, sizeof(struct inode), "vnode's inode");
	flag = ip->i_flag;
#if 0
	if (flag & IN_LOCKED)
		*flags++ = 'L';
	if (flag & IN_WANTED)
		*flags++ = 'W';
	if (flag & IN_LWAIT)
		*flags++ = 'Z';
#endif
	if (flag & IN_RENAME)
		*flags++ = 'R';
	if (flag & IN_UPDATE)
		*flags++ = 'U';
	if (flag & IN_ACCESS)
		*flags++ = 'A';
	if (flag & IN_CHANGE)
		*flags++ = 'C';
	if (flag & IN_MODIFIED)
		*flags++ = 'M';
	if (flag & IN_SHLOCK)
		*flags++ = 'S';
	if (flag & IN_EXLOCK)
		*flags++ = 'E';
	if (flag == 0)
		*flags++ = '-';
	*flags = '\0';

	(void)printf(" %6d %5s", ip->i_number, flagbuf);
	type = ip->i_ffs_mode & S_IFMT;
	if (S_ISCHR(ip->i_ffs_mode) || S_ISBLK(ip->i_ffs_mode))
		if (usenumflag ||
		    ((name = devname(ip->i_ffs_rdev, type)) == NULL))
			(void)printf("   %2d,%-2d", 
			    major(ip->i_ffs_rdev), minor(ip->i_ffs_rdev));
		else
			(void)printf(" %7s", name);
	else
		(void)printf(" %7qd", ip->i_ffs_size);
	return (0);
}

void
ext2fs_header() 
{
	(void)printf(" FILEID IFLAG SZ");
}

int
ext2fs_print(vp) 
	struct vnode *vp;
{
	register int flag;
	struct inode inode, *ip = &inode;
	char flagbuf[16], *flags = flagbuf;

	KGETRET(VTOI(vp), &inode, sizeof(struct inode), "vnode's inode");
	flag = ip->i_flag;
#if 0
	if (flag & IN_LOCKED)
		*flags++ = 'L';
	if (flag & IN_WANTED)
		*flags++ = 'W';
	if (flag & IN_LWAIT)
		*flags++ = 'Z';
#endif
	if (flag & IN_RENAME)
		*flags++ = 'R';
	if (flag & IN_UPDATE)
		*flags++ = 'U';
	if (flag & IN_ACCESS)
		*flags++ = 'A';
	if (flag & IN_CHANGE)
		*flags++ = 'C';
	if (flag & IN_MODIFIED)
		*flags++ = 'M';
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
nfs_header() 
{
	(void)printf(" FILEID NFLAG RDEV|SZ");
}

int
nfs_print(vp) 
	struct vnode *vp;
{
	struct nfsnode nfsnode, *np = &nfsnode;
	char flagbuf[16], *flags = flagbuf;
	register int flag;
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
	if (flag & NQNFSNONCACHE)
		*flags++ = 'X';
	if (flag & NQNFSWRITE)
		*flags++ = 'O';
	if (flag & NQNFSEVICTED)
		*flags++ = 'G';
	if (flag == 0)
		*flags++ = '-';
	*flags = '\0';

#define VT	np->n_vattr
	(void)printf(" %6ld %5s", VT.va_fileid, flagbuf);
	type = VT.va_mode & S_IFMT;
	if (S_ISCHR(VT.va_mode) || S_ISBLK(VT.va_mode))
		if (usenumflag || ((name = devname(VT.va_rdev, type)) == NULL))
			(void)printf("   %2d,%-2d", 
			    major(VT.va_rdev), minor(VT.va_rdev));
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
getmnt(maddr)
	struct mount *maddr;
{
	static struct mtab {
		struct mtab *next;
		struct mount *maddr;
		struct mount mount;
	} *mhead = NULL;
	register struct mtab *mt;

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
mount_print(mp)
	struct mount *mp;
{
	register int flags;

#define ST	mp->mnt_stat
	(void)printf("*** MOUNT ");
	(void)printf("%.*s %s on %s", MFSNAMELEN, ST.f_fstypename,
	    ST.f_mntfromname, ST.f_mntonname);
	if ((flags = mp->mnt_flag)) {
		char *comma = "(";

		putchar(' ');
		/* user visable flags */
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
		if (flags & MNT_UNION) {
			(void)printf("%sunion", comma);
			flags &= ~MNT_UNION;
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
		/* filesystem control flags */
		if (flags & MNT_UPDATE) {
			(void)printf("%supdate", comma);
			flags &= ~MNT_UPDATE;
			comma = ",";
		}
		if (flags & MNT_MLOCK) {
			(void)printf("%slock", comma);
			flags &= ~MNT_MLOCK;
			comma = ",";
		}
		if (flags & MNT_MWAIT) {
			(void)printf("%swait", comma);
			flags &= ~MNT_MWAIT;
			comma = ",";
		}
		if (flags & MNT_MPBUSY) {
			(void)printf("%sbusy", comma);
			flags &= ~MNT_MPBUSY;
			comma = ",";
		}
		if (flags & MNT_MPWANT) {
			(void)printf("%swant", comma);
			flags &= ~MNT_MPWANT;
			comma = ",";
		}
		if (flags & MNT_UNMOUNT) {
			(void)printf("%sunmount", comma);
			flags &= ~MNT_UNMOUNT;
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
#undef ST
}

struct e_vnode *
loadvnodes(avnodes)
	int *avnodes;
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
kinfo_vnodes(avnodes)
	int *avnodes;
{
	struct mntlist mountlist;
	struct mount *mp, mount;
	struct vnode *vp, vnode;
	char *vbuf, *evbuf, *bp;
	int num, numvnodes;

#define VPTRSZ  sizeof(struct vnode *)
#define VNODESZ sizeof(struct vnode)

	KGET(V_NUMV, numvnodes);
	if ((vbuf = malloc((numvnodes + 20) * (VPTRSZ + VNODESZ))) == NULL)
		err(1, "malloc: vnode buffer");
	bp = vbuf;
	evbuf = vbuf + (numvnodes + 20) * (VPTRSZ + VNODESZ);
	KGET(V_MOUNTLIST, mountlist);
	for (num = 0, mp = mountlist.cqh_first; ; mp = mount.mnt_list.cqe_next) {
		KGET2(mp, &mount, sizeof(mount), "mount entry");
		for (vp = mount.mnt_vnodelist.lh_first;
		    vp != NULL; vp = vnode.v_mntvnodes.le_next) {
			KGET2(vp, &vnode, sizeof(vnode), "vnode");
			if ((bp + VPTRSZ + VNODESZ) > evbuf)
				/* XXX - should realloc */
				errx(1, "no more room for vnodes");
			memmove(bp, &vp, VPTRSZ);
			bp += VPTRSZ;
			memmove(bp, &vnode, VNODESZ);
			bp += VNODESZ;
			num++;
		}
		if (mp == mountlist.cqh_last)
			break;
	}
	*avnodes = num;
	return ((struct e_vnode *)vbuf);
}
	
char hdr[]="   LINE RAW  CAN  OUT  HWT LWT    COL STATE      SESS  PGID DISC\n";

void
ttymode()
{
	int ntty;
	struct ttylist_head tty_head;
	struct tty *tp, tty;

	KGET(TTY_NTTY, ntty);
	(void)printf("%d terminal device%s\n", ntty, ntty == 1 ? "" : "s");
	KGET(TTY_TTYLIST, tty_head);
	(void)printf(hdr);
	for (tp = tty_head.tqh_first; tp; tp = tty.tty_link.tqe_next) {
		KGET2(tp, &tty, sizeof tty, "tty struct");
		ttyprt(&tty);
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
	{ 0,	       '\0'},
};

void
ttyprt(tp)
	register struct tty *tp;
{
	register int i, j;
	pid_t pgid;
	char *name, state[20];

	if (usenumflag || (name = devname(tp->t_dev, S_IFCHR)) == NULL)
		(void)printf("%2d,%-3d   ", major(tp->t_dev), minor(tp->t_dev));
	else
		(void)printf("%7s ", name);
	(void)printf("%3d %4d ", tp->t_rawq.c_cc, tp->t_canq.c_cc);
	(void)printf("%4d %4d %3d %6d ", tp->t_outq.c_cc, 
		tp->t_hiwat, tp->t_lowat, tp->t_column);
	for (i = j = 0; ttystates[i].flag; i++)
		if (tp->t_state&ttystates[i].flag)
			state[j++] = ttystates[i].val;
	if (j == 0)
		state[j++] = '-';
	state[j] = '\0';
	(void)printf("%-6s %8lx", state, (u_long)tp->t_session & ~KERNBASE);
	pgid = 0;
	if (tp->t_pgrp != NULL)
		KGET2(&tp->t_pgrp->pg_id, &pgid, sizeof(pid_t), "pgid");
	(void)printf("%6d ", pgid);
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
filemode()
{
	struct file fp, *ffp;
	struct file *addr;
	char *buf, flagbuf[16], *fbp;
	int len, maxfile, nfile;
	static char *dtypes[] = { "???", "inode", "socket" };

	KGET(FNL_MAXFILE, maxfile);
	if (totalflag) {
		KGET(FNL_NFILE, nfile);
		(void)printf("%3d/%3d files\n", nfile, maxfile);
		return;
	}
	if (getfiles(&buf, &len) == -1)
		return;
	/*
	 * Getfiles returns in malloc'd memory a pointer to the first file
	 * structure, and then an array of file structs (whose addresses are
	 * derivable from the previous entry).
	 */
	addr = ((struct filelist *)buf)->lh_first;
	ffp = (struct file *)(buf + sizeof(struct filelist));
	nfile = (len - sizeof(struct filelist)) / sizeof(struct file);
	
	(void)printf("%d/%d open files\n", nfile, maxfile);
	(void)printf("     LOC TYPE       FLG  CNT  MSG      DATA  OFFSET\n");
	for (; (char *)ffp < buf + len; addr = ffp->f_list.le_next, ffp++) {
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
#ifdef FSHLOCK	/* currently gone */
		if (fp.f_flag & FSHLOCK)
			*fbp++ = 'S';
		if (fp.f_flag & FEXLOCK)
			*fbp++ = 'X';
#endif
		if (fp.f_flag & FASYNC)
			*fbp++ = 'I';
		*fbp = '\0';
		(void)printf("%6s  %3ld", flagbuf, fp.f_count);
		(void)printf("  %3ld", fp.f_msgcount);
		(void)printf("  %8.1lx", (long)fp.f_data);

		if (fp.f_offset < 0)
			(void)printf("  %qx\n", fp.f_offset);
		else
			(void)printf("  %qd\n", fp.f_offset);
	}
	free(buf);
}

int
getfiles(abuf, alen)
	char **abuf;
	int *alen;
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
swapmode()
{
	char *header;
	int hlen = 10, nswap, rnswap;
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
	rnswap = swapctl(SWAP_STATS, swdev, nswap);

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

	free(swdev);
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: pstat [-Tfknstv] [-M core] [-N system]\n");
	exit(1);
}
