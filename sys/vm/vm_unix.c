/*	$OpenBSD: vm_unix.c,v 1.9 1998/07/28 00:13:21 millert Exp $	*/
/*	$NetBSD: vm_unix.c,v 1.19 1996/02/10 00:08:14 christos Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * from: Utah $Hdr: vm_unix.c 1.1 89/11/07$
 *
 *	@(#)vm_unix.c	8.2 (Berkeley) 1/9/95
 */

/*
 * Traditional sbrk/grow interface to VM
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/vnode.h>
#include <sys/core.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>

/* ARGSUSED */
int
sys_obreak(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_obreak_args /* {
		syscallarg(char *) nsize;
	} */ *uap = v;
	register struct vmspace *vm = p->p_vmspace;
	vm_offset_t new, old;
	int rv;
	register int diff;

	old = (vm_offset_t)vm->vm_daddr;
	new = (vm_offset_t)SCARG(uap, nsize);

	/* Check for overflow, round to page */
	if(round_page(new) < new)
		return(ENOMEM);
	new = round_page(new);

	/* Check limit */
	if ((new > old) && ((new - old) > p->p_rlimit[RLIMIT_DATA].rlim_cur))
		return(ENOMEM);

	/* Turn the trick */
	old = round_page(old + ctob(vm->vm_dsize));
	diff = new - old;
	if (diff > 0) {
		rv = vm_allocate(&vm->vm_map, &old, diff, FALSE);
		if (rv != KERN_SUCCESS) {
			uprintf("sbrk: grow failed, return = %d\n", rv);
			return(ENOMEM);
		}
		vm->vm_dsize += btoc(diff);
	} else if (diff < 0) {
		diff = -diff;
		rv = vm_deallocate(&vm->vm_map, new, diff);
		if (rv != KERN_SUCCESS) {
			uprintf("sbrk: shrink failed, return = %d\n", rv);
			return(ENOMEM);
		}
		vm->vm_dsize -= btoc(diff);
	}
	return(0);
}

/*
 * Enlarge the "stack segment" to include the specified
 * stack pointer for the process.
 */
int
grow(p, sp)
	struct proc *p;
	vm_offset_t sp;
{
	register struct vmspace *vm = p->p_vmspace;
	register int si;

	/*
	 * For user defined stacks (from sendsig).
	 */
	if (sp < (vm_offset_t)vm->vm_maxsaddr)
		return (0);
	/*
	 * For common case of already allocated (from trap).
	 */
	if (sp >= USRSTACK - ctob(vm->vm_ssize))
		return (1);
	/*
	 * Really need to check vs limit and increment stack size if ok.
	 */
	si = clrnd(btoc(USRSTACK-sp) - vm->vm_ssize);
	if (vm->vm_ssize + si > btoc(p->p_rlimit[RLIMIT_STACK].rlim_cur))
		return (0);
	vm->vm_ssize += si;
	return (1);
}

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

int
vm_coredump(p, vp, cred, chdr)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
	struct core *chdr;
{
	register struct vmspace *vm = p->p_vmspace;
	register vm_map_t	map = &vm->vm_map;
	register vm_map_entry_t	entry;
	vm_offset_t start, end;
	struct coreseg cseg;
	off_t offset;
	int flag, error = 0;

	if (!map->is_main_map) {
#ifdef DEBUG
		uprintf(
	"vm_coredump: %s map %p: pmap=%p, ref=%d, nentries=%d, version=%d\n",
			(map->is_main_map ? "Task" : "Share"),
			map, (map->pmap), map->ref_count, map->nentries,
			map->timestamp);
#endif
		return EIO;
	}

	offset = chdr->c_hdrsize + chdr->c_seghdrsize + chdr->c_cpusize;

	for (entry = map->header.next; entry != &map->header;
	     entry = entry->next) {

		if (entry->is_a_map || entry->is_sub_map) {
#ifdef DEBUG
			uprintf("vm_coredump: entry: share=%p, offset=%p\n",
				entry->object.share_map, entry->offset);
#endif
			continue;
		}

		if (entry->object.vm_object &&
		    entry->object.vm_object->pager &&
		    entry->object.vm_object->pager->pg_type == PG_DEVICE) {
#ifdef DEBUG
			printf("vm_coredump: skipping dev @ 0x%lx\n",
			       entry->start);
#endif
			continue;
		}

		if (!(entry->protection & VM_PROT_WRITE))
			continue;

		start = entry->start;
		end = entry->end;

		if (start >= VM_MAXUSER_ADDRESS)
			continue;

		if (end > VM_MAXUSER_ADDRESS)
			end = VM_MAXUSER_ADDRESS;

		if (start >= (vm_offset_t)vm->vm_maxsaddr) {
			flag = CORE_STACK;
			start = trunc_page(USRSTACK - ctob(vm->vm_ssize));
			if (start >= end)
				continue;
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
		if (error)
			break;

		offset += chdr->c_seghdrsize;
		error = vn_rdwr(UIO_WRITE, vp,
		    (caddr_t)cseg.c_addr, (int)cseg.c_size,
		    offset, UIO_USERSPACE,
		    IO_NODELOCKED|IO_UNIT, cred, NULL, p);
		if (error)
			break;

		offset += cseg.c_size;
		chdr->c_nseg++;
	}

	return error;
}
