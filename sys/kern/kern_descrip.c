/*	$OpenBSD: kern_descrip.c,v 1.60 2002/10/15 01:27:31 nordin Exp $	*/
/*	$NetBSD: kern_descrip.c,v 1.42 1996/03/30 22:24:38 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_descrip.c	8.6 (Berkeley) 4/19/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/ucred.h>
#include <sys/unistd.h>
#include <sys/resourcevar.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/event.h>
#include <sys/pool.h>

#include <uvm/uvm_extern.h>

#include <sys/pipe.h>

/*
 * Descriptor management.
 */
struct filelist filehead;	/* head of list of open files */
int nfiles;			/* actual number of open files */

static __inline void fd_used(struct filedesc *, int);
static __inline void fd_unused(struct filedesc *, int);
static __inline int find_next_zero(u_int *, int, u_int);
int finishdup(struct proc *, struct file *, int, int, register_t *);
int find_last_set(struct filedesc *, int);

struct pool file_pool;
struct pool fdesc_pool;

void
filedesc_init()
{
	pool_init(&file_pool, sizeof(struct file), 0, 0, 0, "filepl",
		&pool_allocator_nointr);
	pool_init(&fdesc_pool, sizeof(struct filedesc0), 0, 0, 0, "fdescpl",
		&pool_allocator_nointr);
	LIST_INIT(&filehead);
}

static __inline int
find_next_zero (u_int *bitmap, int want, u_int bits)
{
	int i, off, maxoff;
	u_int sub;

	if (want > bits)
		return -1;

	off = want >> NDENTRYSHIFT;
	i = want & NDENTRYMASK;
	if (i) {
		sub = bitmap[off] | ((u_int)~0 >> (NDENTRIES - i));
		if (sub != ~0)
			goto found;
		off++;
	}

	maxoff = NDLOSLOTS(bits);
	while (off < maxoff) {
		if ((sub = bitmap[off]) != ~0)
			goto found;
		off++;
	}

	return -1;

 found:
	return (off << NDENTRYSHIFT) + ffs(~sub) - 1;
}

int
find_last_set(struct filedesc *fd, int last)
{
	int off, i;
	struct file **ofiles = fd->fd_ofiles;
	u_int *bitmap = fd->fd_lomap;

	off = (last - 1) >> NDENTRYSHIFT;

	while (!bitmap[off] && off >= 0)
		off--;
	if (off < 0)
		return 0;

	i = ((off + 1) << NDENTRYSHIFT) - 1;
	if (i >= last)
		i = last - 1;

	while (i > 0 && ofiles[i] == NULL)
		i--;
	return i;
}

static __inline void
fd_used(fdp, fd)
	register struct filedesc *fdp;
	register int fd;
{
	u_int off = fd >> NDENTRYSHIFT;

	fdp->fd_lomap[off] |= 1 << (fd & NDENTRYMASK);
	if (fdp->fd_lomap[off] == ~0)
		fdp->fd_himap[off >> NDENTRYSHIFT] |= 1 << (off & NDENTRYMASK);

	if (fd > fdp->fd_lastfile)
		fdp->fd_lastfile = fd;
}

static __inline void
fd_unused(fdp, fd)
	register struct filedesc *fdp;
	register int fd;
{
	u_int off = fd >> NDENTRYSHIFT;

	if (fd < fdp->fd_freefile)
		fdp->fd_freefile = fd;

	if (fdp->fd_lomap[off] == ~0)
		fdp->fd_himap[off >> NDENTRYSHIFT] &= ~(1 << (off & NDENTRYMASK));
	fdp->fd_lomap[off] &= ~(1 << (fd & NDENTRYMASK));

#ifdef DIAGNOSTIC
	if (fd > fdp->fd_lastfile)
		panic("fd_unused: fd_lastfile inconsistent");
#endif
	if (fd == fdp->fd_lastfile)
		fdp->fd_lastfile = find_last_set(fdp, fd);
}

struct file *
fd_getfile(fdp, fd)
	struct filedesc *fdp;
	int fd;
{
	struct file *fp;

	if ((u_int)fd >= fdp->fd_nfiles || (fp = fdp->fd_ofiles[fd]) == NULL)
		return (NULL);

	if (!FILE_IS_USABLE(fp))
		return (NULL);

	return (fp);
}

/*
 * System calls on descriptors.
 */

/*
 * Duplicate a file descriptor.
 */
