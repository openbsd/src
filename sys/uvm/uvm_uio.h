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

#include <vm/vm.h>	/* for PAGE_SIZE */

/*
 * If the number of pages we're about to transfer is smaller than this number
 * we use the pre-allocated array.
 */
#define UVM_UIO_SMALL_PAGES 8

/*
 * Limit transfers to this number to avoid running out of memory.
 */
#define UVM_UIO_LIMIT (256 * PAGE_SIZE)

/*
 * m_ext structure.
 */
struct uvm_mbuf {
	struct vm_page **um_pages;	/* The pages */
	int um_npages;			/* number of pages */
	int um_usecount;		/* ref cnt */
	vaddr_t um_kva;			/* where the pages are mapped */
	struct vm_page *um_pages_small[UVM_UIO_SMALL_PAGES];
};

extern int uvm_uio_enable;

#define UVM_UIO_MINIO PAGE_SIZE		/* XXX - tweak */
#define UVM_UIO_TRY(uio) (uvm_uio_enable && \
			((uio)->uio_iov->iov_len >= UVM_UIO_MINIO) && \
			((uio)->uio_procp != NULL) && \
			((uio)->uio_rw == UIO_WRITE) && \
			((uio)->uio_segflg == UIO_USERSPACE))

size_t uvm_uio_to_mbuf __P((struct uio *, struct mbuf *));
