/*	$OpenBSD: fstat.c,v 1.26 2000/01/17 16:26:19 itojun Exp $	*/

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
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)fstat.c	8.1 (Berkeley) 6/6/93";*/
static char *rcsid = "$OpenBSD: fstat.c,v 1.26 2000/01/17 16:26:19 itojun Exp $";
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
#include <sys/unpcb.h>
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#include <sys/mount.h>
#define	_KERNEL
#include <sys/file.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <miscfs/nullfs/null.h>
#undef _KERNEL
#define NFS
#include <nfs/nfsproto.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#undef NFS

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

int 	fsflg,	/* show files on same filesystem as file(s) argument */
	pflg,	/* show files open by a particular pid */
	uflg;	/* show files open by a particular (effective) user */
int 	checkfile; /* true if restricting to particular files or filesystems */
int	nflg;	/* (numerical) display f.s. and rdev as dev_t */
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

int ufs_filestat __P((struct vnode *, struct filestat *));
int ext2fs_filestat __P((struct vnode *, struct filestat *));
int isofs_filestat __P((struct vnode *, struct filestat *));
int msdos_filestat __P((struct vnode *, struct filestat *));
int nfs_filestat __P((struct vnode *, struct filestat *));
void dofiles __P((struct kinfo_proc *));
void getinetproto __P((int));
void socktrans __P((struct socket *, int));
void usage __P((void));
void vtrans __P((struct vnode *, int, int));
int getfname __P((char *));
void pipetrans __P((struct pipe *, int));

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	register struct passwd *passwd;
	struct kinfo_proc *p, *plast;
	int arg, ch, what;
	char *memf, *nlistf;
	char buf[_POSIX2_LINE_MAX];
	int cnt;

	arg = 0;
	what = KERN_PROC_ALL;
	nlistf = memf = NULL;
	while ((ch = getopt(argc, argv, "fnp:u:vN:M:")) != -1)
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
		case 'p':
			if (pflg++)
				usage();
			if (!isdigit(*optarg)) {
				warnx( "-p requires a process id\n");
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
		case '?':
		default:
			usage();
		}

	if (*(argv += optind)) {
		for (; *argv; ++argv) {
			if (getfname(*argv))
				checkfile = 1;
		}
		if (!checkfile)	/* file(s) specified, but none accessable */
			exit(1);
	}

	ALLOC_OFILES(256);	/* reserve space for file pointers */

	if (fsflg && !checkfile) {	
		/* -f with no files means use wd */
		if (getfname(".") == 0)
			exit(1);
		checkfile = 1;
	}

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL) {
		setegid(getgid());
		setgid(getgid());
	}

	if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, buf)) == NULL)
		errx(1, "%s", buf);

	setegid(getgid());
	setgid(getgid());

	if ((p = kvm_getprocs(kd, what, arg, &cnt)) == NULL)
		errx(1, "%s", kvm_geterr(kd));
	if (nflg)
		printf("%s",
"USER     CMD          PID   FD  DEV    INUM       MODE SZ|DV R/W");
	else
		printf("%s",
"USER     CMD          PID   FD MOUNT      INUM MODE         SZ|DV R/W");
	if (checkfile && fsflg == 0)
		printf(" NAME\n");
	else
		putchar('\n');

	for (plast = &p[cnt]; p < plast; ++p) {
		if (p->kp_proc.p_stat == SZOMB)
			continue;
		dofiles(p);
	}
	exit(0);
}

char	*Uname, *Comm;
pid_t	Pid;

#define PREFIX(i) printf("%-8.8s %-10s %5d", Uname, Comm, Pid); \
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
	}

/*
 * print open files attributed to this process
 */
void
dofiles(kp)
	struct kinfo_proc *kp;
{
	int i;
	struct file file;
	struct filedesc0 filed0;
#define	filed	filed0.fd_fd
	struct proc *p = &kp->kp_proc;
	struct eproc *ep = &kp->kp_eproc;

	extern char *user_from_uid();

	Uname = user_from_uid(ep->e_ucred.cr_uid, 0);
	Pid = p->p_pid;
	Comm = p->p_comm;

