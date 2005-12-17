/*	$OpenBSD: fstat.c,v 1.54 2005/12/17 13:56:02 pedro Exp $	*/

/*-
 * Copyright (c) 1988, 1993
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
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)fstat.c	8.1 (Berkeley) 6/6/93";*/
static char *rcsid = "$OpenBSD: fstat.c,v 1.54 2005/12/17 13:56:02 pedro Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/unpcb.h>
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#define	_KERNEL
#include <sys/mount.h>
#include <crypto/cryptodev.h>
#include <dev/systrace.h>
#include <sys/file.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#undef _KERNEL
#define NFS
#include <nfs/nfsproto.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#undef NFS

#include <xfs/xfs_config.h>
#include <xfs/xfs_node.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#include <netdb.h>
#include <arpa/inet.h>

#include <sys/pipe.h>

#include <ctype.h>
#include <errno.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <err.h>
#include "fstat.h"

#define	TEXT	-1
#define	CDIR	-2
#define	RDIR	-3
#define	TRACE	-4

typedef struct devs {
	struct	devs *next;
	long	fsid;
	ino_t	ino;
	char	*name;
} DEVS;
DEVS *devs;

int	fsflg;	/* show files on same filesystem as file(s) argument */
int	pflg;	/* show files open by a particular pid */
int	uflg;	/* show files open by a particular (effective) user */
int	checkfile; /* true if restricting to particular files or filesystems */
int	nflg;	/* (numerical) display f.s. and rdev as dev_t */
int	oflg;	/* display file offset */
int	vflg;	/* display errors in locating kernel data objects etc... */

struct file **ofiles;	/* buffer of pointers to file structures */
int maxfiles;
#define ALLOC_OFILES(d)	\
	if ((d) > maxfiles) { \
		free(ofiles); \
		ofiles = malloc((d) * sizeof(struct file *)); \
		if (ofiles == NULL) \
			err(1, "malloc"); \
		maxfiles = (d); \
	}

/*
 * a kvm_read that returns true if everything is read
 */
#define KVM_READ(kaddr, paddr, len) \
	(kvm_read(kd, (u_long)(kaddr), (void *)(paddr), (len)) == (len))

kvm_t *kd;

int ufs_filestat(struct vnode *, struct filestat *);
int ext2fs_filestat(struct vnode *, struct filestat *);
int isofs_filestat(struct vnode *, struct filestat *);
int msdos_filestat(struct vnode *, struct filestat *);
int nfs_filestat(struct vnode *, struct filestat *);
int xfs_filestat(struct vnode *, struct filestat *);
void dofiles(struct kinfo_proc2 *);
void getinetproto(int);
void socktrans(struct socket *, int);
void usage(void);
void vtrans(struct vnode *, int, int, off_t);
int getfname(char *);
void pipetrans(struct pipe *, int);
void kqueuetrans(struct kqueue *, int);
void cryptotrans(void *, int);
void systracetrans(struct fsystrace *, int);
char *getmnton(struct mount *);
const char *inet6_addrstr(struct in6_addr *);

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	struct passwd *passwd;
	struct kinfo_proc2 *p, *plast;
	int arg, ch, what;
	char *memf, *nlistf;
	char buf[_POSIX2_LINE_MAX];
	int cnt;
	gid_t gid;

	arg = 0;
	what = KERN_PROC_ALL;
	nlistf = memf = NULL;
	oflg = 0;
	while ((ch = getopt(argc, argv, "fnop:u:vN:M:")) != -1)
		switch((char)ch) {
		case 'f':
			fsflg = 1;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			nflg = 1;
			break;
		case 'o':
			oflg = 1;
			break;
		case 'p':
			if (pflg++)
				usage();
			if (!isdigit(*optarg)) {
				warnx( "-p requires a process id");
				usage();
			}
			what = KERN_PROC_PID;
			arg = atoi(optarg);
			break;
		case 'u':
			if (uflg++)
				usage();
			if (!(passwd = getpwnam(optarg)))
				errx(1, "%s: unknown uid", optarg);
			what = KERN_PROC_UID;
			arg = passwd->pw_uid;
			break;
		case 'v':
			vflg = 1;
			break;
		default:
			usage();
		}

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	gid = getgid();
	if (nlistf != NULL || memf != NULL)
		if (setresgid(gid, gid, gid) == -1)
			err(1, "setresgid");

	if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf)) == NULL)
		errx(1, "%s", buf);

	if (nlistf == NULL && memf == NULL)
		if (setresgid(gid, gid, gid) == -1)
			err(1, "setresgid");

	if (*(argv += optind)) {
		for (; *argv; ++argv) {
			if (getfname(*argv))
				checkfile = 1;
		}
		if (!checkfile)	/* file(s) specified, but none accessible */
			exit(1);
	}

	ALLOC_OFILES(256);	/* reserve space for file pointers */

	if (fsflg && !checkfile) {
		/* -f with no files means use wd */
		if (getfname(".") == 0)
			exit(1);
		checkfile = 1;
	}

	if ((p = kvm_getproc2(kd, what, arg, sizeof(*p), &cnt)) == NULL)
		errx(1, "%s", kvm_geterr(kd));
	if (nflg)
		printf("%s",
"USER     CMD          PID   FD  DEV      INUM       MODE R/W    DV|SZ");
	else
		printf("%s",
"USER     CMD          PID   FD MOUNT        INUM MODE       R/W    DV|SZ");
	if (oflg)
		printf("%s", ":OFFSET  ");
	if (checkfile && fsflg == 0)
		printf(" NAME\n");
	else
		putchar('\n');

	for (plast = &p[cnt]; p < plast; ++p) {
		if (p->p_stat == SZOMB)
			continue;
		dofiles(p);
	}
	exit(0);
}

