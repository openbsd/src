/* $OpenBSD: osf1_file.c,v 1.1 2000/08/04 15:47:55 ericj Exp $ */
/* $NetBSD: osf1_file.c,v 1.6 2000/06/06 19:04:17 soren Exp $ */

/*
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/reboot.h>
#include <sys/syscallargs.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/socketvar.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/wait.h>

#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_syscallargs.h>
#include <compat/osf1/osf1_util.h>
#include <compat/osf1/osf1_cvt.h>

int
osf1_sys_access(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_access_args *uap = v;
	struct sys_access_args a;
	unsigned long leftovers;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	OSF1_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&a, path) = SCARG(uap, path);

	/* translate flags */
	SCARG(&a, flags) = emul_flags_translate(osf1_access_flags_xtab,
	    SCARG(uap, flags), &leftovers);
	if (leftovers != 0)
		return (EINVAL);

	return sys_access(p, &a, retval);
}

int
osf1_sys_execve(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_execve_args *uap = v;
	struct sys_execve_args ap;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	OSF1_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ap, path) = SCARG(uap, path);
	SCARG(&ap, argp) = SCARG(uap, argp);
	SCARG(&ap, envp) = SCARG(uap, envp);

	return sys_execve(p, &ap, retval);
}

/*
 * Get file status; this version does not follow links.
 */
/* ARGSUSED */
int
osf1_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_lstat_args *uap = v;
	struct stat sb;
	struct osf1_stat osb;
	int error;
	struct nameidata nd;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	OSF1_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)))
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	osf1_cvt_stat_from_native(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)SCARG(uap, ub), sizeof (osb));
	return (error);
}

int
osf1_sys_mknod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_mknod_args *uap = v;
	struct sys_mknod_args a;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	OSF1_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&a, path) = SCARG(uap, path);
	SCARG(&a, mode) = SCARG(uap, mode);
	SCARG(&a, dev) = osf1_cvt_dev_to_native(SCARG(uap, dev));

	return sys_mknod(p, &a, retval);
}

int
osf1_sys_open(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_open_args *uap = v;
	struct sys_open_args a;
	char *path;
	caddr_t sg;
	unsigned long leftovers;
#ifdef SYSCALL_DEBUG
	char pnbuf[1024];

	if (scdebug &&
	    copyinstr(SCARG(uap, path), pnbuf, sizeof pnbuf, NULL) == 0)
		printf("osf1_open: open: %s\n", pnbuf);
#endif

	sg = stackgap_init(p->p_emul);

	/* translate flags */
	SCARG(&a, flags) = emul_flags_translate(osf1_open_flags_xtab,
	    SCARG(uap, flags), &leftovers);
	if (leftovers != 0)
		return (EINVAL);

	/* copy mode, no translation necessary */
	SCARG(&a, mode) = SCARG(uap, mode);

	/* pick appropriate path */
	path = SCARG(uap, path);
	if (SCARG(&a, flags) & O_CREAT)
		OSF1_CHECK_ALT_CREAT(p, &sg, path);
	else
		OSF1_CHECK_ALT_EXIST(p, &sg, path);
	SCARG(&a, path) = path;

	return sys_open(p, &a, retval);
}

int
osf1_sys_pathconf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_pathconf_args *uap = v;
	struct sys_pathconf_args a;
	caddr_t sg;
	int error;

	sg = stackgap_init(p->p_emul);

	OSF1_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	SCARG(&a, path) = SCARG(uap, path);

	error = osf1_cvt_pathconf_name_to_native(SCARG(uap, name),
	    &SCARG(&a, name));

	if (error == 0)
		error = sys_pathconf(p, &a, retval);

	return (error);
}

/*
 * Get file status; this version follows links.
 */
/* ARGSUSED */
int
osf1_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_stat_args *uap = v;
	struct stat sb;
	struct osf1_stat osb;
	int error;
	struct nameidata nd;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	OSF1_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)))
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	osf1_cvt_stat_from_native(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)SCARG(uap, ub), sizeof (osb));
	return (error);
}

int
osf1_sys_truncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_truncate_args *uap = v;
	struct sys_truncate_args a;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	OSF1_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&a, path) = SCARG(uap, path);
	SCARG(&a, pad) = 0;
	SCARG(&a, length) = SCARG(uap, length);

	return sys_truncate(p, &a, retval);
}

int
osf1_sys_utimes(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_utimes_args *uap = v;
	struct sys_utimes_args a;
	struct osf1_timeval otv;
	struct timeval tv;
	caddr_t sg;
	int error;

	sg = stackgap_init(p->p_emul);

	OSF1_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	SCARG(&a, path) = SCARG(uap, path);

	error = 0;
	if (SCARG(uap, tptr) == NULL)
		SCARG(&a, tptr) = NULL;
	else {
		SCARG(&a, tptr) = stackgap_alloc(&sg, sizeof tv);

		/* get the OSF/1 timeval argument */
		error = copyin((caddr_t)SCARG(uap, tptr),
		    (caddr_t)&otv, sizeof otv);
		if (error == 0) {

			/* fill in and copy out the NetBSD timeval */
			memset(&tv, 0, sizeof tv);
			tv.tv_sec = otv.tv_sec;
			tv.tv_usec = otv.tv_usec;

			error = copyout((caddr_t)&tv,
			    (caddr_t)SCARG(&a, tptr), sizeof tv);
		}
	}

	if (error == 0)
		error = sys_utimes(p, &a, retval);

	return (error);
}
