/*	$NetBSD: db_memrw.c,v 1.2 1996/01/19 13:51:11 leo Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
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
 * To write in the text segment, we have to first make
 * the page writable, do the write, then restore the PTE.
 * For reads, validate address first to avoid MMU trap.
 *
 * Note the special handling for 2/4 byte sizes. This is done to make
 * it work sensibly for device registers.
 */

#include <sys/param.h>
#include <sys/proc.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/pte.h>

/*
 * Check if access is allowed to 'addr'. Mask should contain
 * PG_V for read access, PV_V|PG_RO for write access.
 */
static int
db_check(addr, mask)
	char	*addr;
	u_int	mask;
{
	u_int	*pte;

	pte  = kvtopte((vm_offset_t)addr);

	if ((*pte & mask) != PG_V) {
		db_printf(" address 0x%x not a valid page\n", addr);
		return 0;
	}
	return 1;
}
	
/*
 * Read bytes from kernel address space for debugger.
 * It does not matter if this is slow. -gwr
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	u_int8_t	*src, *dst, *limit;

	src   = (u_int8_t *)addr;
	dst   = (u_int8_t *)data;
	limit = src + size;

	if (size == 2 || size == 4) {
		if(db_check(src, PG_V) && db_check(limit, PG_V)) {
			if (size == 2)
				*(u_int16_t*)data = *(u_int16_t*)addr;
			else *(u_int32_t*)data = *(u_int32_t*)addr;
			return;
		}
	}

	while (src < limit) {
		*dst = db_check(src, PG_V) ? *src : 0;
		dst++;
		src++;
	}
}

/*
 * Write one byte somewhere in kernel text.
 * It does not matter if this is slow. -gwr
 */
static void
db_write_text(dst, ch)
	u_int8_t *dst;
	u_int8_t ch;
{
	u_int *pte, oldpte;

	pte = kvtopte((vm_offset_t)dst);
	oldpte = *pte;
	if ((oldpte & PG_V) == 0) {
		db_printf(" address 0x%x not a valid page\n", dst);
		return;
	}

/*printf("db_write_text: %x: %x = %x (%x:%x)\n", dst, *dst, ch, pte, *pte);*/
	*pte &= ~PG_RO;
	TBIS(dst);

	*dst = ch;

	*pte = oldpte;
	TBIS(dst);
	cachectl (4, dst, 1);
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	int		size;
	char		*data;
{
	extern char	etext[] ;
	u_int8_t	*dst, *src, *limit;

	dst   = (u_int8_t *)addr;
	src   = (u_int8_t *)data;
	limit = dst + size;

	if ((char*)dst >= etext && (size == 2 || size == 4)) {
		if(db_check(dst, PG_V|PG_RO) && db_check(limit, PG_V|PG_RO)) {
			if (size == 2)
				*(u_int16_t*)addr = *(u_int16_t*)data;
			else *(u_int32_t*)addr = *(u_int32_t*)data;
			return;
		}
	}
	while (dst < limit) {
		if ((char*)dst < etext)	/* kernel text starts at 0 */
			db_write_text(dst, *src);
		else if (db_check(dst, PG_V|PG_RO))
				*dst = *src;
		dst++;
		src++;
	}
}
