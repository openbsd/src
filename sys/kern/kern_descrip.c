/*	$OpenBSD: kern_descrip.c,v 1.20 2000/04/01 23:29:25 provos Exp $	*/
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

#include <vm/vm.h>

#include <sys/pipe.h>

/*
 * Descriptor management.
 */
struct filelist filehead;	/* head of list of open files */
int nfiles;			/* actual number of open files */

static __inline void fd_used __P((struct filedesc *, int));
static __inline void fd_unused __P((struct filedesc *, int));
static __inline int find_next_zero __P((u_int *, int, u_int));
int finishdup __P((struct filedesc *, int, int, register_t *));
int find_last_set __P((struct filedesc *, int));

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
		syscallarg(u_int) fd;
	} */ *uap = v;
	register struct filedesc *fdp = p->p_fd;
	register int old = SCARG(uap, fd);
	int new;
	int error;

	if ((u_int)old >= fdp->fd_nfiles || fdp->fd_ofiles[old] == NULL)
		return (EBADF);
	if ((error = fdalloc(p, 0, &new)) != 0)
		return (error);
	return (finishdup(fdp, old, new, retval));
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
		syscallarg(u_int) from;
		syscallarg(u_int) to;
	} */ *uap = v;
	register struct filedesc *fdp = p->p_fd;
	register int old = SCARG(uap, from), new = SCARG(uap, to);
	int i, error;

	if ((u_int)old >= fdp->fd_nfiles || fdp->fd_ofiles[old] == NULL ||
	    (u_int)new >= p->p_rlimit[RLIMIT_NOFILE].rlim_cur ||
	    (u_int)new >= maxfiles)
		return (EBADF);
	if (old == new) {
		*retval = new;
		return (0);
	}
	if (new >= fdp->fd_nfiles) {
		if ((error = fdalloc(p, new, &i)) != 0)
			return (error);
		if (new != i)
			panic("dup2: fdalloc");
	} else {
		(void) fdrelease(p, new);
	}
	return (finishdup(fdp, old, new, retval));
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
	register struct sys_fcntl_args /* {
		syscallarg(int) fd;
		syscallarg(int) cmd;
		syscallarg(void *) arg;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	int i, tmp, error, flg = F_POSIX;
	struct flock fl;
	int newmin;

	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	switch (SCARG(uap, cmd)) {

	case F_DUPFD:
		newmin = (long)SCARG(uap, arg);
		if ((u_int)newmin >= p->p_rlimit[RLIMIT_NOFILE].rlim_cur ||
		    (u_int)newmin >= maxfiles)
			return (EINVAL);
		if ((error = fdalloc(p, newmin, &i)) != 0)
			return (error);
		return (finishdup(fdp, fd, i, retval));

	case F_GETFD:
		*retval = fdp->fd_ofileflags[fd] & UF_EXCLOSE ? 1 : 0;
		return (0);

	case F_SETFD:
		if ((long)SCARG(uap, arg) & 1)
			fdp->fd_ofileflags[fd] |= UF_EXCLOSE;
		else
			fdp->fd_ofileflags[fd] &= ~UF_EXCLOSE;
		return (0);

	case F_GETFL:
		*retval = OFLAGS(fp->f_flag);
		return (0);

	case F_SETFL:
		fp->f_flag &= ~FCNTLFLAGS;
		fp->f_flag |= FFLAGS((long)SCARG(uap, arg)) & FCNTLFLAGS;
		tmp = fp->f_flag & FNONBLOCK;
		error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		if (error)
			return (error);
		tmp = fp->f_flag & FASYNC;
		error = (*fp->f_ops->fo_ioctl)(fp, FIOASYNC, (caddr_t)&tmp, p);
		if (!error)
			return (0);
		fp->f_flag &= ~FNONBLOCK;
		tmp = 0;
		(void) (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		return (error);

	case F_GETOWN:
		if (fp->f_type == DTYPE_SOCKET) {
			*retval = ((struct socket *)fp->f_data)->so_pgid;
			return (0);
		}
		error = (*fp->f_ops->fo_ioctl)
			(fp, TIOCGPGRP, (caddr_t)retval, p);
		*retval = -*retval;
		return (error);

	case F_SETOWN:
		if (fp->f_type == DTYPE_SOCKET) {
			struct socket *so = (struct socket *)fp->f_data;

			so->so_pgid = (long)SCARG(uap, arg);
			so->so_siguid = p->p_cred->p_ruid;
			so->so_sigeuid = p->p_ucred->cr_uid;
			return (0);
		}
		if ((long)SCARG(uap, arg) <= 0) {
			SCARG(uap, arg) = (void *)(-(long)SCARG(uap, arg));
		} else {
			struct proc *p1 = pfind((long)SCARG(uap, arg));
			if (p1 == 0)
				return (ESRCH);
			SCARG(uap, arg) = (void *)(long)p1->p_pgrp->pg_id;
		}
		return ((*fp->f_ops->fo_ioctl)
			(fp, TIOCSPGRP, (caddr_t)&SCARG(uap, arg), p));

	case F_SETLKW:
		flg |= F_WAIT;
		/* Fall into F_SETLK */

	case F_SETLK:
		if (fp->f_type != DTYPE_VNODE)
			return (EBADF);
		vp = (struct vnode *)fp->f_data;
		/* Copy in the lock structure */
		error = copyin((caddr_t)SCARG(uap, arg), (caddr_t)&fl,
		    sizeof (fl));
		if (error)
			return (error);
		if (fl.l_whence == SEEK_CUR)
			fl.l_start += fp->f_offset;
		switch (fl.l_type) {

		case F_RDLCK:
			if ((fp->f_flag & FREAD) == 0)
				return (EBADF);
			p->p_flag |= P_ADVLOCK;
			return (VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &fl, flg));

		case F_WRLCK:
			if ((fp->f_flag & FWRITE) == 0)
				return (EBADF);
			p->p_flag |= P_ADVLOCK;
			return (VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &fl, flg));

		case F_UNLCK:
			return (VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &fl,
				F_POSIX));

		default:
			return (EINVAL);
		}

	case F_GETLK:
		if (fp->f_type != DTYPE_VNODE)
			return (EBADF);
		vp = (struct vnode *)fp->f_data;
		/* Copy in the lock structure */
		error = copyin((caddr_t)SCARG(uap, arg), (caddr_t)&fl,
		    sizeof (fl));
		if (error)
			return (error);
		if (fl.l_whence == SEEK_CUR)
			fl.l_start += fp->f_offset;
		else if (fl.l_whence != SEEK_END &&
			 fl.l_whence != SEEK_SET &&
			 fl.l_whence != 0)
			return (EINVAL);
		if (fl.l_start < 0)
			return (EINVAL);
		if (fl.l_type != F_RDLCK &&
		    fl.l_type != F_WRLCK &&
		    fl.l_type != F_UNLCK &&
		    fl.l_type != 0)
			return (EINVAL);
		error = VOP_ADVLOCK(vp, (caddr_t)p, F_GETLK, &fl, F_POSIX);
		if (error)
			return (error);
		return (copyout((caddr_t)&fl, (caddr_t)SCARG(uap, arg),
		    sizeof (fl)));

	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Common code for dup, dup2, and fcntl(F_DUPFD).
 */