/* ARGSUSED */
int
sys_dup(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_dup_args /* {
		syscallarg(int) fd;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	int old = SCARG(uap, fd);
	struct file *fp;
	int new;
	int error;

restart:
	if ((fp = fd_getfile(fdp, old)) == NULL)
		return (EBADF);
	FREF(fp);
	if ((error = fdalloc(p, 0, &new)) != 0) {
		FRELE(fp);
		if (error == ENOSPC) {
			fdexpand(p);
			goto restart;
		}
		return (error);
	}
	return (finishdup(p, fp, old, new, retval));
}

/*
 * Duplicate a file descriptor to a particular value.
 */
/* ARGSUSED */
int
sys_dup2(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_dup2_args /* {
		syscallarg(int) from;
		syscallarg(int) to;
	} */ *uap = v;
	int old = SCARG(uap, from), new = SCARG(uap, to);
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	int i, error;

restart:
	if ((fp = fd_getfile(fdp, old)) == NULL)
		return (EBADF);
	if ((u_int)new >= p->p_rlimit[RLIMIT_NOFILE].rlim_cur ||
	    (u_int)new >= maxfiles)
		return (EBADF);
	if (old == new) {
		/*
		 * NOTE! This doesn't clear the close-on-exec flag. This might
		 * or might not be the intended behavior from the start, but
		 * this is what everyone else does.
		 */
		*retval = new;
		return (0);
	}
	FREF(fp);
	if (new >= fdp->fd_nfiles) {
		if ((error = fdalloc(p, new, &i)) != 0) {
			FRELE(fp);
			if (error == ENOSPC) {
				fdexpand(p);
				goto restart;
			}
			return (error);
		}
		if (new != i)
			panic("dup2: fdalloc");
	}
	/* finishdup() does FRELE */
	return (finishdup(p, fp, old, new, retval));
}

/*
 * The file control system call.
 */
/* ARGSUSED */
int
sys_fcntl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fcntl_args /* {
		syscallarg(int) fd;
		syscallarg(int) cmd;
		syscallarg(void *) arg;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	int i, tmp, newmin, flg = F_POSIX;
	struct flock fl;
	int error = 0;

restart:
	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);
	FREF(fp);
	switch (SCARG(uap, cmd)) {

	case F_DUPFD:
		newmin = (long)SCARG(uap, arg);
		if ((u_int)newmin >= p->p_rlimit[RLIMIT_NOFILE].rlim_cur ||
		    (u_int)newmin >= maxfiles) {
			error = EINVAL;
			break;
		}
		if ((error = fdalloc(p, newmin, &i)) != 0) {
			if (error == ENOSPC) {
				fdexpand(p);
				FRELE(fp);
				goto restart;
			}
			break;
		}
		/* finishdup will FRELE for us. */
		return (finishdup(p, fp, fd, i, retval));

	case F_GETFD:
		*retval = fdp->fd_ofileflags[fd] & UF_EXCLOSE ? 1 : 0;
		break;

	case F_SETFD:
		if ((long)SCARG(uap, arg) & 1)
			fdp->fd_ofileflags[fd] |= UF_EXCLOSE;
		else
			fdp->fd_ofileflags[fd] &= ~UF_EXCLOSE;
		break;

	case F_GETFL:
		*retval = OFLAGS(fp->f_flag);
		break;

	case F_SETFL:
		fp->f_flag &= ~FCNTLFLAGS;
		fp->f_flag |= FFLAGS((long)SCARG(uap, arg)) & FCNTLFLAGS;
		tmp = fp->f_flag & FNONBLOCK;
		error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		if (error)
			break;
		tmp = fp->f_flag & FASYNC;
		error = (*fp->f_ops->fo_ioctl)(fp, FIOASYNC, (caddr_t)&tmp, p);
		if (!error)
			break;
		fp->f_flag &= ~FNONBLOCK;
		tmp = 0;
		(void) (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		break;

	case F_GETOWN:
		if (fp->f_type == DTYPE_SOCKET) {
			*retval = ((struct socket *)fp->f_data)->so_pgid;
			break;
		}
		error = (*fp->f_ops->fo_ioctl)
			(fp, TIOCGPGRP, (caddr_t)&tmp, p);
		*retval = -tmp;
		break;

	case F_SETOWN:
		if (fp->f_type == DTYPE_SOCKET) {
			struct socket *so = (struct socket *)fp->f_data;

			so->so_pgid = (long)SCARG(uap, arg);
			so->so_siguid = p->p_cred->p_ruid;
			so->so_sigeuid = p->p_ucred->cr_uid;
			break;
		}
		if ((long)SCARG(uap, arg) <= 0) {
			SCARG(uap, arg) = (void *)(-(long)SCARG(uap, arg));
		} else {
			struct proc *p1 = pfind((long)SCARG(uap, arg));
			if (p1 == 0) {
				error = ESRCH;
				break;
			}
			SCARG(uap, arg) = (void *)(long)p1->p_pgrp->pg_id;
		}
		error = ((*fp->f_ops->fo_ioctl)
			(fp, TIOCSPGRP, (caddr_t)&SCARG(uap, arg), p));
		break;

	case F_SETLKW:
		flg |= F_WAIT;
		/* FALLTHROUGH */

	case F_SETLK:
		if (fp->f_type != DTYPE_VNODE) {
			error = EBADF;
			break;
		}
		vp = (struct vnode *)fp->f_data;
		/* Copy in the lock structure */
		error = copyin((caddr_t)SCARG(uap, arg), (caddr_t)&fl,
		    sizeof (fl));
		if (error)
			break;
		if (fl.l_whence == SEEK_CUR) {
			if (fl.l_start == 0 && fl.l_len < 0) {
				/* lockf(3) compliance hack */
				fl.l_len = -fl.l_len;
				fl.l_start = fp->f_offset - fl.l_len;
			} else
				fl.l_start += fp->f_offset;
		}
		switch (fl.l_type) {

		case F_RDLCK:
			if ((fp->f_flag & FREAD) == 0) {
				error = EBADF;
				goto out;
			}
			p->p_flag |= P_ADVLOCK;
			error = (VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &fl, flg));
			goto out;

		case F_WRLCK:
			if ((fp->f_flag & FWRITE) == 0) {
				error = EBADF;
				goto out;
			}
			p->p_flag |= P_ADVLOCK;
			error = (VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &fl, flg));
			goto out;

		case F_UNLCK:
			error = (VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &fl,
				F_POSIX));
			goto out;

		default:
			error = EINVAL;
			goto out;
		}

	case F_GETLK:
		if (fp->f_type != DTYPE_VNODE) {
			error = EBADF;
			break;
		}
		vp = (struct vnode *)fp->f_data;
		/* Copy in the lock structure */
		error = copyin((caddr_t)SCARG(uap, arg), (caddr_t)&fl,
		    sizeof (fl));
		if (error)
			break;
		if (fl.l_whence == SEEK_CUR) {
			if (fl.l_start == 0 && fl.l_len < 0) {
				/* lockf(3) compliance hack */
				fl.l_len = -fl.l_len;
				fl.l_start = fp->f_offset - fl.l_len;
			} else
				fl.l_start += fp->f_offset;
		}
		if (fl.l_type != F_RDLCK &&
		    fl.l_type != F_WRLCK &&
		    fl.l_type != F_UNLCK &&
		    fl.l_type != 0) {
			error = EINVAL;
			break;
		}
		error = VOP_ADVLOCK(vp, (caddr_t)p, F_GETLK, &fl, F_POSIX);
		if (error)
			break;
		error = (copyout((caddr_t)&fl, (caddr_t)SCARG(uap, arg),
		    sizeof (fl)));
		break;

	default:
		error = EINVAL;
		break;
	}
