/*	$OpenBSD: pmap_table.h,v 1.4 1999/02/09 06:36:27 smurph Exp $ */
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

/*
 * HISTORY
 */


/* an entry is considered invalid if pm_size = 0 */
/* end of list is indicated by pm_size 0xffffffff */

typedef struct {
  vm_offset_t	phys_start;   /* in bytes */
  vm_offset_t   virt_start;   /* in bytes */
  unsigned int	size;      /* in bytes */
  unsigned int  prot;	      /* vm_prot_read, vm_prot_write */
  unsigned int  cacheability; /* none, writeback, normal */
} pmap_table_entry;

typedef pmap_table_entry *pmap_table_t;

