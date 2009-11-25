/*	$OpenBSD: uvm_object.h,v 1.16 2009/11/25 19:11:38 oga Exp $	*/
/*	$NetBSD: uvm_object.h,v 1.11 2001/03/09 01:02:12 chs Exp $	*/

/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
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
 *
 * from: Id: uvm_object.h,v 1.1.2.2 1998/01/04 22:44:51 chuck Exp
 */

#ifndef _UVM_UVM_OBJECT_H_
#define _UVM_UVM_OBJECT_H_

/*
 * uvm_object.h
 */

/*
 * uvm_object: all that is left of mach objects.
 */

struct uvm_object {
	simple_lock_data_t		 vmobjlock;	/* lock on memt */
	struct uvm_pagerops		*pgops;		/* pager ops */
	RB_HEAD(uvm_objtree, vm_page)	 memt;		/* pages in object */
	int				 uo_npages;	/* # of pages in memt */
	int				 uo_refs;	/* reference count */
};

/*
 * UVM_OBJ_KERN is a 'special' uo_refs value which indicates that the
 * object is a kernel memory object rather than a normal one (kernel
 * memory objects don't have reference counts -- they never die).
 *
 * this value is used to detected kernel object mappings at uvm_unmap()
 * time.   normally when an object is unmapped its pages eventaully become
 * deactivated and then paged out and/or freed.    this is not useful
 * for kernel objects... when a kernel object is unmapped we always want
 * to free the resources associated with the mapping.   UVM_OBJ_KERN
 * allows us to decide which type of unmapping we want to do.
 *
 * in addition, we have kernel objects which may be used in an
 * interrupt context.  these objects get their mappings entered
 * with pmap_kenter*() and removed with pmap_kremove(), which
 * are safe to call in interrupt context, and must be used ONLY
 * for wired kernel mappings in these objects and their associated
 * maps.
 */
#define UVM_OBJ_KERN		(-2)

#define	UVM_OBJ_IS_KERN_OBJECT(uobj)					\
	((uobj)->uo_refs == UVM_OBJ_KERN)

#ifdef _KERNEL

extern struct uvm_pagerops uvm_vnodeops;
extern struct uvm_pagerops uvm_deviceops;

/* For object trees */
int	uvm_pagecmp(struct vm_page *, struct vm_page *);
RB_PROTOTYPE(uvm_objtree, vm_page, objt, uvm_pagecmp)

#define	UVM_OBJ_IS_VNODE(uobj)						\
	((uobj)->pgops == &uvm_vnodeops)

#define UVM_OBJ_IS_DEVICE(uobj)						\
	((uobj)->pgops == &uvm_deviceops)

#define	UVM_OBJ_IS_VTEXT(uobj)						\
	((uobj)->pgops == &uvm_vnodeops &&				\
	 ((struct vnode *)uobj)->v_flag & VTEXT)


int	uvm_objwire(struct uvm_object *, off_t, off_t, struct pglist *);
void	uvm_objunwire(struct uvm_object *, off_t, off_t);

#endif /* _KERNEL */

#endif /* _UVM_UVM_OBJECT_H_ */
