/*	$OpenBSD: pmap_bootstrap.c,v 1.1 2001/11/30 23:14:28 miod Exp $	*/
/*	$NetBSD: pmap.c,v 1.68 1999/06/19 19:44:09 is Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * Copyright (c) 1991 Regents of the University of California.
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
 *	@(#)pmap.c	7.5 (Berkeley) 5/10/91
 */

/*
 *	AMIGA physical map management code, the bootstrap part.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/user.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>
#include <machine/vmparam.h>
#include <amiga/amiga/memlist.h>

extern paddr_t	msgbufpa;	/* physical address of the msgbuf */
extern paddr_t	z2mem_start;

u_long	noncontig_enable;
caddr_t	CADDR1, CADDR2, vmmap;
u_int	*Sysseg_pa;

extern vm_offset_t virtual_avail, virtual_end;
extern st_entry_t *Sysseg;
#if defined(M68040) || defined(M68060)
extern int protostfree;
#endif
extern int protection_codes[];

extern vaddr_t reserve_dumppages __P((vaddr_t));
void amiga_protection_init __P((void));

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	Map the kernel's code and data, and allocate the system page table.
 *
 *	On the HP this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address 0 to the actual (physical)
 *	address of 0xFFxxxxxx.]
 */
void
pmap_bootstrap(firstaddr, loadaddr)
	paddr_t firstaddr;
	paddr_t loadaddr;
{
	vaddr_t va;
	int i;
	struct boot_memseg *sp, *esp;
	paddr_t fromads, toads;

	fromads = firstaddr;
	toads = maxmem << PGSHIFT;

	uvmexp.pagesize = NBPG;
	uvm_setpagesize();

	/* XXX: allow for msgbuf */
	toads -= m68k_round_page(MSGBUFSIZE);
	msgbufpa = toads;

	/*
	 * first segment of memory is always the one loadbsd found
	 * for loading the kernel into.
	 */
	uvm_page_physload(atop(fromads), atop(toads),
		atop(fromads), atop(toads), VM_FREELIST_DEFAULT);

	sp = memlist->m_seg;
	esp = sp + memlist->m_nseg;
	i = 1;
	for (; noncontig_enable && sp < esp; sp++) {
		if ((sp->ms_attrib & MEMF_FAST) == 0)
			continue;		/* skip if not FastMem */
		if (firstaddr >= sp->ms_start &&
		    firstaddr < sp->ms_start + sp->ms_size)
			continue;		/* skip kernel segment */
		if (sp->ms_size == 0)
			continue;		/* skip zero size segments */
		fromads = sp->ms_start;
		toads = sp->ms_start + sp->ms_size;
#ifdef DEBUG_A4000
		/*
		 * My A4000 doesn't seem to like Zorro II memory - this
		 * hack is to skip the motherboard memory and use the
		 * Zorro II memory.  Only for trying to debug the problem.
		 * Michael L. Hitch
		 */
		if (toads == 0x08000000)
			continue;	/* skip A4000 motherboard mem */
#endif
		/*
		 * Deal with Zorro II memory stolen for DMA bounce buffers.
		 * This needs to be handled better.
		 *
		 * XXX is: disabled. This is handled now in amiga_init.c
		 * by removing the stolen memory from the memlist.
		 *
		 * XXX is: enabled again, but check real size and position.
		 * We check z2mem_start is in this segment, and set its end
		 * to the z2mem_start.
		 * 
		 */
		if ((fromads <= z2mem_start) && (toads > z2mem_start))
			toads = z2mem_start;

		uvm_page_physload(atop(fromads), atop(toads),
			atop(fromads), atop(toads), (fromads & 0xff000000) ?
			VM_FREELIST_DEFAULT : VM_FREELIST_ZORROII);
		physmem += (toads - fromads) / NBPG;
		++i;
		if (noncontig_enable == 1)
			break;		/* Only two segments enabled */
	}

	mem_size = physmem << PGSHIFT;
	virtual_avail = VM_MIN_KERNEL_ADDRESS + (firstaddr - loadaddr);
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize protection array.
	 */
	amiga_protection_init();

	/*
	 * Kernel page/segment table allocated in locore,
	 * just initialize pointers.
	 */
	pmap_kernel()->pm_stab = Sysseg;
	pmap_kernel()->pm_ptab = Sysmap;
	simple_lock_init(&pmap_kernel()->pm_lock);
	pmap_kernel()->pm_count = 1;
	pmap_kernel()->pm_stpa = Sysseg_pa;
#if defined(M68040) || defined(M68060)
	if (mmutype <= MMU_68040) {
		pmap_kernel()->pm_stfree = protostfree;
	}
#endif

	/*
	 * Allocate all the submaps we need
	 */
	va = virtual_avail;

	vmmap = (caddr_t)va;
	va += NBPG;

	msgbufp = (struct msgbuf *)va;
	va += btoc(MSGBUFSIZE);
	
	DCIS();
	virtual_avail = reserve_dumppages(va);
}

void
pmap_init_md()
{
	extern vaddr_t	amigahwaddr;
	extern u_int	namigahwpg;
	vaddr_t		addr;

	/*
	 * Now that kernel map has been allocated, we can mark as
	 * unavailable regions which we have mapped in locore.
	 * XXX in pmap_boostrap() ???
	 */
	addr = (vaddr_t) amigahwaddr;
	if (uvm_map(kernel_map, &addr,
		    ptoa(namigahwpg),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE,
				UVM_INH_NONE, UVM_ADV_RANDOM,
				UVM_FLAG_FIXED)))
		panic("pmap_init: bogons in the VM system!\n");
}

void
amiga_protection_init()
{
	int *kp, prot;

	kp = protection_codes;
	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = 0;
			break;
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = PG_RO;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = PG_RW;
			break;
		}
	}
}

