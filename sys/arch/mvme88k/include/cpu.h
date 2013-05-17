/* $OpenBSD: cpu.h,v 1.46 2013/05/17 22:40:01 miod Exp $ */
/*
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */
#ifndef	_MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

#include <m88k/cpu.h>

#ifdef _KERNEL

extern int cpuspeed;

/* board dependent pointers */
extern void	(*md_interrupt_func_ptr)(struct trapframe *);
#define	md_interrupt_func	(*md_interrupt_func_ptr)
extern int	(*md_nmi_func_ptr)(struct trapframe *);
#define	md_nmi_func		(*md_nmi_func_ptr)
extern void	(*md_nmi_wrapup_func_ptr)(struct trapframe *);
#define	md_nmi_wrapup_func	(*md_nmi_wrapup_func_ptr)
extern u_int	(*md_getipl)(void);
extern u_int	(*md_setipl)(u_int);
extern u_int	(*md_raiseipl)(u_int);
extern void	(*md_init_clocks)(void);
extern void	(*md_send_ipi)(int, cpuid_t);
extern void	(*md_delay)(int);
extern void	(*md_smp_setup)(struct cpu_info *);
__dead void doboot(void);
void	nmihand(void *);

#endif /* _KERNEL */

#endif
