/*	$NetBSD: db_memrw.c,v 1.11 1996/02/20 02:42:55 gwr Exp $	*/

/*
 * Copyright (c) 1996 Gordon W. Ross
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
 * 3. The name of the author may not be used to endorse or promote products
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
 */

/*
 * Interface to the debugger for virtual memory read/write.
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
 */

#include <sys/param.h>
#include <sys/proc.h>

#include <vm/vm.h>

#include <machine/pte.h>
#include <machine/db_machdep.h>

#include <ddb/db_access.h>

#include "cache.h"

/*
 * Read bytes from kernel address space for debugger.
 * This used to check for valid PTEs, but now that
 * traps in DDB work correctly, "Just Do It!"
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	register size_t	size;
	register char	*data;
{
	register char	*src = (char*)addr;

	if (size == 4) {
		*((int*)data) = *((int*)src);
		return;
	}

	if (size == 2) {
		*((short*)data) = *((short*)src);
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
 */
static void
db_write_text(addr, size, data)
	vm_offset_t	addr;
	register size_t	size;
	register char	*data;
{
	register char *dst;
	int		ch, oldpte, tmppte;
	vm_offset_t pgva, prevpg;

	/* Prevent restoring a garbage PTE. */
	if (size <= 0)
		return;

	dst = (char*)addr;
	pgva = sun3_trunc_page((long)dst);

	goto firstpage;
	do {

		/*
		 * If we are on a new page, restore the PTE
		 * for the previous page, and make the new
		 * page writable.
		 */
		pgva = sun3_trunc_page((long)dst);
		if (pgva != prevpg) {
			/*
			 * Restore old PTE.  No cache flush,
			 * because the tmp PTE has no-cache.
			 */
			set_pte(prevpg, oldpte);

		firstpage:
			/*
			 * Flush the VAC to prevent a cache hit
			 * on the old, read-only PTE.
			 */
#ifdef	HAVECACHE
			if (cache_size)
				cache_flush_page(pgva);
#endif
			oldpte = get_pte(pgva);
			if ((oldpte & PG_VALID) == 0) {
				db_printf(" address 0x%x not a valid page\n", dst);
				return;
			}
			tmppte = oldpte | PG_WRITE | PG_NC;
			set_pte(pgva, tmppte);

			prevpg = pgva;
		}

		/* Now we can write in this page of kernel text... */
		*dst++ = *data++;

	} while (--size > 0);

	/* Restore old PTE for the last page touched. */
	set_pte(prevpg, oldpte);

	/* Finally, clear the instruction cache. */
	ICIA();
}

/*
 * Write bytes to kernel address space for debugger.
 */
extern char	kernel_text[], etext[];
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	register size_t	size;
	register char	*data;
{
	register char	*dst = (char *)addr;

	/* If any part is in kernel text, use db_write_text() */
	if ((dst < etext) && ((dst + size) > kernel_text)) {
		db_write_text(dst, size, data);
		return;
	}

	if (size == 4) {
		*((int*)dst) = *((int*)data);
		return;
	}

	if (size == 2) {
		*((short*)dst) = *((short*)data);
		return;
	}

	while (size > 0) {
		--size;
		*dst++ = *data++;
	}
}