out:
	FRELE(fp);
	return (error);	
}

/*
 * Common code for dup, dup2, and fcntl(F_DUPFD).
 */
int
finishdup(struct proc *p, struct file *fp, int old, int new, register_t *retval)
{
	struct file *oldfp;
	struct filedesc *fdp = p->p_fd;

	/*
	 * Don't fd_getfile here. We want to closef LARVAL files and
	 * closef can deal with that.
	 */
	oldfp = fdp->fd_ofiles[new];
	if (oldfp != NULL)
		FREF(oldfp);

	if (fp->f_count == LONG_MAX-2) {
		FRELE(fp);
		return (EDEADLK);
	}
	fdp->fd_ofiles[new] = fp;
	fdp->fd_ofileflags[new] = fdp->fd_ofileflags[old] & ~UF_EXCLOSE;
	fp->f_count++;
	FRELE(fp);
	if (oldfp == NULL)
		fd_used(fdp, new);
	*retval = new;

	if (oldfp != NULL) {
		if (new < fdp->fd_knlistsize)
			knote_fdclose(p, new);
		closef(oldfp, p);
	}

	return (0);
}

void
fdremove(fdp, fd)
	struct filedesc *fdp;
	int fd;
{
	fdp->fd_ofiles[fd] = NULL;
	fd_unused(fdp, fd);
}

