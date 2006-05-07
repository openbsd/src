/*	$OpenBSD: pmap_table.c,v 1.1.1.1 2006/05/07 19:38:21 miod Exp $	*/

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

#include <machine/cmmu.h>
#include <machine/pmap_table.h>

#define	R	UVM_PROT_R
#define	RW	UVM_PROT_RW
#define	CW	CACHE_WT
#define	CI	CACHE_INH
#define	CG	CACHE_GLOBAL

#include <machine/av400.h>
const pmap_table_entry
machine_map[] = {
	{ AV400_PROM,	AV400_PROM,	AV400_PROM_SIZE,	RW,	CI },
#if 0	/* mapped by the hardcoded BATC entries */
	{ AV400_UTILITY,AV400_UTILITY,	AV400_UTILITY_SIZE,	RW,	CI },
#endif
	{ 0, 0, (vsize_t)-1, 0, 0 }
};

pmap_table_t
pmap_table_build(void)
{
	return (machine_map);
}
