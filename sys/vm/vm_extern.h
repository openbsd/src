/*	$OpenBSD: vm_extern.h,v 1.18 2000/03/13 14:33:42 art Exp $	*/
/*	$NetBSD: vm_extern.h,v 1.20 1996/04/23 12:25:23 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)vm_extern.h	8.5 (Berkeley) 5/3/95
 */

struct buf;
struct loadavg;
struct proc;
struct vmspace;
struct vmtotal;
struct mount;
struct vnode;
struct core;

#if defined(KGDB) && !defined(UVM)
void		chgkprot __P((caddr_t, int, int));
#endif

#ifdef _KERNEL
#ifdef TYPEDEF_FOR_UAP
int		compat_43_getpagesize __P((struct proc *p, void *, int *));
int		madvise __P((struct proc *, void *, int *));
int		mincore __P((struct proc *, void *, int *));
int		mprotect __P((struct proc *, void *, int *));
int		minherit __P((struct proc *, void *, int *));
int		msync __P((struct proc *, void *, int *));
int		munmap __P((struct proc *, void *, int *));
int		obreak __P((struct proc *, void *, int *));
int		sbrk __P((struct proc *, void *, int *));
int		smmap __P((struct proc *, void *, int *));
int		sstk __P((struct proc *, void *, int *));
#endif

void		assert_wait __P((void *, boolean_t));
void		iprintf __P((int (*)(const char *, ...), const char *, ...));
#if !defined(UVM)
int		grow __P((struct proc *, vm_offset_t));
int		kernacc __P((caddr_t, int, int));
vm_offset_t	kmem_alloc __P((vm_map_t, vm_size_t));
vm_offset_t	kmem_alloc_pageable __P((vm_map_t, vm_size_t));
vm_offset_t	kmem_alloc_wait __P((vm_map_t, vm_size_t));
void		kmem_free __P((vm_map_t, vm_offset_t, vm_size_t));
void		kmem_free_wakeup __P((vm_map_t, vm_offset_t, vm_size_t));
void		kmem_init __P((vm_offset_t, vm_offset_t));
vm_offset_t	kmem_malloc __P((vm_map_t, vm_size_t, boolean_t));
vm_map_t	kmem_suballoc __P((vm_map_t, vm_offset_t *, vm_offset_t *,
		    vm_size_t, boolean_t));
void		loadav __P((struct loadavg *));
void		munmapfd __P((struct proc *, int));
int		pager_cache __P((vm_object_t, boolean_t));
void		sched __P((void));
#ifdef __GNUC__
void		scheduler __P((void)) __attribute ((noreturn));
#else
void		scheduler __P((void));
#endif
int		svm_allocate __P((struct proc *, void *, int *));
int		svm_deallocate __P((struct proc *, void *, int *));
int		svm_inherit __P((struct proc *, void *, int *));
int		svm_protect __P((struct proc *, void *, int *));
void		swapinit __P((void));
void		swapout __P((struct proc *));
void		swapout_threads __P((void));
int		swfree __P((struct proc *, int));
#endif /* !UVM */
void		swstrategy __P((struct buf *));
void		thread_block __P((char *));
void		thread_sleep_msg __P((void *, simple_lock_t,
		    boolean_t, char *, int));


/* backwards compatibility */
#define		thread_sleep(event, lock, ruptible) \
	thread_sleep_msg((event), (lock), (ruptible), "thrd_sleep")

/*
 * This define replaces a thread_wakeup prototype, as thread_wakeup
 * was solely a wrapper around wakeup.
 */
#define thread_wakeup wakeup
#if !defined(UVM)
int		useracc __P((caddr_t, int, int));
int		vm_allocate __P((vm_map_t, vm_offset_t *, vm_size_t,
		    boolean_t));
int		vm_allocate_with_pager __P((vm_map_t, vm_offset_t *,
		    vm_size_t, boolean_t, vm_pager_t, vm_offset_t, boolean_t));
int		vm_coredump __P((struct proc *, struct vnode *, struct ucred *,
		    struct core *));
int		vm_deallocate __P((vm_map_t, vm_offset_t, vm_size_t));
int		vm_fault __P((vm_map_t, vm_offset_t, vm_prot_t, boolean_t));
void		vm_fault_copy_entry __P((vm_map_t,
		    vm_map_t, vm_map_entry_t, vm_map_entry_t));
void		vm_fault_unwire __P((vm_map_t, vm_offset_t, vm_offset_t));
int		vm_fault_wire __P((vm_map_t, vm_offset_t, vm_offset_t));
#ifdef __FORK_BRAINDAMAGE
int		vm_fork __P((struct proc *, struct proc *, void *, size_t));
#else
void		vm_fork __P((struct proc *, struct proc *, void *, size_t));
#endif
int		vm_inherit __P((vm_map_t,
		    vm_offset_t, vm_size_t, vm_inherit_t));
void		vm_init_limits __P((struct proc *));
void		vm_mem_init __P((void));
int		vm_mmap __P((vm_map_t, vm_offset_t *, vm_size_t,
		    vm_prot_t, vm_prot_t, int, caddr_t, vm_offset_t));
int		vm_protect __P((vm_map_t,
		    vm_offset_t, vm_size_t, boolean_t, vm_prot_t));
void		vm_set_page_size __P((void));
void		vmmeter __P((void));
struct vmspace	*vmspace_alloc __P((vm_offset_t, vm_offset_t, int));
struct vmspace	*vmspace_fork __P((struct vmspace *));
void		vmspace_free __P((struct vmspace *));
void		vmtotal __P((struct vmtotal *));
void		vnode_pager_setsize __P((struct vnode *, u_long));
void		vnode_pager_umount __P((struct mount *));
boolean_t	vnode_pager_uncache __P((struct vnode *));
int		vslock __P((caddr_t, u_int));
int		vsunlock __P((caddr_t, u_int));
#endif /* !UVM */

/* Machine dependent portion */
void		vmapbuf __P((struct buf *, vsize_t));
void		vunmapbuf __P((struct buf *, vsize_t));
void		pagemove __P((caddr_t, caddr_t, size_t));
#ifdef __FORK_BRAINDAMAGE
int		cpu_fork __P((struct proc *, struct proc *, void *, size_t));
#else
void		cpu_fork __P((struct proc *, struct proc *, void *, size_t));
#endif
#ifndef	cpu_swapin
void		cpu_swapin __P((struct proc *));
#endif
#ifndef	cpu_swapout
void		cpu_swapout __P((struct proc *));
#endif

#endif
