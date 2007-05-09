/*	$OpenBSD: uvm_init.c,v 1.15 2007/05/09 11:23:35 art Exp $	*/
/*	$NetBSD: uvm_init.c,v 1.14 2000/06/27 17:29:23 mrg Exp $	*/

/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * from: Id: uvm_init.c,v 1.1.2.3 1998/02/06 05:15:27 chs Exp
 */

/*
 * uvm_init.c: init the vm system.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/resourcevar.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>

#include <uvm/uvm.h>

/*
 * struct uvm: we store all global vars in this structure to make them
 * easier to spot...
 */

struct uvm uvm;		/* decl */
struct uvmexp uvmexp;	/* decl */

/*
 * local prototypes
 */

/*
 * uvm_init: init the VM system.   called from kern/init_main.c.
 */

void
uvm_init()
{
	vaddr_t kvm_start, kvm_end;

	/*
	 * step 0: ensure that the hardware set the page size
	 */

	if (uvmexp.pagesize == 0) {
		panic("uvm_init: page size not set");
	}

	/*
	 * step 1: zero the uvm structure
	 */

	memset(&uvm, 0, sizeof(uvm));
	averunnable.fscale = FSCALE;

	/*
	 * step 2: init the page sub-system.  this includes allocating the
	 * vm_page structures, and setting up all the page queues (and
	 * locks).  available memory will be put in the "free" queue.
	 * kvm_start and kvm_end will be set to the area of kernel virtual
	 * memory which is available for general use.
	 */

	uvm_page_init(&kvm_start, &kvm_end);

	/*
	 * step 3: init the map sub-system.  allocates the static pool of
	 * vm_map_entry structures that are used for "special" kernel maps
	 * (e.g. kernel_map, kmem_map, etc...).
	 */

	uvm_map_init();

	/*
	 * step 4: setup the kernel's virtual memory data structures.  this
	 * includes setting up the kernel_map/kernel_object and the kmem_map/
	 * kmem_object.
	 */

	uvm_km_init(kvm_start, kvm_end);

	/*
	 * step 5: init the pmap module.   the pmap module is free to allocate
	 * memory for its private use (e.g. pvlists).
	 */

	pmap_init();

	/*
	 * step 6: init the kernel memory allocator.   after this call the
	 * kernel memory allocator (malloc) can be used.
	 */

	kmeminit();

	/*
	 * step 7: init all pagers and the pager_map.
	 */

	uvm_pager_init();

	/*
	 * step 8: init anonymous memory systems (both amap and anons)
	 */

	amap_init();		/* init amap module */
	uvm_anon_init();	/* allocate initial anons */

	/*
	 * the VM system is now up!  now that malloc is up we can resize the
	 * <obj,off> => <page> hash table for general use and enable paging
	 * of kernel objects.
	 */

	uvm_page_rehash();
	uao_create(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS,
	    UAO_FLAG_KERNSWAP);

	uvm_km_page_init();

	/*
	 * reserve some unmapped space for malloc/pool use after free usage
	 */
#ifdef DEADBEEF0
	kvm_start = trunc_page(DEADBEEF0) - PAGE_SIZE;
	if (uvm_map(kernel_map, &kvm_start, 3 * PAGE_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_NONE,
	    UVM_PROT_NONE, UVM_INH_NONE, UVM_ADV_RANDOM, UVM_FLAG_FIXED)))
		panic("uvm_init: cannot reserve dead beef @0x%x\n", DEADBEEF0);
#endif
#ifdef DEADBEEF1
	kvm_start = trunc_page(DEADBEEF1) - PAGE_SIZE;
	if (uvm_map(kernel_map, &kvm_start, 3 * PAGE_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_NONE,
	    UVM_PROT_NONE, UVM_INH_NONE, UVM_ADV_RANDOM, UVM_FLAG_FIXED)))
		panic("uvm_init: cannot reserve dead beef @0x%x\n", DEADBEEF1);
#endif
	/*
	 * done!
	 */

	return;
}
