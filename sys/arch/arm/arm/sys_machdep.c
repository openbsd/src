/*	$OpenBSD: sys_machdep.c,v 1.3 2004/05/19 03:17:07 drahn Exp $	*/
/*	$NetBSD: sys_machdep.c,v 1.6 2003/07/15 00:24:42 lukem Exp $	*/

/*
 * Copyright (c) 1995-1997 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * sys_machdep.c
 *
 * Machine dependant syscalls
 *
 * Created      : 10/01/96
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>

#include <machine/sysarch.h>

/* Prototypes */
static int arm32_sync_icache (struct proc *, char *, register_t *);
static int arm32_drain_writebuf (struct proc *, char *, register_t *);

static int
arm32_sync_icache(p, args, retval)
	struct proc *p;
	char *args;
	register_t *retval;
{
	struct arm_sync_icache_args ua;
	int error;

	if ((error = copyin(args, &ua, sizeof(ua))) != 0)
		return (error);

	cpu_icache_sync_range(ua.addr, ua.len);

	*retval = 0;
	return(0);
}

static int
arm32_drain_writebuf(p, args, retval)
	struct proc *p;
	char *args;
	register_t *retval;
{
	/* No args. */

	cpu_drain_writebuf();

	*retval = 0;
	return(0);
}

int
sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sysarch_args /* {
		syscallarg(int) op;
		syscallarg(void *) parms;
	} */ *uap = v;
	int error = 0;

	switch(SCARG(uap, op)) {
	case ARM_SYNC_ICACHE : 
		error = arm32_sync_icache(p, SCARG(uap, parms), retval);
		break;

	case ARM_DRAIN_WRITEBUF : 
		error = arm32_drain_writebuf(p, SCARG(uap, parms), retval);
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}
  
/* End of sys_machdep.c */
