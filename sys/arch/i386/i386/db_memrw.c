/*	$OpenBSD: db_memrw.c,v 1.9 2006/04/27 15:37:50 mickey Exp $	*/
/*	$NetBSD: db_memrw.c,v 1.6 1999/04/12 20:38:19 pk Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Routines to read and write memory on behalf of the debugger, used
 * by DDB and KGDB.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>

#include <ddb/db_access.h>

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(vaddr_t addr, size_t size, char *data)
{
	char	*src;

	src = (char *)addr;
	while (size-- > 0)
		*data++ = *src++;
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(vaddr_t addr, size_t size, char *data)
{
	extern char	etext;
	u_int32_t bits, bits1;
	vaddr_t	addr1 = 0;
	char	*dst;

	if (addr >= VM_MIN_KERNEL_ADDRESS &&
	    addr < (vaddr_t)&etext) {
		bits = pmap_pte_setbits(addr, PG_RW, 0) & PG_RW;

		addr1 = trunc_page(addr + size - 1);
		if (trunc_page(addr) != addr1)
			/* data crosses a page boundary */
			bits1 = pmap_pte_setbits(addr1, PG_RW, 0) & PG_RW;
		tlbflush();
	}

	dst = (char *)addr;

	while (size-- > 0)
		*dst++ = *data++;

	if (addr1) {
		pmap_pte_setbits(addr, 0, bits);
		if (bits1)
			pmap_pte_setbits(addr1, 0, bits1);
		tlbflush();
	}
}
