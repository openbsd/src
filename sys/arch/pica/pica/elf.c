/*	$NetBSD: elf.c,v 1.1 1995/01/18 06:16:33 mellon Exp $	*/

/*
 * Copyright (c) 1994 Ted Lemon
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/acct.h>
#include <sys/resourcevar.h>
#include <vm/vm.h>
#include <sys/exec.h>
#include <machine/elf.h>

/* pmax_elf_makecmds (p, epp)

   Test if an executable is a MIPS ELF executable.   If it is,
   try to load it. */

pmax_elf_makecmds (p, epp)
        struct proc *p;
        struct exec_package *epp;
{
	struct ehdr *ex = (struct ehdr *)epp -> ep_hdr;
	struct phdr ph;
	int i, error, resid;

	/* Make sure we got enough data to check magic numbers... */
	if (epp -> ep_hdrvalid < sizeof (struct ehdr)) {
#ifdef DIAGNOSTIC
	    if (epp -> ep_hdrlen < sizeof (struct ehdr))
		printf ("pmax_elf_makecmds: execsw hdrsize too short!\n");
#endif
	    return ENOEXEC;
	}

	/* See if it's got the basic elf magic number leadin... */
	if (ex -> elf_magic [0] != 127
	    || bcmp ("ELF", &ex -> elf_magic [1], 3)) {
		return ENOEXEC;
	}
		/* XXX: Check other magic numbers here. */

		/* See if we got any program header information... */
	if (!ex -> phoff || !ex -> phcount) {
		return ENOEXEC;
	}

	/* Set the entry point... */
	epp -> ep_entry = ex -> entry;

	/*
	 * Check if vnode is open for writing, because we want to
	 * demand-page out of it.  If it is, don't do it.
	 */
	if (epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	epp->ep_vp->v_flag |= VTEXT;

	epp->ep_taddr = 0;
	epp->ep_tsize = 0;
	epp->ep_daddr = 0;
	epp->ep_dsize = 0;

	for (i = 0; i < ex -> phcount; i++) {
		if (error = vn_rdwr(UIO_READ, epp -> ep_vp, (caddr_t)&ph,
				    sizeof ph, ex -> phoff + i * sizeof ph,
				    UIO_SYSSPACE, IO_NODELOCKED,
				    p->p_ucred, &resid, p))
			return error;

		if (resid != 0) {
			return ENOEXEC;
		}

		/* We only care about loadable sections... */
		if (ph.type == PT_LOAD) {
			int prot = VM_PROT_READ | VM_PROT_EXECUTE;
			int residue;
			unsigned vaddr, offset, length;

			vaddr = ph.vaddr;
			offset = ph.offset;
			length = ph.filesz;
			residue = ph.memsz - ph.filesz;

			if (ph.flags & PF_W) {
				prot |= VM_PROT_WRITE;
				if (!epp->ep_daddr || vaddr < epp -> ep_daddr)
					epp->ep_daddr = vaddr;
				epp->ep_dsize += ph.memsz;
				/* Read the data from the file... */
				NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
					  length, vaddr,
					  epp->ep_vp, offset, prot);
				if (residue) {
					vaddr &= ~(NBPG - 1);
					offset &= ~(NBPG - 1);
					length = roundup (length + ph.vaddr
							  - vaddr, NBPG);
					residue = (ph.vaddr + ph.memsz)
						  - (vaddr + length);
				}
			} else {
				vaddr &= ~(NBPG - 1);
				offset &= ~(NBPG - 1);
				length = roundup (length + ph.vaddr - vaddr,
						  NBPG);
				residue = (ph.vaddr + ph.memsz)
					  - (vaddr + length);
				if (!epp->ep_taddr || vaddr < epp -> ep_taddr)
					epp->ep_taddr = vaddr;
				epp->ep_tsize += ph.memsz;
				/* Map the data from the file... */
				NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn,
					  length, vaddr,
					  epp->ep_vp, offset, prot);
			}
			/* If part of the segment is just zeros (e.g., bss),
			   map that. */
			if (residue > 0) {
				NEW_VMCMD (&epp->ep_vmcmds, vmcmd_map_zero,
					   residue, vaddr + length,
					   NULLVP, 0, prot);
			}
		}
	}
 
	epp->ep_maxsaddr = USRSTACK - MAXSSIZ;
	epp->ep_minsaddr = USRSTACK;
	epp->ep_ssize = p->p_rlimit[RLIMIT_STACK].rlim_cur;
 
	/*
	 * set up commands for stack.  note that this takes *two*, one to
	 * map the part of the stack which we can access, and one to map
	 * the part which we can't.
	 *
	 * arguably, it could be made into one, but that would require the
	 * addition of another mapping proc, which is unnecessary
	 *
	 * note that in memory, things assumed to be: 0 ....... ep_maxsaddr
	 * <stack> ep_minsaddr
	 */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero,
	    ((epp->ep_minsaddr - epp->ep_ssize) - epp->ep_maxsaddr),
	    epp->ep_maxsaddr, NULLVP, 0, VM_PROT_NONE);
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, epp->ep_ssize,
	    (epp->ep_minsaddr - epp->ep_ssize), NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
 
	return 0;
}
