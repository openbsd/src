/*	$OpenBSD: pmap_bootstrap.c,v 1.2 2004/08/01 17:44:17 miod Exp $	*/
/*
 * Copyright (c) 2001-2004, Miodrag Vallat
 * Copyright (c) 1998-2001 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
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
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm.h>

extern vaddr_t pmap_map(vaddr_t, paddr_t, paddr_t, vm_prot_t, u_int);
extern vaddr_t avail_start;
extern vaddr_t virtual_avail;

#define ETHERPAGES 16
void *etherbuf = NULL;
int etherlen;

vaddr_t
pmap_bootstrap_md(vaddr_t vaddr)
{
#if defined (MVME187) || defined (MVME197)
	/*
	 * Get ethernet buffer - need ETHERPAGES pages physically contiguous.
	 * XXX need to switch if_ie to bus_space...
	 */
	if (brdtyp == BRD_187 || brdtyp == BRD_8120 || brdtyp == BRD_197) {
		etherlen = ETHERPAGES * PAGE_SIZE;
		etherbuf = (void *)vaddr;

		vaddr = pmap_map(vaddr, avail_start,
		    avail_start + ETHERPAGES * PAGE_SIZE,
		    VM_PROT_WRITE | VM_PROT_READ, CACHE_INH);

		virtual_avail += etherlen;
		avail_start += etherlen;
	}
#endif /* defined (MVME187) || defined (MVME197) */

	return vaddr;
}
