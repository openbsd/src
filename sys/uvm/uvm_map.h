/*	$NetBSD: uvm_map.h,v 1.14 1999/05/26 19:16:36 thorpej Exp $	*/

/* 
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.  
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and 
 *      its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vm_map.h    8.3 (Berkeley) 3/15/94
 * from: Id: uvm_map.h,v 1.1.2.3 1998/02/07 01:16:55 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
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

#ifndef _UVM_UVM_MAP_H_
#define _UVM_UVM_MAP_H_

/*
 * uvm_map.h
 */

/*
 * macros
 */

/*
 * UVM_MAP_CLIP_START: ensure that the entry begins at or after
 * the starting address, if it doesn't we split the entry.
 * 
 * => map must be locked by caller
 */

#define UVM_MAP_CLIP_START(MAP,ENTRY,VA) { \
	if ((VA) > (ENTRY)->start) uvm_map_clip_start(MAP,ENTRY,VA); }

/*
 * UVM_MAP_CLIP_END: ensure that the entry ends at or before
 *      the ending address, if it does't we split the entry.
 *
 * => map must be locked by caller
 */

#define UVM_MAP_CLIP_END(MAP,ENTRY,VA) { \
	if ((VA) < (ENTRY)->end) uvm_map_clip_end(MAP,ENTRY,VA); }

/*
 * extract flags
 */
#define UVM_EXTRACT_REMOVE	0x1	/* remove mapping from old map */
#define UVM_EXTRACT_CONTIG	0x2	/* try to keep it contig */
#define UVM_EXTRACT_QREF	0x4	/* use quick refs */
#define UVM_EXTRACT_FIXPROT	0x8	/* set prot to maxprot as we go */


/*
 * handle inline options
 */

#ifdef UVM_MAP_INLINE
#define MAP_INLINE static __inline
#else 
#define MAP_INLINE /* nothing */
#endif /* UVM_MAP_INLINE */

/*
 * globals:
 */

#ifdef PMAP_GROWKERNEL
extern vaddr_t	uvm_maxkaddr;
#endif

/*
 * protos: the following prototypes define the interface to vm_map
 */

MAP_INLINE
void		uvm_map_deallocate __P((vm_map_t));

int		uvm_map_clean __P((vm_map_t, vaddr_t, vaddr_t, int));
void		uvm_map_clip_start __P((vm_map_t,
				vm_map_entry_t, vaddr_t));
void		uvm_map_clip_end __P((vm_map_t, vm_map_entry_t,
				vaddr_t));
MAP_INLINE
vm_map_t	uvm_map_create __P((pmap_t, vaddr_t, 
			vaddr_t, int));
int		uvm_map_extract __P((vm_map_t, vaddr_t, vsize_t, 
			vm_map_t, vaddr_t *, int));
vm_map_entry_t	uvm_map_findspace __P((vm_map_t, vaddr_t, vsize_t,
			vaddr_t *, struct uvm_object *, vaddr_t, 
			boolean_t));
int		uvm_map_inherit __P((vm_map_t, vaddr_t, vaddr_t,
			vm_inherit_t));
int		uvm_map_advice __P((vm_map_t, vaddr_t, vaddr_t, int));
void		uvm_map_init __P((void));
boolean_t	uvm_map_lookup_entry __P((vm_map_t, vaddr_t, 
			vm_map_entry_t *));
MAP_INLINE
void		uvm_map_reference __P((vm_map_t));
int		uvm_map_replace __P((vm_map_t, vaddr_t, vaddr_t, 
			vm_map_entry_t, int));
int		uvm_map_reserve __P((vm_map_t, vsize_t, vaddr_t, 
			vaddr_t *));
void		uvm_map_setup __P((vm_map_t, vaddr_t, 
			vaddr_t, int));
int		uvm_map_submap __P((vm_map_t, vaddr_t, 
			vaddr_t, vm_map_t));
MAP_INLINE
int		uvm_unmap __P((vm_map_t, vaddr_t, vaddr_t));
void		uvm_unmap_detach __P((vm_map_entry_t,int));
int		uvm_unmap_remove __P((vm_map_t, vaddr_t, vaddr_t,
				      vm_map_entry_t *));

struct vmspace *uvmspace_fork __P((struct vmspace *));

#endif /* _UVM_UVM_MAP_H_ */