int
finishdup(fdp, old, new, retval)
	register struct filedesc *fdp;
	register int old, new;
	register_t *retval;
{
	register struct file *fp;

	fp = fdp->fd_ofiles[old];
	if (fp->f_count == LONG_MAX-2)
		return (EDEADLK);
	fdp->fd_ofiles[new] = fp;
	fdp->fd_ofileflags[new] = fdp->fd_ofileflags[old] &~ UF_EXCLOSE;
	fp->f_count++;
	fd_used(fdp, new);
	*retval = new;
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
	register struct filedesc *fdp = p->p_fd;
	register struct file **fpp, *fp;
	register char *pf;

	fpp = &fdp->fd_ofiles[fd];
	fp = *fpp;
	if (fp == NULL)
		return (EBADF);
	pf = &fdp->fd_ofileflags[fd];
#if defined(UVM)
	if (*pf & UF_MAPPED) {
		/* XXX: USELESS? XXXCDC check it */
		p->p_fd->fd_ofileflags[fd] &= ~UF_MAPPED;
	}
#else
	if (*pf & UF_MAPPED)
		(void) munmapfd(p, fd);
#endif
	*fpp = NULL;
	*pf = 0;
	fd_unused(fdp, fd);
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
	register struct filedesc *fdp = p->p_fd;

	if ((u_int)fd >= fdp->fd_nfiles)
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
	register struct sys_fstat_args /* {
		syscallarg(int) fd;
		syscallarg(struct stat *) sb;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct stat ub;
	int error;

	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	switch (fp->f_type) {

	case DTYPE_VNODE:
		error = vn_stat((struct vnode *)fp->f_data, &ub, p);
		break;

	case DTYPE_SOCKET:
		error = soo_stat((struct socket *)fp->f_data, &ub);
		break;

#ifndef OLD_PIPE
	case DTYPE_PIPE:
		error = pipe_stat((struct pipe *)fp->f_data, &ub);
		break;
#endif

	default:
		panic("fstat");
		/*NOTREACHED*/
	}
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
	register struct sys_fpathconf_args /* {
		syscallarg(int) fd;
		syscallarg(int) name;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;

	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	switch (fp->f_type) {

#ifndef OLD_PIPE
	case DTYPE_PIPE:
#endif
	case DTYPE_SOCKET:
		if (SCARG(uap, name) != _PC_PIPE_BUF)
			return (EINVAL);
		*retval = PIPE_BUF;
		return (0);

	case DTYPE_VNODE:
		vp = (struct vnode *)fp->f_data;
		return (VOP_PATHCONF(vp, SCARG(uap, name), retval));

	default:
		panic("fpathconf");
	}
	/*NOTREACHED*/
}

