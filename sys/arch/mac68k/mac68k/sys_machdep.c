/*	$OpenBSD: sys_machdep.c,v 1.6 2002/03/14 01:26:36 millert Exp $	*/
/*	$NetBSD: sys_machdep.c,v 1.9 1996/05/05 06:18:58 briggs Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
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
/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 *	@(#)sys_machdep.c	7.7 (Berkeley) 5/7/91
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

#include <sys/syscallargs.h>

#include <machine/cpu.h>

/* XXX should be in an include file somewhere */
#define CC_PURGE	1
#define CC_FLUSH	2
#define CC_IPURGE	4
#define CC_EXTPURGE	0x80000000
/* XXX end should be */

int	cachectl(int, caddr_t, int);
void	DCIU(void);
void	ICIA(void);

/*ARGSUSED1*/
int
cachectl(req, addr, len)
	int req;
	caddr_t	addr;
	int len;
{
	int error = 0;

	switch (req) {
	case CC_EXTPURGE|CC_PURGE:
	case CC_EXTPURGE|CC_FLUSH:
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

int sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sysarch_args /* {
		syscallarg(int) op; 
		syscallarg(char *) parms;
	} */ *uap = v;
#endif

	return ENOSYS;
}
