/*	$OpenBSD: uvm_io.c,v 1.2 1999/02/26 05:32:07 art Exp $	*/
/*	$NetBSD: uvm_io.c,v 1.7 1998/10/11 23:18:20 chuck Exp $	*/

/*
 * XXXCDC: "ROUGH DRAFT" QUALITY UVM PRE-RELEASE FILE!   
 *	   >>>USE AT YOUR OWN RISK, WORK IS NOT FINISHED<<<
 */
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
 * from: Id: uvm_io.c,v 1.1.2.2 1997/12/30 12:02:00 mrg Exp
 */

/*
 * uvm_io.c: uvm i/o ops
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <uvm/uvm.h>

/*
 * functions
 */

/*
 * uvm_io: perform I/O on a map
 *
 * => caller must have a reference to "map" so that it doesn't go away
 *    while we are working.
 */

int
uvm_io(map, uio)
	vm_map_t map;
	struct uio *uio;
{
	vaddr_t baseva, endva, pageoffset, kva;
	vsize_t chunksz, togo, sz;
	vm_map_entry_t dead_entries;
	int error;

	/*
	 * step 0: sanity checks and set up for copy loop.  start with a
	 * large chunk size.  if we have trouble finding vm space we will
	 * reduce it.
	 */

	if (uio->uio_resid == 0)
		return(0);
	togo = uio->uio_resid;

	baseva = (vaddr_t) uio->uio_offset;
	endva = baseva + (togo - 1);

	if (endva < baseva)   /* wrap around? */
		return(EIO);

	if (baseva >= VM_MAXUSER_ADDRESS)
		return(0);
	if (endva >= VM_MAXUSER_ADDRESS)
		/* EOF truncate */
		togo = togo - (endva - VM_MAXUSER_ADDRESS + 1);
	pageoffset = baseva & PAGE_MASK;
	baseva = trunc_page(baseva);
	chunksz = min(round_page(togo + pageoffset), MAXBSIZE);
	error = 0;

	/*
	 * step 1: main loop...  while we've got data to move
	 */

	for (/*null*/; togo > 0 ; pageoffset = 0) {

		/*
		 * step 2: extract mappings from the map into kernel_map
		 */

		error = uvm_map_extract(map, baseva, chunksz, kernel_map, &kva,
			    UVM_EXTRACT_QREF | UVM_EXTRACT_CONTIG | 
			    UVM_EXTRACT_FIXPROT);
		if (error) {

			/* retry with a smaller chunk... */
			if (error == ENOMEM && chunksz > PAGE_SIZE) {
				chunksz = trunc_page(chunksz / 2);
				if (chunksz < PAGE_SIZE)
					chunksz = PAGE_SIZE;
				continue;
			}

			break;
		}

		/*
		 * step 3: move a chunk of data
		 */

		sz = chunksz - pageoffset;
		if (sz > togo)
			sz = togo;
		error = uiomove((caddr_t) (kva + pageoffset), sz, uio);
		if (error)
			break;
		togo -= sz;
		baseva += chunksz;


		/*
		 * step 4: unmap the area of kernel memory
		 */

		vm_map_lock(kernel_map);
		(void)uvm_unmap_remove(kernel_map, kva, kva+chunksz,
		    &dead_entries);
		vm_map_unlock(kernel_map);

		if (dead_entries != NULL)
			uvm_unmap_detach(dead_entries, AMAP_REFALL);
	}

	/*
	 * done
	 */

	return (error);
}
