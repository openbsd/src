/*	$NetBSD: cpu_exec.c,v 1.4 1995/04/25 19:16:46 mellon Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by Ralph
 * Campbell.
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
 *	@(#)machdep.c	8.3 (Berkeley) 1/12/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/resourcevar.h>
#include <vm/vm.h>

#include <sys/exec_ecoff.h>
#ifdef COMPAT_09
#include <machine/bsd-aout.h>
#endif
#include <machine/reg.h>

/*
 * cpu_exec_aout_makecmds():
 *	cpu-dependent a.out format hook for execve().
 * 
 * Determine of the given exec package refers to something which we
 * understand and, if so, set up the vmcmds for it.
 *
 */
int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	/* If COMPAT_09 is defined, allow loading of old-style 4.4bsd a.out
	   executables. */
#ifdef COMPAT_09
	struct bsd_aouthdr *hdr = (struct bsd_aouthdr *)epp -> ep_hdr;

	/* Only handle paged files (laziness). */
	if (hdr -> a_magic != BSD_ZMAGIC)
#endif
		/* If it's not a.out, maybe it's ELF.  (This wants to
		   be moved up to the machine independent code as soon
		   as possible.)  XXX */
		return pmax_elf_makecmds (p, epp);

#ifdef COMPAT_09
	epp -> ep_taddr = 0x1000;
	epp -> ep_entry = hdr -> a_entry;
	epp -> ep_tsize = hdr -> a_text;
	epp -> ep_daddr = epp -> ep_taddr + hdr -> a_text;
	epp -> ep_dsize = hdr -> a_data + hdr -> a_bss;

	/*
	 * check if vnode is in open for writing, because we want to
	 * demand-page out of it.  if it is, don't do it, for various
	 * reasons
	 */
	if ((hdr -> a_text != 0 || hdr -> a_data != 0)
	    && epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	epp->ep_vp->v_flag |= VTEXT;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, hdr -> a_text,
	    epp->ep_taddr, epp->ep_vp, 0, VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, hdr -> a_data,
	    epp->ep_daddr, epp->ep_vp, hdr -> a_text,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, hdr -> a_bss,
	    epp->ep_daddr + hdr -> a_data, NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return exec_aout_setup_stack(p, epp);
#endif
}

#ifdef COMPAT_ULTRIX
extern struct emul emul_ultrix;

void
cpu_exec_ecoff_setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	struct ecoff_aouthdr *eap;

	setregs(p, pack, stack, retval);
	eap = (struct ecoff_aouthdr *)
	    ((caddr_t)pack->ep_hdr + sizeof(struct ecoff_filehdr));
	p->p_md.md_regs[GP] = eap->ea_gp_value;
}

/*
 * cpu_exec_ecoff_hook():
 *	cpu-dependent ECOFF format hook for execve().
 * 
 * Do any machine-dependent diddling of the exec package when doing ECOFF.
 *
 */
int
cpu_exec_ecoff_hook(p, epp, eap)
	struct proc *p;
	struct exec_package *epp;
	struct ecoff_aouthdr *eap;
{

	epp->ep_emul = &emul_ultrix;
	return 0;
}
#endif
