/*	$OpenBSD: compat_exec.c,v 1.7 2001/11/15 06:22:29 art Exp $	*/
/*	$NetBSD: compat_exec.c,v 1.1 1996/05/18 15:52:21 christos Exp $	*/

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
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/resourcevar.h>
#include <uvm/uvm_extern.h>

/*
 * exec_aout_prep_oldzmagic():
 *	Prepare the vmcmds to build a vmspace for an old ZMAGIC
 *	binary. [386BSD/BSDI/4.4BSD/NetBSD0.8]
 *
 * Cloned from exec_aout_prep_zmagic() in kern/exec_aout.c; a more verbose
 * description of operation is there.
 * There were copies of this in the mac68k, hp300, and i386 ports.
 */
int
exec_aout_prep_oldzmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;

	epp->ep_taddr = 0;
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = epp->ep_taddr + execp->a_text;
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

	/*
	 * check if vnode is in open for writing, because we want to
	 * demand-page out of it.  if it is, don't do it, for various
	 * reasons
	 */
	if ((execp->a_text != 0 || execp->a_data != 0) &&
	    epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0");
#endif
		return ETXTBSY;
	}
	vn_marktext(epp->ep_vp);

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_text,
	    epp->ep_taddr, epp->ep_vp, PAGE_SIZE,
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_data,
	    epp->ep_daddr, epp->ep_vp,
	    execp->a_text + PAGE_SIZE,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, execp->a_bss,
	    epp->ep_daddr + execp->a_data, NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return exec_setup_stack(p, epp);
}


/*
 * exec_aout_prep_oldnmagic():
 *	Prepare the vmcmds to build a vmspace for an old NMAGIC
 *	binary. [BSDI]
 *
 * Cloned from exec_aout_prep_nmagic() in kern/exec_aout.c; with text starting
 * at 0.
 * XXX: There must be a better way to share this code.
 */
int
exec_aout_prep_oldnmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;
	long bsize, baddr;

	epp->ep_taddr = 0;
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = roundup(epp->ep_taddr + execp->a_text, __LDPGSZ);
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, execp->a_text,
	    epp->ep_taddr, epp->ep_vp, sizeof(struct exec),
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, execp->a_data,
	    epp->ep_daddr, epp->ep_vp, execp->a_text + sizeof(struct exec),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	baddr = round_page(epp->ep_daddr + execp->a_data);
	bsize = epp->ep_daddr + epp->ep_dsize - baddr;
	if (bsize > 0)
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, bsize, baddr,
		    NULLVP, 0, VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return exec_setup_stack(p, epp);
}


/*
 * exec_aout_prep_oldomagic():
 *	Prepare the vmcmds to build a vmspace for an old OMAGIC
 *	binary. [BSDI]
 *
 * Cloned from exec_aout_prep_omagic() in kern/exec_aout.c; with text starting
 * at 0.
 * XXX: There must be a better way to share this code.
 */
int
exec_aout_prep_oldomagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;
	long dsize, bsize, baddr;

	epp->ep_taddr = 0;
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = epp->ep_taddr + execp->a_text;
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

	/* set up command for text and data segments */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
	    execp->a_text + execp->a_data, epp->ep_taddr, epp->ep_vp,
	    sizeof(struct exec), VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	baddr = round_page(epp->ep_daddr + execp->a_data);
	bsize = epp->ep_daddr + epp->ep_dsize - baddr;
	if (bsize > 0)
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, bsize, baddr,
		    NULLVP, 0, VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/*
	 * Make sure (# of pages) mapped above equals (vm_tsize + vm_dsize);
	 * obreak(2) relies on this fact. Both `vm_tsize' and `vm_dsize' are
	 * computed (in execve(2)) by rounding *up* `ep_tsize' and `ep_dsize'
	 * respectively to page boundaries.
	 * Compensate `ep_dsize' for the amount of data covered by the last
	 * text page. 
	 */
	dsize = epp->ep_dsize + execp->a_text - round_page(execp->a_text);
	epp->ep_dsize = (dsize > 0) ? dsize : 0;
	return exec_setup_stack(p, epp);
}
