/*	$OpenBSD: vax1k_subr.c,v 1.3 2005/11/06 22:21:33 miod Exp $	*/
/*	$NetBSD: vax1k_subr.c,v 1.2 1999/03/24 05:51:20 mrg Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1996 Christopher G. Demetriou
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
#include <sys/filedesc.h>
#include <sys/exec.h>
#include <sys/mman.h>

#include <compat/vax1k/vax1k_exec.h>

#include <uvm/uvm_extern.h>

/*
 * vax1k_map_readvn():
 *	handle vmcmd which specifies that a vnode should be read from.
 *	This is used to be able to read in and execute vax1k binaries
 *	even if the page size is bigger. (cannot mmap).
 */
int
vax1k_map_readvn(p, cmd)
	struct proc *p;
	struct exec_vmcmd *cmd;
{
	vaddr_t oaddr;
	int error;

	if (cmd->ev_len == 0)
		return (0);
	
	oaddr = cmd->ev_addr;
	cmd->ev_addr = trunc_page(cmd->ev_addr); /* required by uvm_map */
	error = uvm_map(&p->p_vmspace->vm_map, &cmd->ev_addr, 
			round_page(cmd->ev_len + (oaddr - cmd->ev_addr)),
			NULL, UVM_UNKNOWN_OFFSET, 0,
			UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_COPY,
			UVM_ADV_NORMAL,
			UVM_FLAG_FIXED|UVM_FLAG_OVERLAY|UVM_FLAG_COPYONW));

	if (error)
		return error;

	error = vn_rdwr(UIO_READ, cmd->ev_vp, (caddr_t)oaddr,
	    cmd->ev_len, cmd->ev_offset, UIO_USERSPACE, IO_UNIT,
	    p->p_ucred, NULL, p);
	if (error)
		return error;

	if (cmd->ev_prot != (VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE)) {
		/*
		 * we had to map in the area at PROT_ALL so that vn_rdwr()
		 * could write to it.   however, the caller seems to want
		 * it mapped read-only, so now we are going to have to call
		 * uvm_map_protect() to fix up the protection.  ICK.
		 */
		return(uvm_map_protect(&p->p_vmspace->vm_map, 
				trunc_page(cmd->ev_addr),
				round_page(cmd->ev_addr + cmd->ev_len),
				cmd->ev_prot, FALSE));
	}

	return (0);
}