	if (p->p_fd == NULL)
		return;
	if (!KVM_READ(p->p_fd, &filed0, sizeof (filed0))) {
		dprintf("can't read filedesc at %p for pid %d", p->p_fd, Pid);
		return;
	}
	if (filed.fd_nfiles < 0 || filed.fd_lastfile >= filed.fd_nfiles ||
	    filed.fd_freefile > filed.fd_lastfile + 1) {
		dprintf("filedesc corrupted at %p for pid %d", p->p_fd, Pid);
		return;
	}
	/*
	 * root directory vnode, if one
	 */
	if (filed.fd_rdir)
		vtrans(filed.fd_rdir, RDIR, FREAD);
	/*
	 * current working directory vnode
	 */
	vtrans(filed.fd_cdir, CDIR, FREAD);
	/*
	 * ktrace vnode, if one
	 */
	if (p->p_tracep)
		vtrans(p->p_tracep, TRACE, FREAD|FWRITE);
	/*
	 * open files
	 */
#define FPSIZE	(sizeof (struct file *))
	ALLOC_OFILES(filed.fd_lastfile+1);
	if (filed.fd_nfiles > NDFILE) {
		if (!KVM_READ(filed.fd_ofiles, ofiles,
		    (filed.fd_lastfile+1) * FPSIZE)) {
			dprintf("can't read file structures at %p for pid %d",
			    filed.fd_ofiles, Pid);
			return;
		}
	} else
		bcopy(filed0.fd_dfiles, ofiles, (filed.fd_lastfile+1) * FPSIZE);
	for (i = 0; i <= filed.fd_lastfile; i++) {
		if (ofiles[i] == NULL)
			continue;
		if (!KVM_READ(ofiles[i], &file, sizeof (struct file))) {
			dprintf("can't read file %d at %p for pid %d",
				i, ofiles[i], Pid);
			continue;
		}
		if (file.f_type == DTYPE_VNODE)
			vtrans((struct vnode *)file.f_data, i, file.f_flag);
		else if (file.f_type == DTYPE_SOCKET) {
			if (checkfile == 0)
				socktrans((struct socket *)file.f_data, i);
		} else if (file.f_type == DTYPE_PIPE) {
			if (checkfile == 0)
				pipetrans((struct pipe *)file.f_data, i);
		} else {
			dprintf("unknown file type %d for file %d of pid %d",
				file.f_type, i, Pid);
		}
	}
}

void
vtrans(vp, i, flag)
	struct vnode *vp;
	int i;
	int flag;
{
	struct vnode vn;
	struct filestat fst;
	char rw[3], mode[17];
	char *badtype = NULL, *filename, *getmnton();

	filename = badtype = NULL;
	if (!KVM_READ(vp, &vn, sizeof (struct vnode))) {
		dprintf("can't read vnode at %p for pid %d", vp, Pid);
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
		case VT_NULL:
			if (!null_filestat(&vn, &fst))
				badtype = "error";
			break;		
		default: {
			static char unknown[30];
			sprintf(badtype = unknown, "?(%x)", vn.v_tag);
			break;
		}
	}
	if (checkfile) {
		int fsmatch = 0;
		register DEVS *d;

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
		(void)printf(" -         -  %10s    -\n", badtype);
		return;
	}
	if (nflg)
		(void)printf(" %2d,%-2d", major(fst.fsid), minor(fst.fsid));
	else
		(void)printf(" %-8s", getmnton(vn.v_mount));
	if (nflg)
		(void)sprintf(mode, "%o", fst.mode);
	else
		strmode(fst.mode, mode);
	(void)printf(" %6ld %10s", fst.fileid, mode);
	switch (vn.v_type) {
	case VBLK:
	case VCHR: {
		char *name;

		if (nflg || ((name = devname(fst.rdev, vn.v_type == VCHR ? 
		    S_IFCHR : S_IFBLK)) == NULL))
			printf("  %2d,%-2d", major(fst.rdev), minor(fst.rdev));
		else
			printf(" %6s", name);
		break;
	}
	default:
		printf(" %6qd", fst.size);
	}
	rw[0] = '\0';
	if (flag & FREAD)
		strcat(rw, "r");
	if (flag & FWRITE)
		strcat(rw, "w");
	printf(" %2s", rw);
	if (filename && !fsflg)
		printf("  %s", filename);
	putchar('\n');
}

int
ufs_filestat(vp, fsp)
	struct vnode *vp;
	struct filestat *fsp;
{
	struct inode inode;

	if (!KVM_READ(VTOI(vp), &inode, sizeof (inode))) {
		dprintf("can't read inode at %p for pid %d", VTOI(vp), Pid);
		return 0;
	}
	fsp->fsid = inode.i_dev & 0xffff;
	fsp->fileid = (long)inode.i_number;
	fsp->mode = inode.i_ffs_mode;
	fsp->size = inode.i_ffs_size;
	fsp->rdev = inode.i_ffs_rdev;

	return 1;
}

int
ext2fs_filestat(vp, fsp)
	struct vnode *vp;
	struct filestat *fsp;
{
	struct inode inode;

	if (!KVM_READ(VTOI(vp), &inode, sizeof (inode))) {
		dprintf("can't read inode at %p for pid %d", VTOI(vp), Pid);
		return 0;
	}
	fsp->fsid = inode.i_dev & 0xffff;
	fsp->fileid = (long)inode.i_number;
	fsp->mode = inode.i_e2fs_mode;
	fsp->size = inode.i_e2fs_size;
	fsp->rdev = 0;	/* XXX */

	return 1;
}

int
msdos_filestat(vp, fsp)
	struct vnode *vp;
	struct filestat *fsp;
{
#if 0
	struct inode inode;

