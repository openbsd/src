/*	$NetBSD: isa_dma.h,v 1.1 1995/06/28 01:24:50 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

struct	isadma_fcns {
	int	(*isadma_map) __P((caddr_t addr, vm_size_t size,
		    vm_offset_t *mappings, int flags));
	void	(*isadma_unmap) __P((caddr_t addr, vm_size_t size,
		    int nmappings, vm_offset_t *mappings));
	void	(*isadma_copytobuf) __P((caddr_t addr, vm_size_t size,
		    int nmappings, vm_offset_t *mappings));
	void	(*isadma_copyfrombuf) __P((caddr_t addr, vm_size_t size,
		    int nmappings, vm_offset_t *mappings));
};

/*
 * Global which tells which set of functions are correct
 * for this machine.
 */
struct	isadma_fcns *isadma_fcns;

struct	isadma_fcns apecs_isadma_fcns;		/* APECS DMA mapping */