/*
 * Allocate a file descriptor for the process.
 */
int fdexpand;

int
fdalloc(p, want, result)
	struct proc *p;
	int want;
	int *result;
{
	register struct filedesc *fdp = p->p_fd;
	register int i;
	int lim, last, nfiles;
	struct file **newofile;
	char *newofileflags;
	u_int *newhimap, *newlomap, new, off;

	/*
	 * Search for a free descriptor starting at the higher
	 * of want or fd_freefile.  If that fails, consider
	 * expanding the ofile array.
	 */
	lim = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles);
	for (;;) {
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
				/* free file descriptor in this block was
				 * below want, try again with higher want.
				 */
				want = (new + 1) << NDENTRYSHIFT;
				continue;
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

		/*
		 * No space in current array.  Expand?
		 */
		if (fdp->fd_nfiles >= lim)
			return (EMFILE);
		if (fdp->fd_nfiles < NDEXTENT)
			nfiles = NDEXTENT;
		else
			nfiles = 2 * fdp->fd_nfiles;
		nfiles = min(lim, nfiles);
		MALLOC(newofile, struct file **, nfiles * OFILESIZE,
		    M_FILEDESC, M_WAITOK);
		newofileflags = (char *) &newofile[nfiles];

		MALLOC(newhimap, u_int *, NDHISLOTS(nfiles) * sizeof(u_int),
		       M_FILEDESC, M_WAITOK);
		MALLOC(newlomap, u_int *, NDLOSLOTS(nfiles) * sizeof(u_int),
		       M_FILEDESC, M_WAITOK);
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

		bcopy(fdp->fd_himap, newhimap,
		      (i = NDHISLOTS(fdp->fd_nfiles) * sizeof(u_int)));
		bzero((char *)newhimap + i,
		      NDHISLOTS(nfiles) * sizeof(u_int) - i);

		bcopy(fdp->fd_lomap, newlomap,
		      (i = NDLOSLOTS(fdp->fd_nfiles) * sizeof(u_int)));
		bzero((char *)newlomap + i,
		      NDLOSLOTS(nfiles) * sizeof(u_int) - i);

		if (fdp->fd_nfiles > NDFILE)
			FREE(fdp->fd_ofiles, M_FILEDESC);
		if (NDHISLOTS(fdp->fd_nfiles) > NDHISLOTS(NDFILE)) {
			FREE(fdp->fd_himap, M_FILEDESC);
			FREE(fdp->fd_lomap, M_FILEDESC);
		}
		fdp->fd_ofiles = newofile;
		fdp->fd_ofileflags = newofileflags;
		fdp->fd_nfiles = nfiles;
		fdp->fd_himap = newhimap;
		fdp->fd_lomap = newlomap;
		fdexpand++;
	}
}

