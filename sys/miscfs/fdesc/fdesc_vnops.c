/*	$NetBSD: fdesc_vnops.c,v 1.27 1995/10/09 14:20:04 mycroft Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *
 *	@(#)fdesc_vnops.c	8.12 (Berkeley) 8/20/94
 *
 * #Id: fdesc_vnops.c,v 1.12 1993/04/06 16:17:17 jsp Exp #
 */

/*
 * /dev/fd Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/kernel.h>	/* boottime */
#include <sys/resourcevar.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <miscfs/fdesc/fdesc.h>

#define cttyvp(p) ((p)->p_flag & P_CONTROLT ? (p)->p_session->s_ttyvp : NULL)

#define FDL_WANT	0x01
#define FDL_LOCKED	0x02
static int fdcache_lock;

dev_t devctty;

#if (FD_STDIN != FD_STDOUT-1) || (FD_STDOUT != FD_STDERR-1)
FD_STDIN, FD_STDOUT, FD_STDERR must be a sequence n, n+1, n+2
#endif

#define	NFDCACHE 4

#define FD_NHASH(ix) \
	(&fdhashtbl[(ix) & fdhash])
LIST_HEAD(fdhashhead, fdescnode) *fdhashtbl;
u_long fdhash;

/*
 * Initialise cache headers
 */
fdesc_init()
{

	devctty = makedev(nchrdev, 0);
	fdhashtbl = hashinit(NFDCACHE, M_CACHE, &fdhash);
}

int
fdesc_allocvp(ftype, ix, mp, vpp)
	fdntype ftype;
	int ix;
	struct mount *mp;
	struct vnode **vpp;
{
	struct fdhashhead *fc;
	struct fdescnode *fd;
	int error = 0;

	fc = FD_NHASH(ix);
loop:
	for (fd = fc->lh_first; fd != 0; fd = fd->fd_hash.le_next) {
		if (fd->fd_ix == ix && fd->fd_vnode->v_mount == mp) {
			if (vget(fd->fd_vnode, 0))
				goto loop;
			*vpp = fd->fd_vnode;
			return (error);
		}
	}

	/*
	 * otherwise lock the array while we call getnewvnode
	 * since that can block.
	 */ 
	if (fdcache_lock & FDL_LOCKED) {
		fdcache_lock |= FDL_WANT;
		sleep((caddr_t) &fdcache_lock, PINOD);
		goto loop;
	}
	fdcache_lock |= FDL_LOCKED;

	error = getnewvnode(VT_FDESC, mp, fdesc_vnodeop_p, vpp);
	if (error)
		goto out;
	MALLOC(fd, void *, sizeof(struct fdescnode), M_TEMP, M_WAITOK);
	(*vpp)->v_data = fd;
	fd->fd_vnode = *vpp;
	fd->fd_type = ftype;
	fd->fd_fd = -1;
	fd->fd_link = 0;
	fd->fd_ix = ix;
	LIST_INSERT_HEAD(fc, fd, fd_hash);

out:;
	fdcache_lock &= ~FDL_LOCKED;

	if (fdcache_lock & FDL_WANT) {
		fdcache_lock &= ~FDL_WANT;
		wakeup((caddr_t) &fdcache_lock);
	}

	return (error);
}

/*
 * vp is the current namei directory
 * ndp is the name to locate in that directory...
 */
