/* $OpenBSD: vfp.h,v 1.1 2016/12/17 23:38:33 patrick Exp $ */
/*
 * Copyright (c) 2012 Mark Tinguely
 *
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * $FreeBSD$
 */


#ifndef _MACHINE__VFP_H_
#define _MACHINE__VFP_H_

#ifdef _KERNEL
/* Only kernel defines exist here */

#define VFP_KFPEN	(1 << 20)
#define VFP_UFPEN	(3 << 20)

void		vfp_init(void);
void		vfp_discard(struct proc *p);
void		vfp_save(void);
void		vfp_enable(void);
int vfp_fault(vaddr_t pc, uint32_t insn, trapframe_t *tf, int fault_code);

#endif /* _KERNEL */
#endif /* _MACHINE__VFP_H_ */
