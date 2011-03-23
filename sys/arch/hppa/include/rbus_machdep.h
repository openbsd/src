/*	$OpenBSD: rbus_machdep.h,v 1.5 2011/03/23 16:54:35 pirofti Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#if !defined _MACHINE_RBUS_MACHDEP_H_
#define _MACHINE_RBUS_MACHDEP_H_

static __inline int
md_space_map(rbus_tag_t rbt, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp)
{
	if (bshp)
		*(bshp) = bpa;

	return (0);
}

#define md_space_unmap(rbt,bsh,s,addrp)	do { *(addrp) = (bsh); } while (0)

struct pci_attach_args;

#define rbus_pccbb_parent_mem(d, p) (*(p)->pa_pc->pc_alloc_parent)((d), (p), 0)
#define rbus_pccbb_parent_io(d, p)  (*(p)->pa_pc->pc_alloc_parent)((d), (p), 1)

#define pccbb_attach_hook(parent, self, pa)	/* nothing */

#endif /* _MACHINE_RBUS_MACHDEP_H_ */