int
fdrelease(p, fd)
	struct proc *p;
	int fd;
{
	struct filedesc *fdp = p->p_fd;
	struct file **fpp, *fp;

	/*
	 * Don't fd_getfile here. We want to closef LARVAL files and closef
	 * can deal with that.
	 */
	fpp = &fdp->fd_ofiles[fd];
	fp = *fpp;
	if (fp == NULL)
		return (EBADF);
	FREF(fp);
	*fpp = NULL;
	fdp->fd_ofileflags[fd] = 0;
	fd_unused(fdp, fd);
	if (fd < fdp->fd_knlistsize)
		knote_fdclose(p, fd);
	return (closef(fp, p));
}

/*
 * Close a file descriptor.
 */
/* ARGSUSED */
int
sys_close(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_close_args /* {
		syscallarg(int) fd;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	struct filedesc *fdp = p->p_fd;

	if (fd_getfile(fdp, fd) == NULL)
		return (EBADF);
	return (fdrelease(p, fd));
}

/*
 * Return status information about a file descriptor.
 */
/* ARGSUSED */
int
sys_fstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fstat_args /* {
		syscallarg(int) fd;
		syscallarg(struct stat *) sb;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct stat ub;
	int error;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);
	FREF(fp);
	error = (*fp->f_ops->fo_stat)(fp, &ub, p);
	FRELE(fp);
	if (error == 0) {
		/* Don't let non-root see generation numbers
		   (for NFS security) */
		if (suser(p->p_ucred, &p->p_acflag))
			ub.st_gen = 0;
		error = copyout((caddr_t)&ub, (caddr_t)SCARG(uap, sb),
		    sizeof (ub));
	}
	return (error);
}

/*
 * Return pathconf information about a file descriptor.
 */
/* ARGSUSED */
int
sys_fpathconf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fpathconf_args /* {
		syscallarg(int) fd;
		syscallarg(int) name;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	int error;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);
	FREF(fp);
	switch (fp->f_type) {
	case DTYPE_PIPE:
	case DTYPE_SOCKET:
		if (SCARG(uap, name) != _PC_PIPE_BUF) {
			error = EINVAL;
			break;
		}
		*retval = PIPE_BUF;
		error = 0;
		break;
		return (0);

	case DTYPE_VNODE:
		vp = (struct vnode *)fp->f_data;
		error = VOP_PATHCONF(vp, SCARG(uap, name), retval);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	FRELE(fp);
	return (error);
}

/*
 * Allocate a file descriptor for the process.
 */
int
fdalloc(p, want, result)
	struct proc *p;
	int want;
	int *result;
{
	struct filedesc *fdp = p->p_fd;
	int lim, last, i;
	u_int new, off;

	/*
	 * Search for a free descriptor starting at the higher
	 * of want or fd_freefile.  If that fails, consider
	 * expanding the ofile array.
	 */
restart:
	lim = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles);
	last = min(fdp->fd_nfiles, lim);
	if ((i = want) < fdp->fd_freefile)
		i = fdp->fd_freefile;
	off = i >> NDENTRYSHIFT;
	new = find_next_zero(fdp->fd_himap, off,
	    (last + NDENTRIES - 1) >> NDENTRYSHIFT);
	if (new != -1) {
		i = find_next_zero(&fdp->fd_lomap[new], 
				   new > off ? 0 : i & NDENTRYMASK,
				   NDENTRIES);
		if (i == -1) {
			/*
			 * Free file descriptor in this block was
			 * below want, try again with higher want.
			 */
			want = (new + 1) << NDENTRYSHIFT;
			goto restart;
		}
		i += (new << NDENTRYSHIFT);
		if (i < last) {
			fd_used(fdp, i);
			if (want <= fdp->fd_freefile)
				fdp->fd_freefile = i;
			*result = i;
			return (0);
		}
	}
	if (fdp->fd_nfiles >= lim)
		return (EMFILE);

	return (ENOSPC);
}

void
fdexpand(p)
	struct proc *p;
{
	struct filedesc *fdp = p->p_fd;
	int nfiles, i;
	struct file **newofile;
	char *newofileflags;
	u_int *newhimap, *newlomap;

	/*
	 * If it's already expanding just wait for that expansion to finish
	 * and return and let the caller retry the operation.
	 */
	if (lockmgr(&fdp->fd_lock, LK_EXCLUSIVE|LK_SLEEPFAIL, NULL, p))
		return;