/*
 * Check to see whether n user file descriptors
 * are available to the process p.
 */
int
fdavail(p, n)
	struct proc *p;
	register int n;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file **fpp;
	register int i, lim;

	lim = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles);
	if ((i = lim - fdp->fd_nfiles) > 0 && (n -= i) <= 0)
		return (1);
	fpp = &fdp->fd_ofiles[fdp->fd_freefile];
	for (i = min(lim, fdp->fd_nfiles) - fdp->fd_freefile; --i >= 0; fpp++)
		if (*fpp == NULL && --n <= 0)
			return (1);
	return (0);
}

/*
 * Create a new open file structure and allocate
 * a file decriptor for the process that refers to it.
 */
int
falloc(p, resultfp, resultfd)
	register struct proc *p;
	struct file **resultfp;
	int *resultfd;
{
	register struct file *fp, *fq;
	int error, i;

	if ((error = fdalloc(p, 0, &i)) != 0)
		return (error);
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
	MALLOC(fp, struct file *, sizeof(struct file), M_FILE, M_WAITOK);
	bzero(fp, sizeof(struct file));
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
	return (0);
}

/*
 * Free a file descriptor.
 */
void
ffree(fp)
	register struct file *fp;
{
	LIST_REMOVE(fp, f_list);
	crfree(fp->f_cred);
#ifdef DIAGNOSTIC
	fp->f_count = 0;
#endif
	nfiles--;
	FREE(fp, M_FILE);
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

	MALLOC(newfdp, struct filedesc0 *, sizeof(struct filedesc0),
	    M_FILEDESC, M_WAITOK);
	bzero(newfdp, sizeof(struct filedesc0));
	newfdp->fd_fd.fd_cdir = fdp->fd_cdir;
	VREF(newfdp->fd_fd.fd_cdir);
	newfdp->fd_fd.fd_rdir = fdp->fd_rdir;
	if (newfdp->fd_fd.fd_rdir)
		VREF(newfdp->fd_fd.fd_rdir);

	/* Create the file descriptor table. */
	newfdp->fd_fd.fd_refcnt = 1;
	newfdp->fd_fd.fd_cmask = cmask;
	newfdp->fd_fd.fd_ofiles = newfdp->fd_dfiles;
	newfdp->fd_fd.fd_ofileflags = newfdp->fd_dfileflags;
	newfdp->fd_fd.fd_nfiles = NDFILE;
	newfdp->fd_fd.fd_himap = newfdp->fd_dhimap;
	newfdp->fd_fd.fd_lomap = newfdp->fd_dlomap;

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
	register struct filedesc *newfdp, *fdp = p->p_fd;
	register struct file **fpp;
	register int i;

	MALLOC(newfdp, struct filedesc *, sizeof(struct filedesc0),
	    M_FILEDESC, M_WAITOK);
	bcopy(fdp, newfdp, sizeof(struct filedesc));
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
		MALLOC(newfdp->fd_ofiles, struct file **, i * OFILESIZE,
		    M_FILEDESC, M_WAITOK);
		newfdp->fd_ofileflags = (char *) &newfdp->fd_ofiles[i];
	}
	if (NDHISLOTS(i) <= NDHISLOTS(NDFILE)) {
		newfdp->fd_himap =
			((struct filedesc0 *) newfdp)->fd_dhimap;
		newfdp->fd_lomap =
			((struct filedesc0 *) newfdp)->fd_dlomap;
	} else {
		MALLOC(newfdp->fd_himap, u_int *, NDHISLOTS(i) * sizeof(u_int),
		       M_FILEDESC, M_WAITOK);
		MALLOC(newfdp->fd_lomap, u_int *, NDLOSLOTS(i) * sizeof(u_int),
		       M_FILEDESC, M_WAITOK);
	}
	newfdp->fd_nfiles = i;
	bcopy(fdp->fd_ofiles, newfdp->fd_ofiles, i * sizeof(struct file **));
	bcopy(fdp->fd_ofileflags, newfdp->fd_ofileflags, i * sizeof(char));
	bcopy(fdp->fd_himap, newfdp->fd_himap, NDHISLOTS(i) * sizeof(u_int));
	bcopy(fdp->fd_lomap, newfdp->fd_lomap, NDLOSLOTS(i) * sizeof(u_int));
	fpp = newfdp->fd_ofiles;
	for (i = newfdp->fd_lastfile; i >= 0; i--, fpp++)
		if (*fpp != NULL) {
			/*
			 * XXX Gruesome hack. If count gets too high, fail
			 * to copy an fd, since fdcopy()'s callers do not
			 * permit it to indicate failure yet.
			 */
			if ((*fpp)->f_count == LONG_MAX-2)
				*fpp = NULL;
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
			*fpp = NULL;
			(void) closef(fp, p);
		}
	}
	p->p_fd = NULL;
	if (fdp->fd_nfiles > NDFILE)
		FREE(fdp->fd_ofiles, M_FILEDESC);
	if (NDHISLOTS(fdp->fd_nfiles) > NDHISLOTS(NDFILE)) {
		FREE(fdp->fd_himap, M_FILEDESC);
		FREE(fdp->fd_lomap, M_FILEDESC);
	}
	vrele(fdp->fd_cdir);
	if (fdp->fd_rdir)
		vrele(fdp->fd_rdir);
	FREE(fdp, M_FILEDESC);
}