char	*Uname, *Comm;
pid_t	Pid;

#define PREFIX(i) do { \
	printf("%-8.8s %-10s %5ld", Uname, Comm, (long)Pid); \
	switch(i) { \
	case TEXT: \
		printf(" text"); \
		break; \
	case CDIR: \
		printf("   wd"); \
		break; \
	case RDIR: \
		printf(" root"); \
		break; \
	case TRACE: \
		printf("   tr"); \
		break; \
	default: \
		printf(" %4d", i); \
		break; \
	} \
} while (0)

/*
 * print open files attributed to this process
 */
void
dofiles(struct kinfo_proc2 *kp)
{
	int i;
	struct file file;
	struct filedesc0 filed0;
#define	filed	filed0.fd_fd

	Uname = user_from_uid(kp->p_uid, 0);
	Pid = kp->p_pid;
	Comm = kp->p_comm;

	if (kp->p_fd == 0)
		return;
	if (!KVM_READ(kp->p_fd, &filed0, sizeof (filed0))) {
		dprintf("can't read filedesc at %p for pid %ld",
		    (void *)(u_long)kp->p_fd, (long)Pid);
		return;
	}
	if (filed.fd_nfiles < 0 || filed.fd_lastfile >= filed.fd_nfiles ||
	    filed.fd_freefile > filed.fd_lastfile + 1) {
		dprintf("filedesc corrupted at %p for pid %ld",
		    (void *)(u_long)kp->p_fd, (long)Pid);
		return;
	}
	/*
	 * root directory vnode, if one
	 */
	if (filed.fd_rdir)
		vtrans(filed.fd_rdir, RDIR, FREAD, 0);
	/*
	 * current working directory vnode
	 */
	vtrans(filed.fd_cdir, CDIR, FREAD, 0);
	/*
	 * ktrace vnode, if one
	 */
	if (kp->p_tracep)
		vtrans((struct vnode *)(u_long)kp->p_tracep, TRACE, FREAD|FWRITE, 0);
	/*
	 * open files
	 */
#define FPSIZE	(sizeof (struct file *))
	ALLOC_OFILES(filed.fd_lastfile+1);
	if (filed.fd_nfiles > NDFILE) {
		if (!KVM_READ(filed.fd_ofiles, ofiles,
		    (filed.fd_lastfile+1) * FPSIZE)) {
			dprintf("can't read file structures at %p for pid %ld",
			    filed.fd_ofiles, (long)Pid);
			return;
		}
	} else
		bcopy(filed0.fd_dfiles, ofiles, (filed.fd_lastfile+1) * FPSIZE);
	for (i = 0; i <= filed.fd_lastfile; i++) {
		if (ofiles[i] == NULL)
			continue;
		if (!KVM_READ(ofiles[i], &file, sizeof (struct file))) {
			dprintf("can't read file %d at %p for pid %ld",
				i, ofiles[i], (long)Pid);
			continue;
		}
		if (file.f_type == DTYPE_VNODE)
			vtrans((struct vnode *)file.f_data, i, file.f_flag,
			    file.f_offset);
		else if (file.f_type == DTYPE_SOCKET) {
			if (checkfile == 0)
				socktrans((struct socket *)file.f_data, i);
		} else if (file.f_type == DTYPE_PIPE) {
			if (checkfile == 0)
				pipetrans((struct pipe *)file.f_data, i);
		} else if (file.f_type == DTYPE_KQUEUE) {
			if (checkfile == 0)
				kqueuetrans((struct kqueue *)file.f_data, i);
		} else if (file.f_type == DTYPE_CRYPTO) {
			if (checkfile == 0)
				cryptotrans(file.f_data, i);
		} else if (file.f_type == DTYPE_SYSTRACE) {
			if (checkfile == 0)
				systracetrans((struct fsystrace *)file.f_data, i);
		} else {
			dprintf("unknown file type %d for file %d of pid %ld",
				file.f_type, i, (long)Pid);
		}
	}
}