	/*
	 * No space in current array.
	 */
	if (fdp->fd_nfiles < NDEXTENT)
		nfiles = NDEXTENT;
	else
		nfiles = 2 * fdp->fd_nfiles;

	newofile = malloc(nfiles * OFILESIZE, M_FILEDESC, M_WAITOK);
	newofileflags = (char *) &newofile[nfiles];

	/*
	 * Copy the existing ofile and ofileflags arrays
	 * and zero the new portion of each array.
	 */
	bcopy(fdp->fd_ofiles, newofile,
		(i = sizeof(struct file *) * fdp->fd_nfiles));
	bzero((char *)newofile + i, nfiles * sizeof(struct file *) - i);
	bcopy(fdp->fd_ofileflags, newofileflags,
		(i = sizeof(char) * fdp->fd_nfiles));
	bzero(newofileflags + i, nfiles * sizeof(char) - i);

	if (fdp->fd_nfiles > NDFILE)
		free(fdp->fd_ofiles, M_FILEDESC);

	if (NDHISLOTS(nfiles) > NDHISLOTS(fdp->fd_nfiles)) {
		newhimap = malloc(NDHISLOTS(nfiles) * sizeof(u_int),
		    M_FILEDESC, M_WAITOK);
		newlomap = malloc(NDLOSLOTS(nfiles) * sizeof(u_int),
		    M_FILEDESC, M_WAITOK);

		bcopy(fdp->fd_himap, newhimap,
		    (i = NDHISLOTS(fdp->fd_nfiles) * sizeof(u_int)));
		bzero((char *)newhimap + i,
		    NDHISLOTS(nfiles) * sizeof(u_int) - i);

		bcopy(fdp->fd_lomap, newlomap,
		    (i = NDLOSLOTS(fdp->fd_nfiles) * sizeof(u_int)));
		bzero((char *)newlomap + i,
		    NDLOSLOTS(nfiles) * sizeof(u_int) - i);

		if (NDHISLOTS(fdp->fd_nfiles) > NDHISLOTS(NDFILE)) {
			free(fdp->fd_himap, M_FILEDESC);
			free(fdp->fd_lomap, M_FILEDESC);
		}
		fdp->fd_himap = newhimap;
		fdp->fd_lomap = newlomap;
	}
	fdp->fd_ofiles = newofile;
	fdp->fd_ofileflags = newofileflags;
	fdp->fd_nfiles = nfiles;	

	lockmgr(&fdp->fd_lock, LK_RELEASE, NULL, p);
}

/*
 * Create a new open file structure and allocate
 * a file descriptor for the process that refers to it.
 */
int
falloc(p, resultfp, resultfd)
	register struct proc *p;
	struct file **resultfp;
	int *resultfd;
{
	register struct file *fp, *fq;
	int error, i;

restart:
	if ((error = fdalloc(p, 0, &i)) != 0) {
		if (error == ENOSPC) {
			fdexpand(p);
			goto restart;
		}
		return (error);
	}
	if (nfiles >= maxfiles) {
		fd_unused(p->p_fd, i);
		tablefull("file");
		return (ENFILE);
	}
	/*
	 * Allocate a new file descriptor.
	 * If the process has file descriptor zero open, add to the list
	 * of open files at that point, otherwise put it at the front of
	 * the list of open files.
	 */
	nfiles++;
	fp = pool_get(&file_pool, PR_WAITOK);
	bzero(fp, sizeof(struct file));
	fp->f_iflags = FIF_LARVAL;
	if ((fq = p->p_fd->fd_ofiles[0]) != NULL) {
		LIST_INSERT_AFTER(fq, fp, f_list);
	} else {
		LIST_INSERT_HEAD(&filehead, fp, f_list);
	}
	p->p_fd->fd_ofiles[i] = fp;
	fp->f_count = 1;
	fp->f_cred = p->p_ucred;
	crhold(fp->f_cred);
	if (resultfp)
		*resultfp = fp;
	if (resultfd)
		*resultfd = i;
	FREF(fp);
	return (0);
}

/*
 * Build a new filedesc structure.
 */
struct filedesc *
fdinit(p)
	struct proc *p;
{
	register struct filedesc0 *newfdp;
	register struct filedesc *fdp = p->p_fd;
	extern int cmask;

