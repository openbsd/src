/*	$OpenBSD: vm86.h,v 1.10 2011/06/25 19:20:41 jsg Exp $	*/
/*	$NetBSD: vm86.h,v 1.8 1996/05/03 19:26:32 christos Exp $	*/

#undef	VM86_USE_VIF

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John T. Kohl and Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define SETFLAGS(targ, new, newmask) (targ) = ((targ) & ~(newmask)) | ((new) & (newmask))

#define VM86_TYPE(x)	((x) & 0xff)
#define VM86_ARG(x)	(((x) & 0xff00) >> 8)
#define	VM86_MAKEVAL(type,arg) ((type) | (((arg) & 0xff) << 8))
#define		VM86_STI	0
#define		VM86_INTx	1
#define		VM86_SIGNAL	2
#define		VM86_UNKNOWN	3

#define	VM86_REALFLAGS	(~PSL_USERSTATIC)
#define	VM86_VIRTFLAGS	(PSL_USERSTATIC & ~(PSL_MBO | PSL_MBZ))

struct vm86_regs {
	struct sigcontext vmsc;
};

struct vm86_kern {			/* kernel uses this stuff */
	struct vm86_regs regs;
	unsigned long ss_cpu_type;
};
#define cpu_type substr.ss_cpu_type

/*
 * Kernel keeps copy of user-mode address of this, but doesn't copy it in.
 */
struct vm86_struct {
	struct vm86_kern substr;
	unsigned long screen_bitmap;	/* not used/supported (yet) */
	unsigned long flags;		/* not used/supported (yet) */
	unsigned char int_byuser[32];	/* 256 bits each: pass control to user */
	unsigned char int21_byuser[32];	/* otherwise, handle directly */
};

#define VCPU_086		0
#define VCPU_186		1
#define VCPU_286		2
#define VCPU_386		3
#define VCPU_486		4
#define VCPU_586		5

#ifdef _KERNEL
int i386_vm86(struct proc *, char *, register_t *);
void vm86_gpfault(struct proc *, int);
void vm86_return(struct proc *, int);
static __inline void clr_vif(struct proc *);
static __inline void set_vif(struct proc *);
static __inline void set_vflags(struct proc *, int);
static __inline int get_vflags(struct proc *);
static __inline void set_vflags_short(struct proc *, int);
static __inline int get_vflags_short(struct proc *);

static __inline void
clr_vif(p)
	struct proc *p;
{
	struct pcb *pcb = &p->p_addr->u_pcb;

#ifndef VM86_USE_VIF
	pcb->vm86_eflags &= ~PSL_I;
#else
	pcb->vm86_eflags &= ~PSL_VIF;
#endif
}

static __inline void
set_vif(struct proc *p)
{
	struct pcb *pcb = &p->p_addr->u_pcb;

#ifndef VM86_USE_VIF
	pcb->vm86_eflags |= PSL_I;
	if ((pcb->vm86_eflags & (PSL_I|PSL_VIP)) == (PSL_I|PSL_VIP))
#else
	pcb->vm86_eflags |= PSL_VIF;
	if ((pcb->vm86_eflags & (PSL_VIF|PSL_VIP)) == (PSL_VIF|PSL_VIP))
#endif
		vm86_return(p, VM86_STI);
}

static __inline void
set_vflags(struct proc *p, int flags)
{
	struct trapframe *tf = p->p_md.md_regs;
	struct pcb *pcb = &p->p_addr->u_pcb;

	flags &= ~pcb->vm86_flagmask;
	SETFLAGS(pcb->vm86_eflags, flags, VM86_VIRTFLAGS);
	SETFLAGS(tf->tf_eflags, flags, VM86_REALFLAGS);
#ifndef VM86_USE_VIF
	if ((pcb->vm86_eflags & (PSL_I|PSL_VIP)) == (PSL_I|PSL_VIP))
#else
	if ((pcb->vm86_eflags & (PSL_VIF|PSL_VIP)) == (PSL_VIF|PSL_VIP))
#endif
		vm86_return(p, VM86_STI);
}

static __inline int
get_vflags(struct proc *p)
{
	struct trapframe *tf = p->p_md.md_regs;
	struct pcb *pcb = &p->p_addr->u_pcb;
	int flags = PSL_MBO;

	SETFLAGS(flags, pcb->vm86_eflags, VM86_VIRTFLAGS);
	SETFLAGS(flags, tf->tf_eflags, VM86_REALFLAGS);
	return (flags);
}

static __inline void
set_vflags_short(struct proc *p, int flags)
{
	struct trapframe *tf = p->p_md.md_regs;
	struct pcb *pcb = &p->p_addr->u_pcb;

	flags &= ~pcb->vm86_flagmask;
	SETFLAGS(pcb->vm86_eflags, flags, VM86_VIRTFLAGS & 0xffff);
	SETFLAGS(tf->tf_eflags, flags, VM86_REALFLAGS & 0xffff);
#ifndef VM86_USE_VIF
	if ((pcb->vm86_eflags & (PSL_I|PSL_VIP)) == (PSL_I|PSL_VIP))
		vm86_return(p, VM86_STI);
#endif
}

static __inline int
get_vflags_short(struct proc *p)
{
	struct trapframe *tf = p->p_md.md_regs;
	struct pcb *pcb = &p->p_addr->u_pcb;
	int flags = PSL_MBO;

	SETFLAGS(flags, pcb->vm86_eflags, VM86_VIRTFLAGS & 0xffff);
	SETFLAGS(flags, tf->tf_eflags, VM86_REALFLAGS & 0xffff);
	return (flags);
}
#else
int i386_vm86(struct vm86_struct *vmcp);
#endif
