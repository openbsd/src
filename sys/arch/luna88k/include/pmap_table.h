/*	$OpenBSD: pmap_table.h,v 1.1.1.1 2004/04/21 15:23:58 aoyama Exp $ */
/*
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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

#ifndef __MACHINE_PMAP_TABLE_H__
#define __MACHINE_PMAP_TABLE_H__

/*
 * Built-in mappings list.
 * An entry is considered invalid if pm_size = 0, and
 * end of list is indicated by pm_size 0xffffffff
 */
typedef struct {
	vaddr_t		phys_start;	/* in bytes */
	vaddr_t		virt_start;	/* in bytes */
	vsize_t		size;		/* in bytes */
	unsigned int	prot;		/* vm_prot_read, vm_prot_write */
	unsigned int	cacheability;	/* none, writeback, normal */
} pmap_table_entry;

typedef const pmap_table_entry *pmap_table_t;

pmap_table_t pmap_table_build(void);

#endif	/* __MACHINE_PMAP_TABLE_H__ */