	newfdp = pool_get(&fdesc_pool, PR_WAITOK);
	bzero(newfdp, sizeof(struct filedesc0));
	newfdp->fd_fd.fd_cdir = fdp->fd_cdir;
	VREF(newfdp->fd_fd.fd_cdir);
	newfdp->fd_fd.fd_rdir = fdp->fd_rdir;
	if (newfdp->fd_fd.fd_rdir)
		VREF(newfdp->fd_fd.fd_rdir);
	lockinit(&newfdp->fd_fd.fd_lock, PLOCK, "fdexpand", 0, 0);

	/* Create the file descriptor table. */
	newfdp->fd_fd.fd_refcnt = 1;
	newfdp->fd_fd.fd_cmask = cmask;
	newfdp->fd_fd.fd_ofiles = newfdp->fd_dfiles;
	newfdp->fd_fd.fd_ofileflags = newfdp->fd_dfileflags;
	newfdp->fd_fd.fd_nfiles = NDFILE;
	newfdp->fd_fd.fd_himap = newfdp->fd_dhimap;
	newfdp->fd_fd.fd_lomap = newfdp->fd_dlomap;
	newfdp->fd_fd.fd_knlistsize = -1;

	newfdp->fd_fd.fd_freefile = 0;
	newfdp->fd_fd.fd_lastfile = 0;

	return (&newfdp->fd_fd);
}

/*
 * Share a filedesc structure.
 */
struct filedesc *
fdshare(p)
	struct proc *p;
{
	p->p_fd->fd_refcnt++;
	return (p->p_fd);
}

/*
 * Copy a filedesc structure.
 */
struct filedesc *
fdcopy(p)
	struct proc *p;
{
	struct filedesc *newfdp, *fdp = p->p_fd;
	struct file **fpp;
	int i;

	newfdp = pool_get(&fdesc_pool, PR_WAITOK);
	bcopy(fdp, newfdp, sizeof(struct filedesc));
	if (newfdp->fd_cdir)
		VREF(newfdp->fd_cdir);
	if (newfdp->fd_rdir)
		VREF(newfdp->fd_rdir);
	newfdp->fd_refcnt = 1;

	/*
	 * If the number of open files fits in the internal arrays
	 * of the open file structure, use them, otherwise allocate
	 * additional memory for the number of descriptors currently
	 * in use.
	 */
	if (newfdp->fd_lastfile < NDFILE) {
		newfdp->fd_ofiles = ((struct filedesc0 *) newfdp)->fd_dfiles;
		newfdp->fd_ofileflags =
		    ((struct filedesc0 *) newfdp)->fd_dfileflags;
		i = NDFILE;
	} else {
		/*
		 * Compute the smallest multiple of NDEXTENT needed
		 * for the file descriptors currently in use,
		 * allowing the table to shrink.
		 */
		i = newfdp->fd_nfiles;
		while (i >= 2 * NDEXTENT && i > newfdp->fd_lastfile * 2)
			i /= 2;
		newfdp->fd_ofiles = malloc(i * OFILESIZE, M_FILEDESC, M_WAITOK);
		newfdp->fd_ofileflags = (char *) &newfdp->fd_ofiles[i];
	}
	if (NDHISLOTS(i) <= NDHISLOTS(NDFILE)) {
		newfdp->fd_himap =
			((struct filedesc0 *) newfdp)->fd_dhimap;
		newfdp->fd_lomap =
			((struct filedesc0 *) newfdp)->fd_dlomap;
	} else {
		newfdp->fd_himap = malloc(NDHISLOTS(i) * sizeof(u_int),
		    M_FILEDESC, M_WAITOK);
		newfdp->fd_lomap = malloc(NDLOSLOTS(i) * sizeof(u_int),
		    M_FILEDESC, M_WAITOK);
	}
	newfdp->fd_nfiles = i;
	bcopy(fdp->fd_ofiles, newfdp->fd_ofiles, i * sizeof(struct file **));
	bcopy(fdp->fd_ofileflags, newfdp->fd_ofileflags, i * sizeof(char));
	bcopy(fdp->fd_himap, newfdp->fd_himap, NDHISLOTS(i) * sizeof(u_int));
	bcopy(fdp->fd_lomap, newfdp->fd_lomap, NDLOSLOTS(i) * sizeof(u_int));

	/*
	 * kq descriptors cannot be copied.
	 */
	if (newfdp->fd_knlistsize != -1) {
		fpp = newfdp->fd_ofiles;
		for (i = 0; i <= newfdp->fd_lastfile; i++, fpp++)
			if (*fpp != NULL && (*fpp)->f_type == DTYPE_KQUEUE)
				fdremove(newfdp, i);
		newfdp->fd_knlist = NULL;
		newfdp->fd_knlistsize = -1;
		newfdp->fd_knhash = NULL;
		newfdp->fd_knhashmask = 0;
	}

