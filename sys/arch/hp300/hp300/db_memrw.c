/*	$OpenBSD: db_memrw.c,v 1.10 2008/06/26 05:42:10 ray Exp $	*/
/*	$NetBSD: db_memrw.c,v 1.5 1997/06/10 18:48:47 veego Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross and Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Interface to the debugger for virtual memory read/write.
 * This file is shared by DDB and KGDB, and must work even
 * when only KGDB is included (thus no db_printf calls).
 *
 * To write in the text segment, we have to first make
 * the page writable, do the write, then restore the PTE.
 * For writes outside the text segment, and all reads,
 * just do the access -- if it causes a fault, the debugger
 * will recover with a longjmp to an appropriate place.
 *
 * ALERT!  If you want to access device registers with a
 * specific size, then the read/write functions have to
 * make sure to do the correct sized pointer access.
 *
 * Modified from sun3 version for hp300 (and probably other m68ks, too)
 * by Jason R. Thorpe <thorpej@NetBSD.ORG>.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <machine/pte.h>
#include <machine/db_machdep.h>
#include <machine/cpu.h>

#include <ddb/db_access.h>

static void	db_write_text(db_addr_t, size_t, char *);

/*
 * Read bytes from kernel address space for debugger.
 * This used to check for valid PTEs, but now that
 * traps in DDB work correctly, "Just Do It!"
 */
void
db_read_bytes(addr, size, data)
	db_addr_t	addr;
	size_t		size;
	char		*data;
{
	char	*src = (char *)addr;

	if (size == 4) {
		*((int *)data) = *((int *)src);
		return;
	}

	if (size == 2) {
		*((short *)data) = *((short *)src);
		return;
	}

	while (size > 0) {
		--size;
		*data++ = *src++;
	}
}

/*
 * Write bytes somewhere in kernel text.
 * Makes text page writable temporarily.
 * We're probably a little to cache-paranoid.
 */
static void
db_write_text(addr, size, data)
	db_addr_t addr;
	size_t size;
	char *data;
{
	char *dst, *odst;
	pt_entry_t *pte, oldpte, tmppte;
	vaddr_t pgva;
	int limit;

	if (size == 0)
		return;

	dst = (char *)addr;

	do {
		/*
		 * Get the VA for the page.
		 */
		pgva = trunc_page((vaddr_t)dst);

		/*
		 * Save this destination address, for TLB
		 * flush.
		 */
		odst = dst;

		/*
		 * Compute number of bytes that can be written
		 * with this mapping and subtract it from the
		 * total size.
		 */
		limit = NBPG - ((u_long)dst & PGOFSET);
		if (limit > size)
			limit = size;
		size -= limit;

#ifdef M68K_MMU_HP
		/*
		 * Flush the supervisor side of the VAC to
		 * prevent a cache hit on the old, read-only PTE.
		 * XXX Is this really necessary, or am I just
		 * paranoid?
		 */
		if (ectype == EC_VIRT)
			DCIS();
#endif

		/*
		 * Make the page writable.  Note the mapping is
		 * cache-inhibited to save hair.
		 */
		pte = kvtopte(pgva);
		oldpte = *pte;

		if ((oldpte & PG_V) == 0) {
			printf(" address %p not a valid page\n", dst);
			return;
		}

		tmppte = (oldpte & ~PG_RO) | PG_RW | PG_CI;
		*pte = tmppte;
		TBIS((vaddr_t)odst);

		/*
		 * Page is now writable.  Do as much access as we
		 * can in this page.
		 */
		for (; limit > 0; limit--)
			*dst++ = *data++;

		/*
		 * Restore the old PTE.
		 */
		*pte = oldpte;
		TBIS((vaddr_t)odst);
	} while (size != 0);

	/*
	 * Invalidate the instruction cache so our changes
	 * take effect.
	 */
	ICIA();
}

/*
 * Write bytes to kernel address space for debugger.
 */
extern char	kernel_text[], etext[];
void
db_write_bytes(addr, size, data)
	db_addr_t	addr;
	size_t	size;
	char	*data;
{
	char	*dst = (char *)addr;

	/* If any part is in kernel text, use db_write_text() */
	if ((dst < etext) && ((dst + size) > kernel_text)) {
		db_write_text(addr, size, data);
		return;
	}

	if (size == 4) {
		*((int *)dst) = *((int *)data);
		return;
	}

	if (size == 2) {
		*((short *)dst) = *((short *)data);
		return;
	}

	while (size > 0) {
		--size;
		*dst++ = *data++;
	}
}
