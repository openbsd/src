/*	$OpenBSD: vax1k_exec.c,v 1.2 2002/03/14 01:26:51 millert Exp $	*/
/*	$NetBSD: vax1k_exec.c,v 1.1 1998/08/21 13:25:47 ragge Exp $	*/

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

/*
 * Exec glue to provide compatibility with older NetBSD vax1k exectuables.
 *
 * Because NetBSD/vax now uses 4k page size, older binaries (that started
 * on an 1k boundary) cannot be mmap'ed. Therefore they are read in 
 * (via vn_rdwr) as OMAGIC binaries and executed. This will use a little
 * bit more memory, but otherwise won't affect the execution speed.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/resourcevar.h>

#include <compat/vax1k/vax1k_exec.h>

int	exec_vax1k_prep_anymagic(struct proc *, struct exec_package *, int);

/*
 * exec_vax1k_makecmds(): Check if it's an a.out-format executable
 * with an vax1k magic number.
 *
 * Given a proc pointer and an exec package pointer, see if the referent
 * of the epp is in a.out format.  Just check 'standard' magic numbers for
 * this architecture.
 *
 * This function, in the former case, or the hook, in the latter, is
 * responsible for creating a set of vmcmds which can be used to build
 * the process's vm space and inserting them into the exec package.
 */

int
exec_vax1k_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	u_long midmag, magic;
	u_short mid;
	int error;
	struct exec *execp = epp->ep_hdr;

	if (epp->ep_hdrvalid < sizeof(struct exec))
		return ENOEXEC;

	midmag = ntohl(execp->a_midmag);
	mid = (midmag >> 16) & 0x3ff;
	magic = midmag & 0xffff;

	midmag = mid << 16 | magic;

	switch (midmag) {
	case (MID_VAX1K << 16) | ZMAGIC:
		error = exec_vax1k_prep_anymagic(p, epp, 0);
		break;

	case (MID_VAX1K << 16) | NMAGIC:
	case (MID_VAX1K << 16) | OMAGIC:
		error = exec_vax1k_prep_anymagic(p, epp, sizeof(struct exec));
		break;

	default:
		error = ENOEXEC;
	}

	if (error)
		kill_vmcmds(&epp->ep_vmcmds);

	return error;
}

/*
 * exec_vax1k_prep_anymagic(): Prepare an vax1k ?MAGIC binary's exec package
 *
 * First, set of the various offsets/lengths in the exec package.
 * Note that all code is mapped RW; no protection, but because it is
 * only used for compatibility it won't hurt.
 *
 */
int
exec_vax1k_prep_anymagic(p, epp, off)
        struct proc *p;
        struct exec_package *epp;
	int off;
{
	long etmp, tmp;
        struct exec *execp = epp->ep_hdr;

        epp->ep_taddr = execp->a_entry & ~(VAX1K_USRTEXT - 1);
	epp->ep_tsize = execp->a_text + execp->a_data;
	epp->ep_daddr = epp->ep_tsize + epp->ep_taddr;
	epp->ep_dsize = execp->a_bss;
        epp->ep_entry = execp->a_entry;

        /* set up command for text segment */
        NEW_VMCMD(&epp->ep_vmcmds, vax1k_map_readvn,
	    epp->ep_tsize,  epp->ep_taddr, epp->ep_vp, off,
	    VM_PROT_WRITE|VM_PROT_READ|VM_PROT_EXECUTE);

	tmp = round_page(epp->ep_daddr);
	etmp = execp->a_bss - (tmp - epp->ep_daddr);

        /* set up command for bss segment */
	if (etmp > 0)
	        NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, etmp, tmp, NULLVP, 0,
		    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

        return exec_setup_stack(p, epp);
}