int
fdesc_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	char *pname;
	struct proc *p;
	int nfiles;
	unsigned fd;
	int error;
	struct vnode *fvp;
	char *ln;

	pname = ap->a_cnp->cn_nameptr;
	if (ap->a_cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);	
		VOP_LOCK(dvp);
		return (0);
	}

	p = ap->a_cnp->cn_proc;
	nfiles = p->p_fd->fd_nfiles;

	switch (VTOFDESC(dvp)->fd_type) {
	default:
	case Flink:
	case Fdesc:
	case Fctty:
		error = ENOTDIR;
		goto bad;

	case Froot:
		if (ap->a_cnp->cn_namelen == 2 && bcmp(pname, "fd", 2) == 0) {
			error = fdesc_allocvp(Fdevfd, FD_DEVFD, dvp->v_mount, &fvp);
			if (error)
				goto bad;
			*vpp = fvp;
			fvp->v_type = VDIR;
			VOP_LOCK(fvp);
			return (0);
		}

		if (ap->a_cnp->cn_namelen == 3 && bcmp(pname, "tty", 3) == 0) {
			struct vnode *ttyvp = cttyvp(p);
			if (ttyvp == NULL) {
				error = ENXIO;
				goto bad;
			}
			error = fdesc_allocvp(Fctty, FD_CTTY, dvp->v_mount, &fvp);
			if (error)
				goto bad;
			*vpp = fvp;
			fvp->v_type = VFIFO;
			VOP_LOCK(fvp);
			return (0);
		}

		ln = 0;
		switch (ap->a_cnp->cn_namelen) {
		case 5:
			if (bcmp(pname, "stdin", 5) == 0) {
				ln = "fd/0";
				fd = FD_STDIN;
			}
			break;
		case 6:
			if (bcmp(pname, "stdout", 6) == 0) {
				ln = "fd/1";
				fd = FD_STDOUT;
			} else
			if (bcmp(pname, "stderr", 6) == 0) {
				ln = "fd/2";
				fd = FD_STDERR;
			}
			break;
		}

		if (ln) {
			error = fdesc_allocvp(Flink, fd, dvp->v_mount, &fvp);
			if (error)
				goto bad;
			VTOFDESC(fvp)->fd_link = ln;
			*vpp = fvp;
			fvp->v_type = VLNK;
			VOP_LOCK(fvp);
			return (0);
		} else {
			error = ENOENT;
			goto bad;
		}

		/* FALL THROUGH */

	case Fdevfd:
		if (ap->a_cnp->cn_namelen == 2 && bcmp(pname, "..", 2) == 0) {
			error = fdesc_root(dvp->v_mount, vpp);
			return (error);
		}

		fd = 0;
		while (*pname >= '0' && *pname <= '9') {
			fd = 10 * fd + *pname++ - '0';
			if (fd >= nfiles)
				break;
		}

		if (*pname != '\0') {
			error = ENOENT;
			goto bad;
		}

		if (fd >= nfiles || p->p_fd->fd_ofiles[fd] == NULL) {
			error = EBADF;
			goto bad;
		}

		error = fdesc_allocvp(Fdesc, FD_DESC+fd, dvp->v_mount, &fvp);
		if (error)
			goto bad;
		VTOFDESC(fvp)->fd_fd = fd;
		*vpp = fvp;
		return (0);
	}

bad:;
	*vpp = NULL;
	return (error);
}

int
fdesc_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	switch (VTOFDESC(vp)->fd_type) {
	case Fdesc:
		/*
		 * XXX Kludge: set p->p_dupfd to contain the value of the
		 * the file descriptor being sought for duplication. The error 
		 * return ensures that the vnode for this device will be
		 * released by vn_open. Open will detect this special error and
		 * take the actions in dupfdopen.  Other callers of vn_open or
		 * VOP_OPEN will simply report the error.
		 */
		ap->a_p->p_dupfd = VTOFDESC(vp)->fd_fd;	/* XXX */
		return (ENODEV);

	case Fctty:
		return (cttyopen(devctty, ap->a_mode, 0, ap->a_p));
	}

	return (0);
}

