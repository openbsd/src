/*	$OpenBSD: db_memrw.c,v 1.7 2005/11/13 14:23:26 martin Exp $	*/
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
	char	*dst;

	pt_entry_t *ptep0 = 0;
	pt_entry_t	oldmap0 = { 0 };
	vaddr_t	addr1;
	pt_entry_t *ptep1 = 0;
	pt_entry_t	oldmap1 = { 0 };
	extern char	etext;

	if (addr >= VM_MIN_KERNEL_ADDRESS &&
	    addr < (vaddr_t)&etext) {
		ptep0 = PTE_BASE + atop(addr);
		oldmap0 = *ptep0;
		*(int *)ptep0 |= /* INTEL_PTE_WRITE */ PG_RW;

		addr1 = trunc_page(addr + size - 1);
		if (trunc_page(addr) != addr1) {
			/* data crosses a page boundary */
			ptep1 = PTE_BASE + atop(addr1);
			oldmap1 = *ptep1;
			*(int *)ptep1 |= /* INTEL_PTE_WRITE */ PG_RW;
		}
		tlbflush();
	}

	dst = (char *)addr;

	while (size-- > 0)
		*dst++ = *data++;

	if (ptep0) {
		*ptep0 = oldmap0;
		if (ptep1)
			*ptep1 = oldmap1;
		tlbflush();
	}
}