	if (!KVM_READ(VTOI(vp), &inode, sizeof (inode))) {
		dprintf("can't read inode at %p for pid %d", VTOI(vp), Pid);
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
nfs_filestat(vp, fsp)
	struct vnode *vp;
	struct filestat *fsp;
{
	struct nfsnode nfsnode;
	register mode_t mode;

	if (!KVM_READ(VTONFS(vp), &nfsnode, sizeof (nfsnode))) {
		dprintf("can't read nfsnode at %p for pid %d", VTONFS(vp), Pid);
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
	};
	fsp->mode = mode;

	return 1;
}

int
xfs_filestat(vp, fsp)
	struct vnode *vp;
	struct filestat *fsp;
{
	struct xfs_node xfs_node;

	if (!KVM_READ(VNODE_TO_XNODE(vp), &xfs_node, sizeof (xfs_node))) {
		dprintf("can't read xfs_node at %p for pid %d", VTOI(vp), Pid);
		return 0;
	}
	fsp->fsid = xfs_node.attr.va_fsid;
	fsp->fileid = (long)xfs_node.attr.va_fileid;
	fsp->mode = xfs_node.attr.va_mode;
	fsp->size = xfs_node.attr.va_size;
	fsp->rdev = xfs_node.attr.va_rdev;

	return 1;
}

int
null_filestat(vp, fsp)
	struct vnode *vp;
	struct filestat *fsp;
{
	struct null_node node;
	struct filestat fst;
	struct vnode vn;
	int fail = 1;

	memset(&fst, 0, sizeof fst);

	if (!KVM_READ(VTONULL(vp), &node, sizeof (node))) {
		dprintf("can't read node at %p for pid %d", VTONULL(vp), Pid);
		return 0;
	}

	/*
	 * Attempt to find information that might be useful.
	 */
	if (node.null_lowervp) {
		if (!KVM_READ(node.null_lowervp, &vn, sizeof (vn))) {
			dprintf("can't read vnode at %p for pid %d",
			    node.null_lowervp, Pid);
			return 0;
		}

		fail = 0;
		if (vn.v_type == VNON || vn.v_tag == VT_NON)
			fail = 1;
		else if (vn.v_type == VBAD)
			fail = 1;
		else
			switch (vn.v_tag) {
			case VT_UFS:
			case VT_MFS:
				if (!ufs_filestat(&vn, &fst))
					fail = 1;
				break;
			case VT_NFS:
				if (!nfs_filestat(&vn, &fst))
					fail = 1;
				break;
			case VT_EXT2FS:
				if (!ext2fs_filestat(&vn, &fst))
					fail = 1;
				break;
			case VT_ISOFS:
				if (!isofs_filestat(&vn, &fst))
					fail = 1;
				break;
			case VT_MSDOSFS:
				if (!msdos_filestat(&vn, &fst))
					fail = 1;
				break;
			case VT_XFS:
				if (!xfs_filestat(&vn, &fst))
					fail = 1;
				break;
			default:
				break;
			}
	}

	fsp->fsid = (long)node.null_vnode;
	if (fail)
		fsp->fileid = (long)node.null_lowervp;
	else
		fsp->fileid = fst.fileid; 
	fsp->mode = fst.mode;
	fsp->size = fst.mode;
	fsp->rdev = fst.mode;

	return 1;
}

char *
getmnton(m)
	struct mount *m;
{
	static struct mount mount;
	static struct mtab {
		struct mtab *next;
		struct mount *m;
		char mntonname[MNAMELEN];
	} *mhead = NULL;
	register struct mtab *mt;

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
pipetrans(pipe, i)
	struct pipe *pipe;
	int i;
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

	printf("pipe %p state: %s%s%s", maxaddr,
	       (pi.pipe_state & PIPE_WANTR) ? "R" : "",
	       (pi.pipe_state & PIPE_WANTW) ? "W" : "",
	       (pi.pipe_state & PIPE_EOF) ? "E" : "");
	
	printf("\n");
	return;
bad:
	printf("* error\n");
}

#ifdef INET6
const char *
inet6_addrstr(p)
	struct in6_addr *p;
{
	struct sockaddr_in6 sin6;
	static char hbuf[NI_MAXHOST];
#ifdef NI_WITHSCOPEID
	const int niflags = NI_NUMERICHOST | NI_WITHSCOPEID;
#else
	const int niflags = NI_NUMERICHOST
#endif

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
socktrans(sock, i)
	struct socket *sock;
	int i;
{
	static char *stypename[] = {
		"unused",	/* 0 */
		"stream", 	/* 1 */
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
	}
	else
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
			if (inpcb.inp_fport)
				snprintf(xaddrbuf, sizeof(xaddrbuf), "[%s]",
				    inet6_addrstr(&inpcb.inp_faddr6));
				printf(" <-> %s:%d",
				    IN6_IS_ADDR_UNSPECIFIED(&inpcb.inp_faddr6) ? "*" :
				    xaddrbuf,
				    ntohs(inpcb.inp_fport));
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
	register struct protoent *pe;

	if (!isopen)
		setprotoent(++isopen);
	if ((pe = getprotobynumber(number)) != NULL)
		printf(" %s", pe->p_name);
	else
		printf(" %d", number);
}

int
getfname(filename)
	char *filename;
{
	struct stat statbuf;
	DEVS *cur;

	if (stat(filename, &statbuf)) {
		warn(filename);
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
usage()
{
	(void)fprintf(stderr,
 "usage: fstat [-fnv] [-p pid] [-u user] [-N system] [-M core] [file ...]\n");
	exit(1);
}