static int
fdesc_attr(fd, vap, cred, p)
	int fd;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *p;
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct stat stb;
	int error;

	if (fd >= fdp->fd_nfiles || (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);

	switch (fp->f_type) {
	case DTYPE_VNODE:
		error = VOP_GETATTR((struct vnode *) fp->f_data, vap, cred, p);
		if (error == 0 && vap->va_type == VDIR) {
			/*
			 * directories can cause loops in the namespace,
			 * so turn off the 'x' bits to avoid trouble.
			 */
			vap->va_mode &= ~((VEXEC)|(VEXEC>>3)|(VEXEC>>6));
		}
		break;

	case DTYPE_SOCKET:
		error = soo_stat((struct socket *)fp->f_data, &stb);
		if (error == 0) {
			vattr_null(vap);
			vap->va_type = VSOCK;
			vap->va_mode = stb.st_mode;
			vap->va_nlink = stb.st_nlink;
			vap->va_uid = stb.st_uid;
			vap->va_gid = stb.st_gid;
			vap->va_fsid = stb.st_dev;
			vap->va_fileid = stb.st_ino;
			vap->va_size = stb.st_size;
			vap->va_blocksize = stb.st_blksize;
			vap->va_atime = stb.st_atimespec;
			vap->va_mtime = stb.st_mtimespec;
			vap->va_ctime = stb.st_ctimespec;
			vap->va_gen = stb.st_gen;
			vap->va_flags = stb.st_flags;
			vap->va_rdev = stb.st_rdev;
			vap->va_bytes = stb.st_blocks * stb.st_blksize;
		}
		break;

	default:
		panic("fdesc attr");
		break;
	}

	return (error);
}

int
fdesc_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	unsigned fd;
	int error = 0;

	switch (VTOFDESC(vp)->fd_type) {
	case Froot:
	case Fdevfd:
	case Flink:
	case Fctty:
		bzero((caddr_t) vap, sizeof(*vap));
		vattr_null(vap);
		vap->va_fileid = VTOFDESC(vp)->fd_ix;

		switch (VTOFDESC(vp)->fd_type) {
		case Flink:
			vap->va_mode = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
			vap->va_type = VLNK;
			vap->va_nlink = 1;
			vap->va_size = strlen(VTOFDESC(vp)->fd_link);
			break;

		case Fctty:
			vap->va_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
			vap->va_type = VFIFO;
			vap->va_nlink = 1;
			vap->va_size = 0;
			break;

		default:
			vap->va_mode = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
			vap->va_type = VDIR;
			vap->va_nlink = 2;
			vap->va_size = DEV_BSIZE;
			break;
		}
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
		vap->va_blocksize = DEV_BSIZE;
		vap->va_atime.ts_sec = boottime.tv_sec;
		vap->va_atime.ts_nsec = 0;
		vap->va_mtime = vap->va_atime;
		vap->va_ctime = vap->va_mtime;
		vap->va_gen = 0;
		vap->va_flags = 0;
		vap->va_rdev = 0;
		vap->va_bytes = 0;
		break;

	case Fdesc:
		fd = VTOFDESC(vp)->fd_fd;
		error = fdesc_attr(fd, vap, ap->a_cred, ap->a_p);
		break;

	default:
		panic("fdesc_getattr");
		break;	
	}

	if (error == 0)
		vp->v_type = vap->va_type;

	return (error);
}

int
fdesc_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct filedesc *fdp = ap->a_p->p_fd;
	struct file *fp;
	unsigned fd;
	int error;

	/*
	 * Can't mess with the root vnode
	 */
	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fdesc:
		break;

	case Fctty:
		return (0);

	default:
		return (EACCES);
	}

	fd = VTOFDESC(ap->a_vp)->fd_fd;
	if (fd >= fdp->fd_nfiles || (fp = fdp->fd_ofiles[fd]) == NULL) {
		return (EBADF);
	}

	/*
	 * Can setattr the underlying vnode, but not sockets!
	 */
	switch (fp->f_type) {
	case DTYPE_VNODE:
		error = VOP_SETATTR((struct vnode *) fp->f_data, ap->a_vap, ap->a_cred, ap->a_p);
		break;

	case DTYPE_SOCKET:
		error = 0;
		break;

	default:
		panic("fdesc setattr");
		break;
	}

	return (error);
}

#define UIO_MX 32

