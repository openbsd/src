/*	$OpenBSD: db_memrw.c,v 1.7 2001/11/06 19:53:13 miod Exp $	*/
/*	$NetBSD: db_memrw.c,v 1.7 1997/06/10 18:24:34 veego Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * To write in the text segment, we have to first make
 * the page writable, do the write, then restore the PTE.
 * For reads, validate address first to avoid MMU trap.
 */

#include <sys/param.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <ddb/db_access.h>

#include <machine/cpu.h>
#include <machine/pte.h>

char db_read_data __P((char *src));
void db_write_text __P((char *dst, int ch));
void db_write_data __P((char *dst, int ch));

/*
 * Read one byte somewhere in the kernel.
 * It does not matter if this is slow. -gwr
 */
char
db_read_data(src)
	char *src;
{
	u_int *pte;
	vm_offset_t pgva;

	pgva = m68k_trunc_page((long)src);
	pte = kvtopte(pgva);

	if ((*pte & PG_V) == 0) {
		db_printf(" address %p not a valid page\n", src);
		return 0;
	}
	return (*src);
}

/*
 * Read bytes from kernel address space for debugger.
 * It does not matter if this is slow. -gwr
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	size_t		size;
	char	       *data;
{
	char	*src, *limit;

	src = (char *)addr;
	limit = src + size;

	while (src < limit) {
		*data = db_read_data(src);
		data++;
		src++;
	}
}

/*
 * Write one byte somewhere in kernel text.
 * It does not matter if this is slow. -gwr
 */
void
db_write_text(dst, ch)
	char *dst;
	int ch;
{
	u_int *pte, oldpte;

	pte = kvtopte((vm_offset_t)dst);
	oldpte = *pte;
	if ((oldpte & PG_V) == 0) {
		db_printf(" address %p not a valid page\n", dst);
		return;
	}

#if 0
	printf("db_write_text: %x: %x = %x (%x:%x)\n", dst, *dst, ch, pte,
	    *pte);
#endif
	*pte &= ~PG_RO;
	TBIS((vm_offset_t)dst);

	*dst = (char) ch;

	*pte = oldpte;
	TBIS((vm_offset_t)dst);
	dma_cachectl (dst, 1);
}

/*
 * Write one byte somewhere outside kernel text.
 * It does not matter if this is slow. -gwr
 */
void
db_write_data(dst, ch)
	char *dst;
	int ch;
{
	u_int *pte;

	pte = kvtopte((vm_offset_t)dst);

	if ((*pte & (PG_V | PG_RO)) != PG_V) {
		db_printf(" address %p not a valid page\n", dst);
		return;
	}
	*dst = (char) ch;
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	size_t		size;
	char	       *data;
{
	extern char	etext[];
	char	       *dst, *limit;

	dst = (char *)addr;
	limit = dst + size;

	while (dst < limit) {
		if (dst < etext)	/* kernel text starts at 0 */
			db_write_text(dst, *data);
		else
			db_write_data(dst, *data);
		dst++;
		data++;
	}
}