/*
 * Internal form of close.
 * Decrement reference count on file structure.
 * Note: p may be NULL when closing a file
 * that was being passed in a message.
 */
int
closef(fp, p)
	register struct file *fp;
	register struct proc *p;
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
	if (--fp->f_count > 0)
		return (0);
	if (fp->f_count < 0)
		panic("closef: count < 0");
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
	ffree(fp);
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
	register struct sys_flock_args /* {
		syscallarg(int) fd;
		syscallarg(int) how;
	} */ *uap = v;
	int fd = SCARG(uap, fd);
	int how = SCARG(uap, how);
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vnode *vp;
	struct flock lf;

	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
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
		return (VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK));
	}
	if (how & LOCK_EX)
		lf.l_type = F_WRLCK;
	else if (how & LOCK_SH)
		lf.l_type = F_RDLCK;
	else
		return (EINVAL);
	fp->f_flag |= FHASLOCK;
	if (how & LOCK_NB)
		return (VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, F_FLOCK));
	return (VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, F_FLOCK|F_WAIT));
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
	register struct filedesc *fdp;
	register int indx, dfd;
	int mode;
	int error;
{
	register struct file *wfp;
	struct file *fp;

	/*
	 * If the to-be-dup'd fd number is greater than the allowed number
	 * of file descriptors, or the fd to be dup'd has already been
	 * closed, reject.  Note, check for new == old is necessary as
	 * falloc could allocate an already closed to-be-dup'd descriptor
	 * as the new descriptor.
	 */
	fp = fdp->fd_ofiles[indx];
	if ((u_int)dfd >= fdp->fd_nfiles ||
	    (wfp = fdp->fd_ofiles[dfd]) == NULL || fp == wfp)
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