struct fdesc_target {
	ino_t ft_fileno;
	u_char ft_type;
	u_char ft_namlen;
	char *ft_name;
} fdesc_targets[] = {
/* NOTE: The name must be less than UIO_MX-16 chars in length */
#define N(s) sizeof(s)-1, s
	{ FD_DEVFD,  DT_DIR,     N("fd")     },
	{ FD_STDIN,  DT_LNK,     N("stdin")  },
	{ FD_STDOUT, DT_LNK,     N("stdout") },
	{ FD_STDERR, DT_LNK,     N("stderr") },
	{ FD_CTTY,   DT_UNKNOWN, N("tty")    },
#undef N
};
static int nfdesc_targets = sizeof(fdesc_targets) / sizeof(fdesc_targets[0]);

int
fdesc_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *ap;
{
	struct uio *uio = ap->a_uio;
	struct dirent d;
	struct filedesc *fdp;
	int i;
	int error;
	u_long *cookies = ap->a_cookies;
	int ncookies = ap->a_ncookies;

	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fctty:
		return (0);

	case Fdesc:
		return (ENOTDIR);
	}

	fdp = uio->uio_procp->p_fd;

	if (uio->uio_resid < UIO_MX)
		return (EINVAL);
	if (uio->uio_offset < 0)
		return (EINVAL);

	error = 0;
	i = uio->uio_offset;
	bzero((caddr_t)&d, UIO_MX);
	d.d_reclen = UIO_MX;

	if (VTOFDESC(ap->a_vp)->fd_type == Froot) {
		struct fdesc_target *ft;

		for (ft = &fdesc_targets[i];
		     uio->uio_resid >= UIO_MX && i < nfdesc_targets; ft++, i++) {
			switch (ft->ft_fileno) {
			case FD_CTTY:
				if (cttyvp(uio->uio_procp) == NULL)
					continue;
				break;

			case FD_STDIN:
			case FD_STDOUT:
			case FD_STDERR:
				if ((ft->ft_fileno - FD_STDIN) >= fdp->fd_nfiles)
					continue;
				if (fdp->fd_ofiles[ft->ft_fileno - FD_STDIN] == NULL)
					continue;
				break;
			}

			d.d_fileno = ft->ft_fileno;
			d.d_namlen = ft->ft_namlen;
			bcopy(ft->ft_name, d.d_name, ft->ft_namlen + 1);
			d.d_type = ft->ft_type;

			if (error = uiomove((caddr_t)&d, UIO_MX, uio))
				break;
			if (ncookies-- > 0)
				*cookies++ = i + 1;
		}
	} else {
		for (; i - 2 < fdp->fd_nfiles && uio->uio_resid >= UIO_MX;
		     i++) {
			switch (i) {
			case 0:
			case 1:
				d.d_fileno = FD_ROOT;		/* XXX */
				d.d_namlen = i + 1;
				bcopy("..", d.d_name, d.d_namlen);
				d.d_name[i + 1] = '\0';
				d.d_type = DT_DIR;
				break;
	
			default:
				if (fdp->fd_ofiles[i - 2] == NULL)
					continue;
				d.d_fileno = i - 2 + FD_STDIN;
				d.d_namlen = sprintf(d.d_name, "%d", i - 2);
				d.d_type = DT_UNKNOWN;
				break;
			}

			if (error = uiomove((caddr_t)&d, UIO_MX, uio))
				break;
			if (ncookies-- > 0)
				*cookies++ = i + 1;
		}
	}

	uio->uio_offset = i;
	return (error);
}

int
fdesc_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	int error;

	if (vp->v_type != VLNK)
		return (EPERM);

	if (VTOFDESC(vp)->fd_type == Flink) {
		char *ln = VTOFDESC(vp)->fd_link;
		error = uiomove(ln, strlen(ln), ap->a_uio);
	} else {
		error = EOPNOTSUPP;
	}

	return (error);
}

int
fdesc_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error = EOPNOTSUPP;

	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fctty:
		error = cttyread(devctty, ap->a_uio, ap->a_ioflag);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	
	return (error);
}