void
vtrans(struct vnode *vp, int i, int flag, off_t offset)
{
	struct vnode vn;
	struct filestat fst;
	char rw[3], mode[17];
	char *badtype = NULL, *filename;

	filename = badtype = NULL;
	if (!KVM_READ(vp, &vn, sizeof (struct vnode))) {
		dprintf("can't read vnode at %p for pid %ld", vp, (long)Pid);
		return;
	}
	if (vn.v_type == VNON || vn.v_tag == VT_NON)
		badtype = "none";
	else if (vn.v_type == VBAD)
		badtype = "bad";
	else
		switch (vn.v_tag) {
		case VT_UFS:
		case VT_MFS:
			if (!ufs_filestat(&vn, &fst))
				badtype = "error";
			break;
		case VT_NFS:
			if (!nfs_filestat(&vn, &fst))
				badtype = "error";
			break;
		case VT_EXT2FS:
			if (!ext2fs_filestat(&vn, &fst))
				badtype = "error";
			break;
		case VT_ISOFS:
			if (!isofs_filestat(&vn, &fst))
				badtype = "error";
			break;
		case VT_MSDOSFS:
			if (!msdos_filestat(&vn, &fst))
				badtype = "error";
			break;
		case VT_XFS:
			if (!xfs_filestat(&vn, &fst))
				badtype = "error";
			break;
		default: {
			static char unknown[30];
			snprintf(badtype = unknown, sizeof unknown,
			    "?(%x)", vn.v_tag);
			break;
		}
	}
	if (checkfile) {
		int fsmatch = 0;
		DEVS *d;

		if (badtype)
			return;
		for (d = devs; d != NULL; d = d->next)
			if (d->fsid == fst.fsid) {
				fsmatch = 1;
				if (d->ino == fst.fileid) {
					filename = d->name;
					break;
				}
			}
		if (fsmatch == 0 || (filename == NULL && fsflg == 0))
			return;
	}
	PREFIX(i);
	if (badtype) {
		(void)printf(" -           -  %10s    -\n", badtype);
		return;
	}
	if (nflg)
		(void)printf(" %2ld,%-2ld", (long)major(fst.fsid),
		    (long)minor(fst.fsid));
	else
		(void)printf(" %-8s", getmnton(vn.v_mount));
	if (nflg)
		(void)snprintf(mode, sizeof mode, "%o", fst.mode);
	else
		strmode(fst.mode, mode);
	(void)printf(" %8ld %11s", fst.fileid, mode);
	rw[0] = '\0';
	if (flag & FREAD)
		strlcat(rw, "r", sizeof rw);
	if (flag & FWRITE)
		strlcat(rw, "w", sizeof rw);
	printf(" %2s", rw);
	switch (vn.v_type) {
	case VBLK:
	case VCHR: {
		char *name;

		if (nflg || ((name = devname(fst.rdev, vn.v_type == VCHR ?
		    S_IFCHR : S_IFBLK)) == NULL))
			printf("   %2d,%-3d", major(fst.rdev), minor(fst.rdev));
		else
			printf("  %7s", name);
		if (oflg)
			printf("         ");
		break;
	}
	default:
		printf(" %8lld", (long long)fst.size);
		if (oflg)
			printf(":%-8lld", (long long)offset);
	}
	if (filename && !fsflg)
		printf(" %s", filename);
	putchar('\n');
}

