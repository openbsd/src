/*	$OpenBSD: subr_rmap.c,v 1.4 1999/01/11 01:29:17 niklas Exp $	*/
/*	$NetBSD: subr_rmap.c,v 1.11 1996/03/16 23:17:11 christos Exp $	*/

/*
 * Copyright (C) 1992, 1994 Wolfgang Solfrank.
 * Copyright (C) 1992, 1994 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/map.h>
#include <sys/systm.h>

/*
 * Resource allocation map handling.
 *
 * Code derived from usage in ../vm/swap_pager.c and ../vm/vm_swap.c
 * and the (corrected due to the above) comments in ../sys/map.h.
 *
 * Assume small maps. Keep it sorted by addr with free mapents last.
 */

/*
 * Initialize a resource map.
 * The map is called "name", and has nelem-1
 * slots (the first one is reused to describe the map).
 * Initially it manages the address range starting at
 * addr with size size.
 */
void
rminit(mp, size, addr, name, nelem)
	struct map *mp;
	long size, addr;
	char *name;
	int nelem;
{
	struct mapent *ep;
	
#ifdef DIAGNOSTIC
	/* mapsize had better be at least 2 */
	if (nelem < 2 || addr <= 0 || size < 0)
		panic("rminit %s",name);
#endif

	mp->m_name = name;
	mp->m_limit = (struct mapent *)mp + nelem;
	
	/* initially the first entry describes all free space */
	ep = (struct mapent *)mp + 1;
	ep->m_size = size;
	ep->m_addr = addr;
	/* the remaining slots are unused (indicated by m_addr == 0) */
	while (++ep < mp->m_limit)
		ep->m_addr = 0;
}

/*
 * Allocate space out of a resource map.
 * Try to find an exact match. Otherwise get the space from
 * the smallest slot.
 */
long
rmalloc(mp, size)
	struct map *mp;
	long size;
{
	struct mapent *ep, *fp;
	long addr;
	
	/* first check arguments */
#ifdef DIAGNOSTIC
	if (size < 0)
		panic("rmalloc %s", mp->m_name);
#endif
	if (size <= 0)
		return 0;
	
	fp = 0;
	/* try to find the smallest fit */
	for (ep = (struct mapent *)mp + 1; ep < mp->m_limit; ep++) {
		if (!ep->m_addr) {
			/* unused slots terminate the list */
			break;
		}
		if (ep->m_size == size) {
			/* found exact match, use it, ... */
			addr = ep->m_addr;
			/* copy over the remaining slots ... */
			ovbcopy(ep + 1,ep,(char *)mp->m_limit - (char *)(ep + 1));
			/* and mark the last slot as unused */
			mp->m_limit[-1].m_addr = 0;
			return addr;
		}
		if (ep->m_size > size
		    && (!fp
			|| fp->m_size > ep->m_size)) {
			/* found a larger slot, remember the smallest of these */
			fp = ep;
		}
	}
	if (fp) {
		/* steal requested size from a larger slot */
		addr = fp->m_addr;
		fp->m_addr += size;
		fp->m_size -= size;
		return addr;
	}
	return 0;
}

/*
 * Free (or add) space to a resource map.
 * If there aren't enough slots in the map to describe the free space,
 * drop the smallest slot.
 */
void
rmfree(mp, size, addr)
	struct map *mp;
	long size, addr;
{
	struct mapent *ep, *fp;

#ifdef DIAGNOSTIC
	/* first check arguments */
	if (size <= 0 || addr <= 0)
		panic("rmfree %s", mp->m_name);
#endif
	
	while (1) {
		fp = 0;
		
		for (ep = (struct mapent *)mp + 1; ep < mp->m_limit; ep++) {
			if (!ep->m_addr) {
				/* unused slots terminate the list */
				break;
			}
			if (ep->m_addr + ep->m_size == addr) {
				/* this slot ends just with the address to free */
				ep->m_size += size; /* increase size of slot */
				if (ep < mp->m_limit
				    && ep[1].m_addr
				    && (addr += size) >= ep[1].m_addr) {
#ifdef DIAGNOSTIC
					/* overlapping frees? */
					if (addr > ep[1].m_addr)
						panic("rmfree %s", mp->m_name);
#endif
					/* the next slot is now contiguous, so join ... */
					ep->m_size += ep[1].m_size;
					ovbcopy(ep + 2, ep + 1,
						(char *)mp->m_limit - (char *)(ep + 2));
					/* and mark the last slot as unused */
					mp->m_limit[-1].m_addr = 0;
				}
				return;
			}
			if (addr + size == ep->m_addr) {
				/* range to free is contiguous to this slot */
				ep->m_addr = addr;
				ep->m_size += size;
				return;
			}
			if (addr < ep->m_addr
			    && !mp->m_limit[-1].m_addr) {
				/* insert entry into list keeping it sorted on m_addr */
				ovbcopy(ep,ep + 1,(char *)(mp->m_limit - 1) - (char *)ep);
				ep->m_addr = addr;
				ep->m_size = size;
				return;
			}
			if (!fp || fp->m_size > ep->m_size) {
				/* find the slot with the smallest size to drop */
				fp = ep;
			}
		}
		if (ep != (struct mapent *)mp + 1
		    && ep[-1].m_addr + ep[-1].m_size == addr) {
			/* range to free is contiguous to the last used slot */
			(--ep)->m_size += size;
			return;
		}
		if (ep != mp->m_limit) {
			/* use empty slot for range to free */
			ep->m_addr = addr;
			ep->m_size = size;
			return;
		}
		/*
		 * The range to free isn't contiguous to any free space,
		 * and there is no free slot available, so we are sorry,
		 * but we have to loose some space.
		 * fp points to the slot with the smallest size
		 */
		if (fp->m_size > size) {
			/* range to free is smaller, so drop that */
			printf("rmfree: map '%s' loses space (%ld)\n",
			       mp->m_name, size);
			return;
		} else {
			/* drop the smallest slot in the list */
			printf("rmfree: map '%s' loses space (%ld)\n",
			       mp->m_name, fp->m_size);
			ovbcopy(fp + 1, fp,
				(char *)(mp->m_limit - 1) - (char *)fp);
			mp->m_limit[-1].m_addr = 0;
			/* now retry */
		}
	}
}