int
fdesc_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error = EOPNOTSUPP;

	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fctty:
		error = cttywrite(devctty, ap->a_uio, ap->a_ioflag);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	
	return (error);
}

int
fdesc_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	int error = EOPNOTSUPP;

	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fctty:
		error = cttyioctl(devctty, ap->a_command, ap->a_data,
					ap->a_fflag, ap->a_p);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	
	return (error);
}

int
fdesc_select(ap)
	struct vop_select_args /* {
		struct vnode *a_vp;
		int  a_which;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	int error = EOPNOTSUPP;

	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fctty:
		error = cttyselect(devctty, ap->a_fflags, ap->a_p);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	
	return (error);
}

int
fdesc_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	/*
	 * Clear out the v_type field to avoid
	 * nasty things happening in vgone().
	 */
	vp->v_type = VNON;
	return (0);
}

int
fdesc_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct fdescnode *fd = VTOFDESC(vp);

	LIST_REMOVE(fd, fd_hash);
	FREE(vp->v_data, M_TEMP);
	vp->v_data = 0;

	return (0);
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
fdesc_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Print out the contents of a /dev/fd vnode.
 */
/* ARGSUSED */
int
fdesc_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	printf("tag VT_NON, fdesc vnode\n");
	return (0);
}

/*void*/
int
fdesc_vfree(ap)
	struct vop_vfree_args /* {
		struct vnode *a_pvp;
		ino_t a_ino;
		int a_mode;
	} */ *ap;
{

	return (0);
}

/*
 * /dev/fd vnode unsupported operation
 */
int
fdesc_enotsupp()
{

	return (EOPNOTSUPP);
}

/*
 * /dev/fd "should never get here" operation
 */
int
fdesc_badop()
{

	panic("fdesc: bad op");
	/* NOTREACHED */
}

/*
 * /dev/fd vnode null operation
 */
int
fdesc_nullop()
{

	return (0);
}

#define fdesc_create ((int (*) __P((struct  vop_create_args *)))fdesc_enotsupp)
#define fdesc_mknod ((int (*) __P((struct  vop_mknod_args *)))fdesc_enotsupp)
#define fdesc_close ((int (*) __P((struct  vop_close_args *)))nullop)
#define fdesc_access ((int (*) __P((struct  vop_access_args *)))nullop)
#define fdesc_mmap ((int (*) __P((struct  vop_mmap_args *)))fdesc_enotsupp)
#define fdesc_fsync ((int (*) __P((struct  vop_fsync_args *)))nullop)
#define fdesc_seek ((int (*) __P((struct  vop_seek_args *)))nullop)
#define fdesc_remove ((int (*) __P((struct  vop_remove_args *)))fdesc_enotsupp)
#define fdesc_link ((int (*) __P((struct  vop_link_args *)))fdesc_enotsupp)
#define fdesc_rename ((int (*) __P((struct  vop_rename_args *)))fdesc_enotsupp)
#define fdesc_mkdir ((int (*) __P((struct  vop_mkdir_args *)))fdesc_enotsupp)
#define fdesc_rmdir ((int (*) __P((struct  vop_rmdir_args *)))fdesc_enotsupp)
#define fdesc_symlink ((int (*) __P((struct vop_symlink_args *)))fdesc_enotsupp)
#define fdesc_abortop ((int (*) __P((struct  vop_abortop_args *)))nullop)
#define fdesc_lock ((int (*) __P((struct  vop_lock_args *)))nullop)
#define fdesc_unlock ((int (*) __P((struct  vop_unlock_args *)))nullop)
#define fdesc_bmap ((int (*) __P((struct  vop_bmap_args *)))fdesc_badop)
#define fdesc_strategy ((int (*) __P((struct  vop_strategy_args *)))fdesc_badop)
#define fdesc_islocked ((int (*) __P((struct  vop_islocked_args *)))nullop)
#define fdesc_advlock ((int (*) __P((struct vop_advlock_args *)))fdesc_enotsupp)
#define fdesc_blkatoff \
	((int (*) __P((struct  vop_blkatoff_args *)))fdesc_enotsupp)
