/*	$NetBSD: vm86.h,v 1.1 1996/01/08 13:51:45 mycroft Exp $	*/

/*
 *  Copyright (c) 1995 John T. Kohl
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#define SETFLAGS(targ, new, newmask) (targ) = ((targ) & ~(newmask)) | ((new) & (newmask))
#define VM86_EFLAGS(p)	((p)->p_addr->u_pcb.vm86_eflags)
#define VM86_FLAGMASK(p) ((p)->p_addr->u_pcb.vm86_flagmask)

#define VM86_TYPE(x)	((x) & 0xff)
#define VM86_ARG(x)	(((x) & 0xff00) >> 8)
#define	VM86_MAKEVAL(type,arg) ((type) | (((arg) & 0xff) << 8))
#define		VM86_STI	0
#define		VM86_INTx	1
#define		VM86_SIGNAL	2
#define		VM86_UNKNOWN	3

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

#define BIOSSEG		0x0f000

#define VCPU_086		0
#define VCPU_186		1
#define VCPU_286		2
#define VCPU_386		3
#define VCPU_486		4
#define VCPU_586		5

#ifdef _KERNEL
int i386_vm86 __P((struct proc *, char *, register_t *));
void vm86_gpfault __P((struct proc *, int));
#else
int i386_vm86 __P((struct vm86_struct *vmcp));
#endif
