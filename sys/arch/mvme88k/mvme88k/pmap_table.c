/*	$OpenBSD: pmap_table.c,v 1.25 2011/01/02 13:40:07 miod Exp $	*/

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

#include <uvm/uvm_extern.h>

#include <machine/pmap_table.h>

#define	R	UVM_PROT_R
#define	RW	UVM_PROT_RW
#define	CW	CACHE_WT
#define	CI	CACHE_INH
#define	CG	CACHE_GLOBAL

#ifdef MVME187
#include <machine/mvme187.h>
const struct pmap_table
m187_board_table[] = {
	{ BUG187_START,		BUG187_SIZE,	RW, CI },
#if 0	/* mapped by the hardcoded BATC entries */
	{ OBIO187_START,	OBIO187_SIZE,	RW, CI },
#endif
	{ 0, 0xffffffff, 0, 0 },
};
#endif

#ifdef MVME188
#include <machine/mvme188.h>
const struct pmap_table
m188_board_table[] = {
	{ MVME188_EPROM,	MVME188_EPROM_SIZE, RW, CI },
#if 0	/* mapped by the hardcoded BATC entries */
	{ MVME188_UTILITY,	MVME188_UTILITY_SIZE, RW, CI },
#endif
	{ 0, 0xffffffff, 0, 0 },
};
#endif

#ifdef MVME197
#include <machine/mvme197.h>
const struct pmap_table
m197_board_table[] = {
	/* We need flash 1:1 mapped to access the 88410 chip underneath */
	{ FLASH_START,		FLASH_SIZE,	RW, CI },
	{ OBIO197_START,	OBIO197_SIZE,	RW, CI },
	/* No need to mention BUG here - it is contained inside OBIO */
	{ 0, 0xffffffff, 0, 0 },
};
#endif

const struct pmap_table *
pmap_table_build()
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
