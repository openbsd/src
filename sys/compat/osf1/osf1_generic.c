/* $OpenBSD: osf1_generic.c,v 1.1 2000/08/04 15:47:55 ericj Exp $ */
/* $NetBSD: osf1_generic.c,v 1.1 1999/05/01 05:06:46 cgd Exp $ */

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
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/exec.h>

#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_syscallargs.h>
#include <compat/osf1/osf1_cvt.h>

/*
 * The structures end up being the same... but we can't be sure that
 * the other word of our iov_len is zero!
 */
int
osf1_sys_readv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_readv_args *uap = v;
	struct sys_readv_args a;
	struct osf1_iovec *oio;
	struct iovec *nio;
	caddr_t sg = stackgap_init(p->p_emul);
	int error, osize, nsize, i;

	if (SCARG(uap, iovcnt) > (STACKGAPLEN / sizeof (struct iovec)))
		return (EINVAL);

	osize = SCARG(uap, iovcnt) * sizeof (struct osf1_iovec);
	nsize = SCARG(uap, iovcnt) * sizeof (struct iovec);

	oio = malloc(osize, M_TEMP, M_WAITOK);
	nio = malloc(nsize, M_TEMP, M_WAITOK);

	error = 0;
	if ((error = copyin(SCARG(uap, iovp), oio, osize)))
		goto punt;
	for (i = 0; i < SCARG(uap, iovcnt); i++) {
		nio[i].iov_base = oio[i].iov_base;
		nio[i].iov_len = oio[i].iov_len;
	}

	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, iovp) = stackgap_alloc(&sg, nsize);
	SCARG(&a, iovcnt) = SCARG(uap, iovcnt);

	if ((error = copyout(nio, (caddr_t)SCARG(&a, iovp), nsize)))
		goto punt;
	error = sys_readv(p, &a, retval);

punt:
	free(oio, M_TEMP);
	free(nio, M_TEMP);
	return (error);
}

int
osf1_sys_select(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_select_args *uap = v;
	struct sys_select_args a;
	struct osf1_timeval otv;
	struct timeval tv;
	int error;
	caddr_t sg;

	SCARG(&a, nd) = SCARG(uap, nd);
	SCARG(&a, in) = SCARG(uap, in);
	SCARG(&a, ou) = SCARG(uap, ou);
	SCARG(&a, ex) = SCARG(uap, ex);

	error = 0;
	if (SCARG(uap, tv) == NULL)
		SCARG(&a, tv) = NULL;
	else {
		sg = stackgap_init(p->p_emul);
		SCARG(&a, tv) = stackgap_alloc(&sg, sizeof tv);

		/* get the OSF/1 timeval argument */
		error = copyin((caddr_t)SCARG(uap, tv),
		    (caddr_t)&otv, sizeof otv);
		if (error == 0) {

			/* fill in and copy out the NetBSD timeval */
			memset(&tv, 0, sizeof tv);
			tv.tv_sec = otv.tv_sec;
			tv.tv_usec = otv.tv_usec;

			error = copyout((caddr_t)&tv,
			    (caddr_t)SCARG(&a, tv), sizeof tv);
		}
	}

	if (error == 0)
		error = sys_select(p, &a, retval);

	return (error);
}

int
osf1_sys_writev(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_writev_args *uap = v;
	struct sys_writev_args a;
	struct osf1_iovec *oio;
	struct iovec *nio;
	caddr_t sg = stackgap_init(p->p_emul);
	int error, osize, nsize, i;

	if (SCARG(uap, iovcnt) > (STACKGAPLEN / sizeof (struct iovec)))
		return (EINVAL);

	osize = SCARG(uap, iovcnt) * sizeof (struct osf1_iovec);
	nsize = SCARG(uap, iovcnt) * sizeof (struct iovec);

	oio = malloc(osize, M_TEMP, M_WAITOK);
	nio = malloc(nsize, M_TEMP, M_WAITOK);

	error = 0;
	if ((error = copyin(SCARG(uap, iovp), oio, osize)))
		goto punt;
	for (i = 0; i < SCARG(uap, iovcnt); i++) {
		nio[i].iov_base = oio[i].iov_base;
		nio[i].iov_len = oio[i].iov_len;
	}

	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, iovp) = stackgap_alloc(&sg, nsize);
	SCARG(&a, iovcnt) = SCARG(uap, iovcnt);

	if ((error = copyout(nio, (caddr_t)SCARG(&a, iovp), nsize)))
		goto punt;
	error = sys_writev(p, &a, retval);

punt:
	free(oio, M_TEMP);
	free(nio, M_TEMP);
	return (error);
}
