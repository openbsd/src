/*	$OpenBSD: freebsd_misc.c,v 1.9 2005/02/19 21:19:28 matthieu Exp $	*/
/*	$NetBSD: freebsd_misc.c,v 1.2 1996/05/03 17:03:10 christos Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
 * All rights reserved.
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * FreeBSD compatibility module. Try to deal with various FreeBSD system calls.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/dirent.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>

#include <sys/syscallargs.h>

#include <compat/freebsd/freebsd_signal.h>
#include <compat/freebsd/freebsd_syscallargs.h>
#include <compat/freebsd/freebsd_util.h>
#include <compat/freebsd/freebsd_rtprio.h>

#include <compat/common/compat_dir.h>

/* just a place holder */

int
freebsd_sys_rtprio(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#ifdef notyet
	struct freebsd_sys_rtprio_args /* {
		syscallarg(int) function;
		syscallarg(pid_t) pid;
		syscallarg(struct freebsd_rtprio *) rtp;
	} */ *uap = v;
#endif

	return ENOSYS;	/* XXX */
}

/*
 * Argh.
 * The syscalls.master mechanism cannot handle a system call that is in
 * two spots in the table.
 */
int
freebsd_sys_poll2(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return (sys_poll(p, v, retval));
}

/*
 * Our madvise is currently dead (always returns EOPNOTSUPP).
 */
int
freebsd_sys_madvise(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return (0);
}


int freebsd_readdir_callback(void *, struct dirent *, off_t);

struct freebsd_readdir_callback_args {
	caddr_t outp;
	int	resid;
};

int 
freebsd_readdir_callback(void *arg, struct dirent *bdp, off_t cookie)
{
	struct freebsd_readdir_callback_args *cb = arg;
	struct dirent idb;
	int error;

	if (cb->resid < bdp->d_reclen)
		return (ENOMEM);
	idb.d_fileno = bdp->d_fileno;
	idb.d_reclen = bdp->d_reclen;
	idb.d_type = bdp->d_type;
	idb.d_namlen = bdp->d_namlen;
	strlcpy(idb.d_name, bdp->d_name, sizeof(idb.d_name));
	
	if ((error = copyout((caddr_t)&idb, cb->outp, bdp->d_reclen)))
		return (error);
	cb->outp += bdp->d_reclen;
	cb->resid -= bdp->d_reclen;

	return (0);
}

int
freebsd_sys_getdents(struct proc *p, void *v, register_t *retval)
{
	struct freebsd_sys_getdents_args /* {
		syscallarg(int) fd;
		syscallarg(void *) dirent;
		syscallarg(unsigned) count;
	} */ *uap = v;
	struct vnode *vp;
	struct file *fp;
	int error;
	struct freebsd_readdir_callback_args args;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0) 
		return (error);
	
	vp = (struct vnode *)fp->f_data;
	
	args.resid = SCARG(uap, count);
	args.outp = (caddr_t)SCARG(uap, dirent);
	
	error = readdir_with_callback(fp, &fp->f_offset, args.resid,
	    freebsd_readdir_callback, &args);
	
	FRELE(fp);
	if (error) 
		return (error);
	
	*retval = SCARG(uap, count) - args.resid;
	return (0);
}