int
ufs_filestat(struct vnode *vp, struct filestat *fsp)
{
	struct inode inode;
	struct ufs1_dinode di1;

	if (!KVM_READ(VTOI(vp), &inode, sizeof (inode))) {
		dprintf("can't read inode at %p for pid %ld",
		    VTOI(vp), (long)Pid);
		return 0;
	}

	if (!KVM_READ(inode.i_din1, &di1, sizeof(struct ufs1_dinode))) {
		dprintf("can't read dinode at %p for pid %ld",
		    inode.i_din1, (long)Pid);
		return (0);
	}

	inode.i_din1 = &di1;

	fsp->fsid = inode.i_dev & 0xffff;
	fsp->fileid = (long)inode.i_number;
	fsp->mode = inode.i_ffs_mode;
	fsp->size = inode.i_ffs_size;
	fsp->rdev = inode.i_ffs_rdev;

	return 1;
}

int
ext2fs_filestat(struct vnode *vp, struct filestat *fsp)
{
	struct inode inode;
	struct ext2fs_dinode e2di;

	if (!KVM_READ(VTOI(vp), &inode, sizeof (inode))) {
		dprintf("can't read inode at %p for pid %ld",
		    VTOI(vp), (long)Pid);
		return 0;
	}

	if (!KVM_READ(inode.i_e2din, &e2di, sizeof(struct ext2fs_dinode))) {
		dprintf("can't read dinode at %p for pid %ld",
		    inode.i_e2din, (long)Pid);
		return (0);
	}

	inode.i_e2din = &e2di;

	fsp->fsid = inode.i_dev & 0xffff;
	fsp->fileid = (long)inode.i_number;
	fsp->mode = inode.i_e2fs_mode;
	fsp->size = inode.i_e2fs_size;
	fsp->rdev = 0;	/* XXX */

	return 1;
}

int
msdos_filestat(struct vnode *vp, struct filestat *fsp)
{
#if 0
	struct inode inode;

	if (!KVM_READ(VTOI(vp), &inode, sizeof (inode))) {
		dprintf("can't read inode at %p for pid %ld",
		    VTOI(vp), (long)Pid);
		return 0;
	}
	fsp->fsid = inode.i_dev & 0xffff;
	fsp->fileid = (long)inode.i_number;
	fsp->mode = inode.i_e2fs_mode;
	fsp->size = inode.i_e2fs_size;
	fsp->rdev = 0;	/* XXX */
#endif

	return 1;
}

int
nfs_filestat(struct vnode *vp, struct filestat *fsp)
{
	struct nfsnode nfsnode;
	mode_t mode;

	if (!KVM_READ(VTONFS(vp), &nfsnode, sizeof (nfsnode))) {
		dprintf("can't read nfsnode at %p for pid %ld",
		    VTONFS(vp), (long)Pid);
		return 0;
	}
	fsp->fsid = nfsnode.n_vattr.va_fsid;
	fsp->fileid = nfsnode.n_vattr.va_fileid;
	fsp->size = nfsnode.n_size;
	fsp->rdev = nfsnode.n_vattr.va_rdev;
	mode = (mode_t)nfsnode.n_vattr.va_mode;
	switch (vp->v_type) {
	case VREG:
		mode |= S_IFREG;
		break;
	case VDIR:
		mode |= S_IFDIR;
		break;
	case VBLK:
		mode |= S_IFBLK;
		break;
	case VCHR:
		mode |= S_IFCHR;
		break;
	case VLNK:
		mode |= S_IFLNK;
		break;
	case VSOCK:
		mode |= S_IFSOCK;
		break;
	case VFIFO:
		mode |= S_IFIFO;
		break;
	default:
		break;
	}
	fsp->mode = mode;

	return 1;
}

int
xfs_filestat(struct vnode *vp, struct filestat *fsp)
{
	struct xfs_node xfs_node;

	if (!KVM_READ(VNODE_TO_XNODE(vp), &xfs_node, sizeof (xfs_node))) {
		dprintf("can't read xfs_node at %p for pid %ld",
		    VTOI(vp), (long)Pid);
		return 0;
	}
	fsp->fsid = xfs_node.attr.va_fsid;
	fsp->fileid = (long)xfs_node.attr.va_fileid;
	fsp->mode = xfs_node.attr.va_mode;
	fsp->size = xfs_node.attr.va_size;
	fsp->rdev = xfs_node.attr.va_rdev;

	return 1;
}

