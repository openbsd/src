/*	$OpenBSD: pmap_table.c,v 1.18 2004/05/19 14:30:53 miod Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1993-1992 Carnegie Mellon University
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <machine/board.h>
#include <machine/cmmu.h>
#include <uvm/uvm_extern.h>
#include <machine/pmap_table.h>

#define	R	VM_PROT_READ
#define	RW	(VM_PROT_READ | VM_PROT_WRITE)
#define	CW	CACHE_WT
#define	CI	CACHE_INH
#define	CG	CACHE_GLOBAL

/*  phys_start, virt_start, size, prot, cacheability */
#ifdef MVME187
const pmap_table_entry
m187_board_table[] = {
	{ BUG187_START, BUG187_START, round_page(BUG187_SIZE), RW, CI },
	{ OBIO_START  , OBIO_START  , round_page(OBIO_SIZE)  , RW, CI },
	{ 0, 0, 0xffffffff, 0, 0 },
};
#endif

#ifdef MVME188
const pmap_table_entry
m188_board_table[] = {
	{ MVME188_UTILITY, MVME188_UTILITY,
	    round_page(MVME188_UTILITY_SIZE), RW, CI },
	{ 0, 0, 0xffffffff, 0, 0 },
};
#endif

#ifdef MVME197
const pmap_table_entry
m197_board_table[] = {
	{ FLASH_START, FLASH_START, round_page(FLASH_SIZE), RW, CI },
	{ OBIO_START , OBIO_START , round_page(OBIO_SIZE) , RW, CI },
	/* No need to mention BUG here - it is contained inside OBIO */
	{ 0, 0, 0xffffffff, 0, 0 },
};
#endif

pmap_table_t
pmap_table_build(void)
{
	switch (brdtyp) {
#ifdef MVME187
	case BRD_187:
	case BRD_8120:
		return m187_board_table;
#endif
#ifdef MVME188
	case BRD_188:
		return m188_board_table;
#endif
#ifdef MVME197
	case BRD_197:
		return m197_board_table;
#endif
	default:
		return NULL;	/* silence warning */
	}
}
