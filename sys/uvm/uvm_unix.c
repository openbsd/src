/*	$OpenBSD: uvm_unix.c,v 1.30 2007/09/15 10:10:37 martin Exp $	*/
/*	$NetBSD: uvm_unix.c,v 1.18 2000/09/13 15:00:25 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993 The Regents of the University of California.  
 * Copyright (c) 1988 University of Utah.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *      This product includes software developed by Charles D. Cranor,
 *	Washington University, the University of California, Berkeley and 
 *	its contributors.
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
 * from: Utah $Hdr: vm_unix.c 1.1 89/11/07$
 *      @(#)vm_unix.c   8.1 (Berkeley) 6/11/93
 * from: Id: uvm_unix.c,v 1.1.2.2 1997/08/25 18:52:30 chuck Exp
 */

/*
 * uvm_unix.c: traditional sbrk/grow interface to vm.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/vnode.h>
#include <sys/core.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm.h>

/*
 * sys_obreak: set break
 */

int
sys_obreak(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_obreak_args /* {
		syscallarg(char *) nsize;
	} */ *uap = v;
	struct vmspace *vm = p->p_vmspace;
	vaddr_t new, old;
	int error;

	old = (vaddr_t)vm->vm_daddr;
	new = round_page((vaddr_t)SCARG(uap, nsize));
	if ((new - old) > p->p_rlimit[RLIMIT_DATA].rlim_cur)
		return (ENOMEM);

	old = round_page(old + ptoa(vm->vm_dsize));

	if (new == old)
		return (0);

	/*
	 * grow or shrink?
	 */
	if (new > old) {
		error = uvm_map(&vm->vm_map, &old, new - old, NULL,
		    UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RWX, UVM_INH_COPY,
		    UVM_ADV_NORMAL, UVM_FLAG_AMAPPAD|UVM_FLAG_FIXED|
		    UVM_FLAG_OVERLAY|UVM_FLAG_COPYONW));
		if (error) {
			uprintf("sbrk: grow %ld failed, error = %d\n",
			    new - old, error);
			return (ENOMEM);
		}
		vm->vm_dsize += atop(new - old);
	} else {
		uvm_deallocate(&vm->vm_map, new, old - new);
		vm->vm_dsize -= atop(old - new);
	}

	return (0);
}

/*
 * uvm_grow: enlarge the "stack segment" to include sp.
 */

void
uvm_grow(p, sp)
	struct proc *p;
	vaddr_t sp;
{
	struct vmspace *vm = p->p_vmspace;
	int si;

	/*
	 * For user defined stacks (from sendsig).
	 */
	if (sp < (vaddr_t)vm->vm_maxsaddr)
		return;

	/*
	 * For common case of already allocated (from trap).
	 */
#ifdef MACHINE_STACK_GROWS_UP
	if (sp < USRSTACK + ptoa(vm->vm_ssize))
#else
	if (sp >= USRSTACK - ptoa(vm->vm_ssize))
#endif
		return;

	/*
	 * Really need to check vs limit and increment stack size if ok.
	 */
#ifdef MACHINE_STACK_GROWS_UP
	si = atop(sp - USRSTACK) - vm->vm_ssize;
#else
	si = atop(USRSTACK - sp) - vm->vm_ssize;
#endif
	if (vm->vm_ssize + si <= atop(p->p_rlimit[RLIMIT_STACK].rlim_cur))
		vm->vm_ssize += si;
}

/*
 * sys_oadvise: old advice system call
 */

/* ARGSUSED */
int
sys_ovadvise(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_ovadvise_args /* {
		syscallarg(int) anom;
	} */ *uap = v;
#endif

	return (EINVAL);
}

/*
 * uvm_coredump: dump core!
 */

int
uvm_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	struct vmspace *vm = p->p_vmspace;
	vm_map_t map = &vm->vm_map;
	vm_map_entry_t entry;
	vaddr_t start, end;
	struct coreseg cseg;
	off_t offset;
	int flag, error = 0;

	offset = chdr->c_hdrsize + chdr->c_seghdrsize + chdr->c_cpusize;

	for (entry = map->header.next; entry != &map->header;
	    entry = entry->next) {

		/* should never happen for a user process */
		if (UVM_ET_ISSUBMAP(entry)) {
			panic("uvm_coredump: user process with submap?");
		}

		if (!(entry->protection & VM_PROT_WRITE))
			continue;

		/*
		 * Don't dump mmaped devices.
		 */
		if (entry->object.uvm_obj != NULL &&
		    UVM_OBJ_IS_DEVICE(entry->object.uvm_obj))
			continue;

		start = entry->start;
		end = entry->end;

		if (start >= VM_MAXUSER_ADDRESS)
			continue;

		if (end > VM_MAXUSER_ADDRESS)
			end = VM_MAXUSER_ADDRESS;

#ifdef MACHINE_STACK_GROWS_UP
		if (USRSTACK <= start && start < (USRSTACK + MAXSSIZ)) {
			end = round_page(USRSTACK + ptoa(vm->vm_ssize));
			if (start >= end)
				continue;
			start = USRSTACK;
#else
		if (start >= (vaddr_t)vm->vm_maxsaddr) {
			start = trunc_page(USRSTACK - ptoa(vm->vm_ssize));

			if (start >= end)
				continue;
#endif
			flag = CORE_STACK;
		} else
			flag = CORE_DATA;

		/*
		 * Set up a new core file segment.
		 */
		CORE_SETMAGIC(cseg, CORESEGMAGIC, CORE_GETMID(*chdr), flag);
		cseg.c_addr = start;
		cseg.c_size = end - start;

		error = vn_rdwr(UIO_WRITE, vp,
		    (caddr_t)&cseg, chdr->c_seghdrsize,
		    offset, UIO_SYSSPACE,
		    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
		/*
		 * We might get an EFAULT on objects mapped beyond
		 * EOF. Ignore the error.
		 */
		if (error && error != EFAULT)
			break;

		offset += chdr->c_seghdrsize;
		error = vn_rdwr(UIO_WRITE, vp,
		    (caddr_t)(u_long)cseg.c_addr, (int)cseg.c_size,
		    offset, UIO_USERSPACE,
		    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
		if (error)
			break;
		
		offset += cseg.c_size;
		chdr->c_nseg++;
	}

	return (error);
}