char *
getmnton(struct mount *m)
{
	static struct mount mount;
	static struct mtab {
		struct mtab *next;
		struct mount *m;
		char mntonname[MNAMELEN];
	} *mhead = NULL;
	struct mtab *mt;

	for (mt = mhead; mt != NULL; mt = mt->next)
		if (m == mt->m)
			return (mt->mntonname);
	if (!KVM_READ(m, &mount, sizeof(struct mount))) {
		warn("can't read mount table at %p", m);
		return (NULL);
	}
	if ((mt = malloc(sizeof (struct mtab))) == NULL)
		err(1, "malloc");
	mt->m = m;
	bcopy(&mount.mnt_stat.f_mntonname[0], &mt->mntonname[0], MNAMELEN);
	mt->next = mhead;
	mhead = mt;
	return (mt->mntonname);
}

void
pipetrans(struct pipe *pipe, int i)
{
	struct pipe pi;
	void *maxaddr;

	PREFIX(i);

	printf(" ");

	/* fill in socket */
	if (!KVM_READ(pipe, &pi, sizeof(struct pipe))) {
		dprintf("can't read pipe at %p", pipe);
		goto bad;
	}

	/*
	 * We don't have enough space to fit both peer and own address, so
	 * we select the higher address so both ends of the pipe have the
	 * same visible addr. (it's the higher address because when the other
	 * end closes, it becomes 0)
	 */
	maxaddr = MAX(pipe, pi.pipe_peer);

	printf("pipe %p state: %s%s%s\n", maxaddr,
	    (pi.pipe_state & PIPE_WANTR) ? "R" : "",
	    (pi.pipe_state & PIPE_WANTW) ? "W" : "",
	    (pi.pipe_state & PIPE_EOF) ? "E" : "");
	return;
bad:
	printf("* error\n");
}

void
kqueuetrans(struct kqueue *kq, int i)
{
	struct kqueue kqi;

	PREFIX(i);

	printf(" ");

	/* fill it in */
	if (!KVM_READ(kq, &kqi, sizeof(struct kqueue))) {
		dprintf("can't read kqueue at %p", kq);
		goto bad;
	}

	printf("kqueue %p %d state: %s%s\n", kq, kqi.kq_count,
	    (kqi.kq_state & KQ_SEL) ? "S" : "",
	    (kqi.kq_state & KQ_SLEEP) ? "W" : "");
	return;
bad:
	printf("* error\n");
}

void
cryptotrans(void *f, int i)
{
	PREFIX(i);

	printf(" ");

	printf("crypto %p\n", f);
}

void
systracetrans(struct fsystrace *f, int i)
{
	struct fsystrace fi;

	PREFIX(i);

	printf(" ");

	/* fill it in */
	if (!KVM_READ(f, &fi, sizeof(fi))) {
		dprintf("can't read fsystrace at %p", f);
		goto bad;
	}

	printf("systrace %p npol %d\n", f, fi.npolicies);
	return;
bad:
	printf("* error\n");
}

#ifdef INET6
const char *
inet6_addrstr(struct in6_addr *p)
{
	struct sockaddr_in6 sin6;
	static char hbuf[NI_MAXHOST];
	const int niflags = NI_NUMERICHOST;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *p;
	if (IN6_IS_ADDR_LINKLOCAL(p) &&
	    *(u_int16_t *)&sin6.sin6_addr.s6_addr[2] != 0) {
		sin6.sin6_scope_id =
		    ntohs(*(u_int16_t *)&sin6.sin6_addr.s6_addr[2]);
		sin6.sin6_addr.s6_addr[2] = sin6.sin6_addr.s6_addr[3] = 0;
	}

	if (getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
	    hbuf, sizeof(hbuf), NULL, 0, niflags))
		return "invalid";

	return hbuf;
}
#endif