	fpp = newfdp->fd_ofiles;
	for (i = 0; i <= newfdp->fd_lastfile; i++, fpp++)
		if (*fpp != NULL) {
			/*
			 * XXX Gruesome hack. If count gets too high, fail
			 * to copy an fd, since fdcopy()'s callers do not
			 * permit it to indicate failure yet.
			 */
			if ((*fpp)->f_count == LONG_MAX-2)
				fdremove(newfdp, i);
			else
				(*fpp)->f_count++;
		}
	return (newfdp);
}

/*
 * Release a filedesc structure.
 */
void
fdfree(p)
	struct proc *p;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file **fpp, *fp;
	register int i;

	if (--fdp->fd_refcnt > 0)
		return;
	fpp = fdp->fd_ofiles;
	for (i = fdp->fd_lastfile; i >= 0; i--, fpp++) {
		fp = *fpp;
		if (fp != NULL) {
			FREF(fp);
			*fpp = NULL;
			(void) closef(fp, p);
		}
	}
	p->p_fd = NULL;
	if (fdp->fd_nfiles > NDFILE)
		free(fdp->fd_ofiles, M_FILEDESC);
	if (NDHISLOTS(fdp->fd_nfiles) > NDHISLOTS(NDFILE)) {
		free(fdp->fd_himap, M_FILEDESC);
		free(fdp->fd_lomap, M_FILEDESC);
	}
	if (fdp->fd_cdir)
		vrele(fdp->fd_cdir);
	if (fdp->fd_rdir)
		vrele(fdp->fd_rdir);
	if (fdp->fd_knlist)
		FREE(fdp->fd_knlist, M_TEMP);
	if (fdp->fd_knhash)
		FREE(fdp->fd_knhash, M_TEMP);
	pool_put(&fdesc_pool, fdp);
}

/*
 * Internal form of close.
 * Decrement reference count on file structure.
 * Note: p may be NULL when closing a file
 * that was being passed in a message.
 *
 * The fp must have its usecount bumped and will be FRELEd here.
 */
int
closef(struct file *fp, struct proc *p)
{
	struct vnode *vp;
	struct flock lf;
	int error;

	if (fp == NULL)
		return (0);

	/*
	 * POSIX record locking dictates that any close releases ALL
	 * locks owned by this process.  This is handled by setting
	 * a flag in the unlock to free ONLY locks obeying POSIX
	 * semantics, and not to free BSD-style file locks.
	 * If the descriptor was in a message, POSIX-style locks
	 * aren't passed with the descriptor.
	 */
	if (p && (p->p_flag & P_ADVLOCK) && fp->f_type == DTYPE_VNODE) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		lf.l_type = F_UNLCK;
		vp = (struct vnode *)fp->f_data;
		(void) VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &lf, F_POSIX);
	}

	/*
	 * Some files passed to this function could be accessed
	 * without a FILE_IS_USABLE check (and in some cases it's perfectly
	 * legal), we must beware of files where someone already won the
	 * race to FIF_WANTCLOSE.
	 */
	if ((fp->f_iflags & FIF_WANTCLOSE) != 0) {
		FRELE(fp);
		return (0);
	}

	if (--fp->f_count > 0) {
		FRELE(fp);
		return (0);
	}

#ifdef DIAGNOSTIC
	if (fp->f_count < 0)
		panic("closef: count < 0");
#endif

	/* Wait for the last usecount to drain. */
	fp->f_iflags |= FIF_WANTCLOSE;
	while (fp->f_usecount > 1)
		tsleep(&fp->f_usecount, PRIBIO, "closef", 0);

	if ((fp->f_flag & FHASLOCK) && fp->f_type == DTYPE_VNODE) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		lf.l_type = F_UNLCK;
		vp = (struct vnode *)fp->f_data;
		(void) VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK);
	}
	if (fp->f_ops)
		error = (*fp->f_ops->fo_close)(fp, p);
	else
		error = 0;

	/* Free fp */
	LIST_REMOVE(fp, f_list);
	crfree(fp->f_cred);
#ifdef DIAGNOSTIC
	if (fp->f_count != 0 || fp->f_usecount != 1)
		panic("closef: count: %d/%d", fp->f_count, fp->f_usecount);
#endif
	nfiles--;
	pool_put(&file_pool, fp);

	return (error);
}

