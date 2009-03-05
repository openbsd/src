/*	$OpenBSD: bsdos_exec.c,v 1.5 2009/03/05 19:52:23 kettenis Exp $	*/

/*
 * Copyright (c) 1993, 1994 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/core.h>
#include <sys/exec.h>
#include <sys/resourcevar.h>
#include <uvm/uvm_extern.h>

#if 0
#include <machine/bsdos_machdep.h>
#endif

#include <compat/bsdos/bsdos_exec.h>
#include <compat/bsdos/bsdos_syscall.h>

extern struct sysent bsdos_sysent[];
#ifdef SYSCALL_DEBUG
extern char *bsdos_syscallnames[];
#endif

extern char sigcode[], esigcode[];

struct emul emul_bsdos = {
	"bsdos",
	NULL,
	sendsig,
	BSDOS_SYS_syscall,
	BSDOS_SYS_MAXSYSCALL,
	bsdos_sysent,
#ifdef SYSCALL_DEBUG
	bsdos_syscallnames,
#else
	NULL,
#endif
	0,
	copyargs,
	setregs,
	NULL,
	coredump_trad,
	sigcode,
	esigcode,
};

/*
 * exec_aout_makecmds(): Check if it's an a.out-format executable.
 *
 * Given a proc pointer and an exec package pointer, see if the referent
 * of the epp is in a.out format.  First check 'standard' magic numbers for
 * this architecture.  If that fails, try a cpu-dependent hook.
 *
 * This function, in the former case, or the hook, in the latter, is
 * responsible for creating a set of vmcmds which can be used to build
 * the process's vm space and inserting them into the exec package.
 */

int
exec_bsdos_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	u_long midmag, magic;
	u_short mid;
	int error = ENOEXEC;
	struct exec *execp = epp->ep_hdr;

	if (epp->ep_hdrvalid < sizeof(struct exec))
		return ENOEXEC;

	midmag = ntohl(execp->a_midmag);
	mid = (midmag >> 16) & 0xffff;
	magic = midmag & 0xffff;

	if (magic == 0) {
		magic = (execp->a_midmag & 0xffff);
		mid = MID_BSDOS;
	}

	midmag = mid << 16 | magic;

	switch (midmag) {
	case (MID_BSDOS << 16) | ZMAGIC:
		/*
		 * 386BSD's ZMAGIC format:
		 */
		error = exec_aout_prep_oldzmagic(p, epp);
		break;
	case (MID_BSDOS << 16) | QMAGIC:
		/*
		 * BSDI's QMAGIC format:
		 * same as new ZMAGIC format, but with different magic number.
		 */
		error = exec_aout_prep_zmagic(p, epp);
		break;
	case (MID_BSDOS << 16) | NMAGIC:
		/*
		 * BSDI's NMAGIC format:
		 * same as NMAGIC format, but with different magic number
		 * and with text starting at 0.
		 */
		error = exec_aout_prep_oldnmagic(p, epp);
		break;
	case (MID_BSDOS << 16) | OMAGIC:
		/*
		 * BSDI's OMAGIC format:
		 * same as OMAGIC format, but with different magic number
		 * and with text starting at 0.
		 */
		error = exec_aout_prep_oldomagic(p, epp);
		break;
	}
	if (error == 0)
		epp->ep_emul = &emul_bsdos;
	else
		kill_vmcmds(&epp->ep_vmcmds);

	return error;
}
