/*	$OpenBSD: uvm_vnode.h,v 1.9 2001/11/27 05:27:12 art Exp $	*/
/*	$NetBSD: uvm_vnode.h,v 1.9 2000/03/26 20:54:48 kleink Exp $	*/

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
 * from: Id: uvm_vnode.h,v 1.1.2.4 1997/10/03 21:18:24 chuck Exp
 */

#ifndef _UVM_UVM_VNODE_H_
#define _UVM_UVM_VNODE_H_

/*
 * uvm_vnode.h
 *
 * vnode handle into the VM system.
 */

/*
 * the uvm_vnode structure.   put at the top of the vnode data structure.
 * this allows:
 *   (struct vnode *) == (struct uvm_vnode *) == (struct uvm_object *)
 */

struct uvm_vnode {
	struct uvm_object u_obj;	/* the actual VM object */
	int u_flags;			/* flags */
	int u_nio;			/* number of running I/O requests */
	voff_t u_size;			/* size of object */
};

#endif /* _UVM_UVM_VNODE_H_ */
