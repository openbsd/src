/*
 * Copyright (c) 1999 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the author nor the names of his contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/mbuf.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <uvm/uvm.h>
#include <uvm/uvm_uio.h>

int uvm_uio_enable = 1;
int uvm_uio_num_try = 0;
int uvm_uio_num_success = 0;

/*
 * m_ext functions.
 */
void uvm_mbuf_free __P((struct mbuf *));
void uvm_mbuf_ref __P((struct mbuf *));

/*
 * returns the length of I/O, 0 on failure.
 *
 * Should not be called if UVM_UIO_TRY(uio) has been checked first.
 */
size_t
uvm_uio_to_mbuf(uio, mbuf)
	struct uio *uio;
	struct mbuf *mbuf;
{
	struct vm_map *map;
	vaddr_t realbase, base, kva;
	vsize_t reallen, len, offset;
	struct vm_page **pages;
	int npages;
	struct iovec *iov;
	struct uvm_mbuf *um;
	struct mbuf *m;
#ifndef PMAP_NEW
	int i;
#endif

	uvm_uio_num_try++;

	if ((mbuf->m_flags & M_EXT)) {
		printf("uvm_uio_to_mbuf: fail 1\n");
		return 0;
	}

	map = &uio->uio_procp->p_vmspace->vm_map;
	iov = uio->uio_iov;

	/*
	 * XXX - check if iov_len is bigger than max vsize_t
	 */

	reallen = (vsize_t)iov->iov_len;
	realbase = (vaddr_t)iov->iov_base;

	/*
	 * Check alignment.
	 *
	 * What we really want is to somehow tell the caller how much the
	 * uios should be adjusted and try again.
	 */
	if ((realbase & (sizeof(long) - 1)) != 0) {
		printf("uvm_uio_to_mbuf: not aligned\n");
		return 0;
	}

	base = trunc_page(realbase);
	offset = realbase - base;

	/*
	 * truncate reallen here so that we won't do a huge malloc.
	 * Subtract offset so that the next round will be page aligned.
	 */
	if (reallen > UVM_UIO_LIMIT)
		reallen = UVM_UIO_LIMIT - offset;

	len = reallen + offset;
	len = round_page(len);
	npages = atop(len);

	if ((mbuf->m_flags & M_PKTHDR)) {

		MGET(m, M_WAIT, MT_DATA);
		mbuf->m_len = 0;
		mbuf->m_next = m;
	} else {
		m = mbuf;
		m->m_next = NULL;
	}

	MALLOC(um, struct uvm_mbuf *, sizeof(struct uvm_mbuf), M_TEMP,
	       M_WAITOK);

	/*
	 * If the pages we have less than UVM_UIO_SMALL_PAGES, we can fit
	 * them into the pages struct in uvm_uio.
	 */
	if (npages > UVM_UIO_SMALL_PAGES)
		MALLOC(pages, struct vm_page **,
		       npages * sizeof(struct vm_page *),  M_TEMP, M_WAITOK);
	else
		pages = um->um_pages_small;

	/*
	 * Loan the pages we want.
	 */
	if (uvm_loan(map, base, len, (void **)pages, UVM_LOAN_TOPAGE) !=
	    KERN_SUCCESS) {
		/*
		 * XXX - This is really ENOMEM or EFAULT.
		 */
		printf("uvm_uio_to_mbuf: loan failed\n");

		goto fail;
	}

	/*
	 * Allocate space to map pages.
	 */
	kva = vm_map_min(kernel_map);
	if (uvm_map(kernel_map, &kva, len, NULL, UVM_UNKNOWN_OFFSET,
		       UVM_MAPFLAG(UVM_PROT_READ, UVM_PROT_READ, UVM_INH_NONE,
				   UVM_ADV_SEQUENTIAL, 0)) != KERN_SUCCESS) {
		uvm_unloanpage(pages, npages);
		goto fail;
	}

	/*
	 * Initialize um.
	 */
	um->um_pages = pages;
	um->um_npages = npages;
	um->um_usecount = 1;
	um->um_kva = kva;

	printf("mapping: 0x%x -> 0x%x\n", kva, kva + len);
	/*
	 * Map pages.
	 */
#ifdef PMAP_NEW
	pmap_kenter_pgs(kva, pages, npages);
#else
	for (i = 0; i < npages; i++, kva += PAGE_SIZE)
		pmap_enter(pmap_kernel(), kva, VM_PAGE_TO_PHYS(pages[i]),
                    VM_PROT_READ, TRUE, VM_PROT_READ);
#endif

	/*
	 * Update mbuf.
	 */
	m->m_flags |= M_EXT | M_RONLY;
	m->m_data = (caddr_t)(um->um_kva + offset);
	m->m_len = reallen;
	m->m_ext.ext_free = uvm_mbuf_free;
	m->m_ext.ext_ref = uvm_mbuf_ref;
	/*
	 * We lie about those two to avoid problems with someone trying
	 * to prepend data.
	 */ 
	m->m_ext.ext_buf = (caddr_t)(um->um_kva + offset);
	m->m_ext.ext_size = reallen;
	m->m_ext.ext_handle = um;

	/*
	 * Update uio.
	 */
	if ((iov->iov_len -= reallen) == 0) {
		uio->uio_iov++;
		uio->uio_iovcnt--;
	}
	uio->uio_resid -= reallen;

	uvm_uio_num_success++;

	return reallen;
fail:
	if (npages > UVM_UIO_SMALL_PAGES)
		FREE(pages, M_TEMP);

	if (m != mbuf)
		m_freem(m);

	FREE(um, M_TEMP);

	return 0;
}

void
uvm_mbuf_free(mb)
	struct mbuf *mb;
{
	struct uvm_mbuf *um = (struct uvm_mbuf *)mb->m_ext.ext_handle;
	vsize_t len;

	if (--um->um_usecount)
		return;

	len = ptoa(um->um_npages);

	printf("unmapping: 0x%x -> 0x%x\n", um->um_kva, um->um_kva + len);
#ifdef PMAP_NEW
	pmap_kremove(um->um_kva, len);
#else
	pmap_remove(pmap_kernel(), um->um_kva, um->um_kva + len);
#endif

	uvm_unloanpage(um->um_pages, um->um_npages);
	uvm_unmap(kernel_map, um->um_kva, um->um_kva + len);
	uvm_km_free_wakeup(kernel_map, um->um_kva, len);
	if (um->um_npages > UVM_UIO_SMALL_PAGES)
		FREE(um->um_pages, M_TEMP);

	FREE(um, M_TEMP);
#ifdef DIAGNOSTIC
	mb->m_data = NULL;
	mb->m_ext.ext_handle = NULL;
	mb->m_flags &= ~M_EXT;
#endif
}

void
uvm_mbuf_ref(mb)
	struct mbuf *mb;
{
	((struct uvm_mbuf *)mb->m_ext.ext_handle)->um_usecount++;
}
