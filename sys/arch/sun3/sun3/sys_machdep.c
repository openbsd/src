/*	$OpenBSD: sys_machdep.c,v 1.9 2001/11/06 19:53:16 miod Exp $	*/
/*	$NetBSD: sys_machdep.c,v 1.3 1995/10/27 15:58:23 gwr Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
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
 *
 *	@(#)sys_machdep.c	8.2 (Berkeley) 1/13/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/mtio.h>
#include <sys/buf.h>
#include <sys/mount.h>

#include <uvm/uvm_extern.h>

#include <sys/syscallargs.h>

#include <machine/cpu.h>
#include <machine/machdep.h>

/* XXX should be in an include file somewhere */
#define CC_PURGE	1
#define CC_FLUSH	2
#define CC_IPURGE	4
#define CC_EXTPURGE	0x80000000
/* XXX end should be */

/*ARGSUSED1*/
int
cachectl(req, addr, len)
	int req;
	caddr_t	addr;
	int len;
{
	int error = 0;

	/* XXX: What about our VAC machines? */
	switch (req) {
	case CC_EXTPURGE|CC_PURGE:
	case CC_EXTPURGE|CC_FLUSH:
		/* fall into... */
	case CC_PURGE:
	case CC_FLUSH:
		DCIU();
		break;
	case CC_EXTPURGE|CC_IPURGE:
		DCIU();
		/* fall into... */
	case CC_IPURGE:
		ICIA();
		break;
	default:
		error = EINVAL;
		break;
	}
	return(error);
}

int
sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_sysarch_args /* {
		syscallarg(int) op;
		syscallarg(char *) parms;
	} */ *uap = v;
#endif

	return (ENOSYS);
}