void
socktrans(struct socket *sock, int i)
{
	static char *stypename[] = {
		"unused",	/* 0 */
		"stream",	/* 1 */
		"dgram",	/* 2 */
		"raw",		/* 3 */
		"rdm",		/* 4 */
		"seqpak"	/* 5 */
	};
#define	STYPEMAX 5
	struct socket	so;
	struct protosw	proto;
	struct domain	dom;
	struct inpcb	inpcb;
	struct unpcb	unpcb;
	int len;
	char dname[32];
#ifdef INET6
	char xaddrbuf[NI_MAXHOST + 2];
#endif

	PREFIX(i);

	/* fill in socket */
	if (!KVM_READ(sock, &so, sizeof(struct socket))) {
		dprintf("can't read sock at %p", sock);
		goto bad;
	}

	/* fill in protosw entry */
	if (!KVM_READ(so.so_proto, &proto, sizeof(struct protosw))) {
		dprintf("can't read protosw at %p", so.so_proto);
		goto bad;
	}

	/* fill in domain */
	if (!KVM_READ(proto.pr_domain, &dom, sizeof(struct domain))) {
		dprintf("can't read domain at %p", proto.pr_domain);
		goto bad;
	}

	if ((len = kvm_read(kd, (u_long)dom.dom_name, dname,
	    sizeof(dname) - 1)) != sizeof(dname) -1) {
		dprintf("can't read domain name at %p", dom.dom_name);
		dname[0] = '\0';
	} else
		dname[len] = '\0';

	if ((u_short)so.so_type > STYPEMAX)
		printf("* %s ?%d", dname, so.so_type);
	else
		printf("* %s %s", dname, stypename[so.so_type]);

	/*
	 * protocol specific formatting
	 *
	 * Try to find interesting things to print.  For tcp, the interesting
	 * thing is the address of the tcpcb, for udp and others, just the
	 * inpcb (socket pcb).  For unix domain, its the address of the socket
	 * pcb and the address of the connected pcb (if connected).  Otherwise
	 * just print the protocol number and address of the socket itself.
	 * The idea is not to duplicate netstat, but to make available enough
	 * information for further analysis.
	 */
	switch(dom.dom_family) {
	case AF_INET:
		getinetproto(proto.pr_protocol);
		if (proto.pr_protocol == IPPROTO_TCP) {
			if (so.so_pcb == NULL)
				break;
			if (kvm_read(kd, (u_long)so.so_pcb, (char *)&inpcb,
			    sizeof(struct inpcb)) != sizeof(struct inpcb)) {
				dprintf("can't read inpcb at %p", so.so_pcb);
				goto bad;
			}
			printf(" %p", inpcb.inp_ppcb);
			printf(" %s:%d",
			    inpcb.inp_laddr.s_addr == INADDR_ANY ? "*" :
			    inet_ntoa(inpcb.inp_laddr),
			    ntohs(inpcb.inp_lport));
			if (inpcb.inp_fport) {
				if (so.so_state & SS_CONNECTOUT)
					printf(" --> ");
				else
					printf(" <-- ");
				printf("%s:%d",
				    inpcb.inp_faddr.s_addr == INADDR_ANY ? "*" :
				    inet_ntoa(inpcb.inp_faddr),
				    ntohs(inpcb.inp_fport));
			}
		} else if (proto.pr_protocol == IPPROTO_UDP) {
			if (so.so_pcb == NULL)
				break;
			if (kvm_read(kd, (u_long)so.so_pcb, (char *)&inpcb,
			    sizeof(struct inpcb)) != sizeof(struct inpcb)) {
				dprintf("can't read inpcb at %p", so.so_pcb);
				goto bad;
			}
			printf(" %s:%d",
			    inpcb.inp_laddr.s_addr == INADDR_ANY ? "*" :
			    inet_ntoa(inpcb.inp_laddr),
			    ntohs(inpcb.inp_lport));
			if (inpcb.inp_fport)
				printf(" <-> %s:%d",
				    inpcb.inp_faddr.s_addr == INADDR_ANY ? "*" :
				    inet_ntoa(inpcb.inp_faddr),
				    ntohs(inpcb.inp_fport));
		} else if (so.so_pcb)
			printf(" %p", so.so_pcb);
		break;
#ifdef INET6
	case AF_INET6:
		getinetproto(proto.pr_protocol);
		if (proto.pr_protocol == IPPROTO_TCP) {
			if (so.so_pcb == NULL)
				break;
			if (kvm_read(kd, (u_long)so.so_pcb, (char *)&inpcb,
			    sizeof(struct inpcb)) != sizeof(struct inpcb)) {
				dprintf("can't read inpcb at %p", so.so_pcb);
				goto bad;
			}
			printf(" %p", inpcb.inp_ppcb);
			snprintf(xaddrbuf, sizeof(xaddrbuf), "[%s]",
			    inet6_addrstr(&inpcb.inp_laddr6));
			printf(" %s:%d",
			    IN6_IS_ADDR_UNSPECIFIED(&inpcb.inp_laddr6) ? "*" :
			    xaddrbuf,
			    ntohs(inpcb.inp_lport));
			if (inpcb.inp_fport) {
				if (so.so_state & SS_CONNECTOUT)
					printf(" --> ");
				else
					printf(" <-- ");
				snprintf(xaddrbuf, sizeof(xaddrbuf), "[%s]",
				    inet6_addrstr(&inpcb.inp_faddr6));
				printf("%s:%d",
				    IN6_IS_ADDR_UNSPECIFIED(&inpcb.inp_faddr6) ? "*" :
				    xaddrbuf,
				    ntohs(inpcb.inp_fport));
			}
		} else if (proto.pr_protocol == IPPROTO_UDP) {
			if (so.so_pcb == NULL)
				break;
			if (kvm_read(kd, (u_long)so.so_pcb, (char *)&inpcb,
			    sizeof(struct inpcb)) != sizeof(struct inpcb)) {
				dprintf("can't read inpcb at %p", so.so_pcb);
				goto bad;
			}
			snprintf(xaddrbuf, sizeof(xaddrbuf), "[%s]",
			    inet6_addrstr(&inpcb.inp_laddr6));
			printf(" %s:%d",
			    IN6_IS_ADDR_UNSPECIFIED(&inpcb.inp_laddr6) ? "*" :
			    xaddrbuf,
			    ntohs(inpcb.inp_lport));
			if (inpcb.inp_fport) {
				snprintf(xaddrbuf, sizeof(xaddrbuf), "[%s]",
				    inet6_addrstr(&inpcb.inp_faddr6));
				printf(" <-> %s:%d",
				    IN6_IS_ADDR_UNSPECIFIED(&inpcb.inp_faddr6) ? "*" :
				    xaddrbuf,
				    ntohs(inpcb.inp_fport));
			}
		} else if (so.so_pcb)
			printf(" %p", so.so_pcb);
		break;
#endif
	case AF_UNIX:
		/* print address of pcb and connected pcb */
		if (so.so_pcb) {
			printf(" %p", so.so_pcb);
			if (kvm_read(kd, (u_long)so.so_pcb, (char *)&unpcb,
			    sizeof(struct unpcb)) != sizeof(struct unpcb)){
				dprintf("can't read unpcb at %p", so.so_pcb);
				goto bad;
			}
			if (unpcb.unp_conn) {
				char shoconn[4], *cp;

				cp = shoconn;
				if (!(so.so_state & SS_CANTRCVMORE))
					*cp++ = '<';
				*cp++ = '-';
				if (!(so.so_state & SS_CANTSENDMORE))
					*cp++ = '>';
				*cp = '\0';
				printf(" %s %p", shoconn,
				    unpcb.unp_conn);
			}
		}
		break;
	default:
		/* print protocol number and socket address */
		printf(" %d %p", proto.pr_protocol, sock);
	}
	printf("\n");
	return;
bad:
	printf("* error\n");
}

/*
 * getinetproto --
 *	print name of protocol number
 */
void
getinetproto(number)
	int number;
{
	static int isopen;
	struct protoent *pe;

	if (!isopen)
		setprotoent(++isopen);
	if ((pe = getprotobynumber(number)) != NULL)
		printf(" %s", pe->p_name);
	else
		printf(" %d", number);
}

int
getfname(char *filename)
{
	struct stat statbuf;
	DEVS *cur;

	if (stat(filename, &statbuf)) {
		warn("%s", filename);
		return(0);
	}
	if ((cur = malloc(sizeof(DEVS))) == NULL)
		err(1, "malloc");
	cur->next = devs;
	devs = cur;

	cur->ino = statbuf.st_ino;
	cur->fsid = statbuf.st_dev & 0xffff;
	cur->name = filename;
	return(1);
}

void
usage(void)
{
	fprintf(stderr, "usage: fstat [-fnov] [-M core] [-N system] "
	    "[-p pid] [-u user] [file ...]\n");
	exit(1);
}