#define fdesc_vget ((int (*) __P((struct  vop_vget_args *)))fdesc_enotsupp)
#define fdesc_valloc ((int(*) __P(( \
		struct vnode *pvp, \
		int mode, \
		struct ucred *cred, \
		struct vnode **vpp))) fdesc_enotsupp)
#define fdesc_truncate \
	((int (*) __P((struct  vop_truncate_args *)))fdesc_enotsupp)
#define fdesc_update ((int (*) __P((struct  vop_update_args *)))fdesc_enotsupp)
#define fdesc_bwrite ((int (*) __P((struct  vop_bwrite_args *)))fdesc_enotsupp)

int (**fdesc_vnodeop_p)();
struct vnodeopv_entry_desc fdesc_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, fdesc_lookup },	/* lookup */
	{ &vop_create_desc, fdesc_create },	/* create */
	{ &vop_mknod_desc, fdesc_mknod },	/* mknod */
	{ &vop_open_desc, fdesc_open },		/* open */
	{ &vop_close_desc, fdesc_close },	/* close */
	{ &vop_access_desc, fdesc_access },	/* access */
	{ &vop_getattr_desc, fdesc_getattr },	/* getattr */
	{ &vop_setattr_desc, fdesc_setattr },	/* setattr */
	{ &vop_read_desc, fdesc_read },		/* read */
	{ &vop_write_desc, fdesc_write },	/* write */
	{ &vop_ioctl_desc, fdesc_ioctl },	/* ioctl */
	{ &vop_select_desc, fdesc_select },	/* select */
	{ &vop_mmap_desc, fdesc_mmap },		/* mmap */
	{ &vop_fsync_desc, fdesc_fsync },	/* fsync */
	{ &vop_seek_desc, fdesc_seek },		/* seek */
	{ &vop_remove_desc, fdesc_remove },	/* remove */
	{ &vop_link_desc, fdesc_link },		/* link */
	{ &vop_rename_desc, fdesc_rename },	/* rename */
	{ &vop_mkdir_desc, fdesc_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, fdesc_rmdir },	/* rmdir */
	{ &vop_symlink_desc, fdesc_symlink },	/* symlink */
	{ &vop_readdir_desc, fdesc_readdir },	/* readdir */
	{ &vop_readlink_desc, fdesc_readlink },	/* readlink */
	{ &vop_abortop_desc, fdesc_abortop },	/* abortop */
	{ &vop_inactive_desc, fdesc_inactive },	/* inactive */
	{ &vop_reclaim_desc, fdesc_reclaim },	/* reclaim */
	{ &vop_lock_desc, fdesc_lock },		/* lock */
	{ &vop_unlock_desc, fdesc_unlock },	/* unlock */
	{ &vop_bmap_desc, fdesc_bmap },		/* bmap */
	{ &vop_strategy_desc, fdesc_strategy },	/* strategy */
	{ &vop_print_desc, fdesc_print },	/* print */
	{ &vop_islocked_desc, fdesc_islocked },	/* islocked */
	{ &vop_pathconf_desc, fdesc_pathconf },	/* pathconf */
	{ &vop_advlock_desc, fdesc_advlock },	/* advlock */
	{ &vop_blkatoff_desc, fdesc_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, fdesc_valloc },	/* valloc */
	{ &vop_vfree_desc, fdesc_vfree },	/* vfree */
	{ &vop_truncate_desc, fdesc_truncate },	/* truncate */
	{ &vop_update_desc, fdesc_update },	/* update */
	{ &vop_bwrite_desc, fdesc_bwrite },	/* bwrite */
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
struct vnodeopv_desc fdesc_vnodeop_opv_desc =
	{ &fdesc_vnodeop_p, fdesc_vnodeop_entries };
