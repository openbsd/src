/*	$NetBSD: uvm_pager_i.h,v 1.7 1999/03/25 18:48:55 mrg Exp $	*/

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
 * from: Id: uvm_pager_i.h,v 1.1.2.2 1997/10/09 23:05:46 chuck Exp 
 */

#ifndef _UVM_UVM_PAGER_I_H_
#define _UVM_UVM_PAGER_I_H_

/*
 * uvm_pager_i.h
 */

/*
 * inline functions [maybe]
 */

#if defined(UVM_PAGER_INLINE) || defined(UVM_PAGER)

/*
 * uvm_pageratop: convert KVAs in the pager map back to their page
 * structures.
 */

PAGER_INLINE struct vm_page *
uvm_pageratop(kva)
	vaddr_t kva;
{
	paddr_t pa;
 
	pa = pmap_extract(pmap_kernel(), kva);
	if (pa == 0)
		panic("uvm_pageratop");
	return (PHYS_TO_VM_PAGE(pa));
} 

#endif /* defined(UVM_PAGER_INLINE) || defined(UVM_PAGER) */

#endif /* _UVM_UVM_PAGER_I_H_ */