/*
 * Apply an advisory lock on a file descriptor.
 *
 * Just attempt to get a record lock of the requested type on
 * the entire file (l_whence = SEEK_SET, l_start = 0, l_len = 0).
 */
/* ARGSUSED */
int
sys_flock(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_flock_args /* {
		syscallarg(int) fd;
		syscallarg(int) how;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	int how = SCARG(uap, how);
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	struct flock lf;
	int error;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (EOPNOTSUPP);
	vp = (struct vnode *)fp->f_data;
	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	if (how & LOCK_UN) {
		lf.l_type = F_UNLCK;
		fp->f_flag &= ~FHASLOCK;
		error = VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK);
		goto out;
	}
	if (how & LOCK_EX)
		lf.l_type = F_WRLCK;
	else if (how & LOCK_SH)
		lf.l_type = F_RDLCK;
	else {
		error = EINVAL;
		goto out;
	}
	fp->f_flag |= FHASLOCK;
	if (how & LOCK_NB)
		error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, F_FLOCK);
	else
		error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, F_FLOCK|F_WAIT);
out:
	return (error);
}

/*
 * File Descriptor pseudo-device driver (/dev/fd/).
 *
 * Opening minor device N dup()s the file (if any) connected to file
 * descriptor N belonging to the calling process.  Note that this driver
 * consists of only the ``open()'' routine, because all subsequent
 * references to this file will be direct to the other driver.
 */
/* ARGSUSED */
int
filedescopen(dev, mode, type, p)
	dev_t dev;
	int mode, type;
	struct proc *p;
{

	/*
	 * XXX Kludge: set curproc->p_dupfd to contain the value of the
	 * the file descriptor being sought for duplication. The error
	 * return ensures that the vnode for this device will be released
	 * by vn_open. Open will detect this special error and take the
	 * actions in dupfdopen below. Other callers of vn_open or VOP_OPEN
	 * will simply report the error.
	 */
	p->p_dupfd = minor(dev);
	return (ENODEV);
}

/*
 * Duplicate the specified descriptor to a free descriptor.
 */
int
dupfdopen(fdp, indx, dfd, mode, error)
	struct filedesc *fdp;
	int indx, dfd;
	int mode;
	int error;
{
	struct file *wfp;

	/*
	 * If the to-be-dup'd fd number is greater than the allowed number
	 * of file descriptors, or the fd to be dup'd has already been
	 * closed, reject. Note, there is no need to check for new == old
	 * because fd_getfile will return NULL if the file at indx is
	 * newly created by falloc (FIF_LARVAL).
	 */
	if ((wfp = fd_getfile(fdp, dfd)) == NULL)
		return (EBADF);

	/*
	 * There are two cases of interest here.
	 *
	 * For ENODEV simply dup (dfd) to file descriptor
	 * (indx) and return.
	 *
	 * For ENXIO steal away the file structure from (dfd) and
	 * store it in (indx).  (dfd) is effectively closed by
	 * this operation.
	 *
	 * Any other error code is just returned.
	 */
	switch (error) {
	case ENODEV:
		/*
		 * Check that the mode the file is being opened for is a
		 * subset of the mode of the existing descriptor.
		 */
		if (((mode & (FREAD|FWRITE)) | wfp->f_flag) != wfp->f_flag)
			return (EACCES);
		if (wfp->f_count == LONG_MAX-2)
			return (EDEADLK);
		fdp->fd_ofiles[indx] = wfp;
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		wfp->f_count++;
		fd_used(fdp, indx);
		return (0);

	case ENXIO:
		/*
		 * Steal away the file pointer from dfd, and stuff it into indx.
		 */
		fdp->fd_ofiles[indx] = fdp->fd_ofiles[dfd];
		fdp->fd_ofileflags[indx] = fdp->fd_ofileflags[dfd];
		fdp->fd_ofiles[dfd] = NULL;
		fdp->fd_ofileflags[dfd] = 0;
		/*
		 * Complete the clean up of the filedesc structure by
		 * recomputing the various hints.
		 */
		fd_used(fdp, indx);
		fd_unused(fdp, dfd);
		return (0);

	default:
		return (error);
	}
	/* NOTREACHED */
}

/*
 * Close any files on exec?
 */
void
fdcloseexec(p)
	struct proc *p;
{
	register struct filedesc *fdp = p->p_fd;
	register int fd;

	for (fd = 0; fd <= fdp->fd_lastfile; fd++)
		if (fdp->fd_ofileflags[fd] & UF_EXCLOSE)
			(void) fdrelease(p, fd);
}
