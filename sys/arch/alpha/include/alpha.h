/* $NetBSD: alpha.h,v 1.11 2000/08/15 22:16:18 thorpej Exp $ */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: cpu.h 1.16 91/03/25$
 *
 *	@(#)cpu.h	8.4 (Berkeley) 1/5/94
 */

#ifndef _ALPHA_H_
#define _ALPHA_H_
#ifdef _KERNEL

#include <machine/bus.h>

struct pcb;
struct proc;
struct reg;
struct rpb;
struct trapframe;

extern int bootdev_debug;

void	XentArith(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	XentIF(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	XentInt(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	XentMM(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	XentRestart(void);					/* MAGIC */
void	XentSys(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	XentUna(u_int64_t, u_int64_t, u_int64_t);		/* MAGIC */
void	alpha_init(u_long, u_long, u_long, u_long, u_long);
int	alpha_pa_access(u_long);
void	ast(struct trapframe *);
int	badaddr(void *, size_t);
int	badaddr_read(void *, size_t, void *);
void	child_return(void *);
u_int64_t console_restart(struct trapframe *);
void	do_sir(void);
void	dumpconf(void);
void	exception_return(void);					/* MAGIC */
void	frametoreg(struct trapframe *, struct reg *);
long	fswintrberr(void);					/* MAGIC */
void	init_bootstrap_console(void);
void	init_prom_interface(struct rpb *);
void	interrupt(unsigned long, unsigned long, unsigned long,
	    struct trapframe *);
void	machine_check(unsigned long, struct trapframe *, unsigned long,
	    unsigned long);
u_int64_t hwrpb_checksum(void);
void	hwrpb_restart_setup(void);
void	regdump(struct trapframe *);
void	regtoframe(struct reg *, struct trapframe *);
void	savectx(struct pcb *);
void    switch_exit(struct proc *);				/* MAGIC */
void	switch_trampoline(void);				/* MAGIC */
void	syscall(u_int64_t, struct trapframe *);
void	trap(unsigned long, unsigned long, unsigned long, unsigned long,
	    struct trapframe *);
void	trap_init(void);
void	enable_nsio_ide(bus_space_tag_t);
char *	dot_conv(unsigned long);

void	release_fpu(int);
void	synchronize_fpstate(struct proc *, int);

/* Multiprocessor glue; cpu.c */
struct cpu_info;
int	cpu_iccb_send(long, const char *);
void	cpu_iccb_receive(void);
void	cpu_hatch(struct cpu_info *);
void	cpu_halt_secondary(unsigned long);
void	cpu_spinup_trampoline(void);				/* MAGIC */
void	cpu_pause(unsigned long);
void	cpu_resume(unsigned long);

#endif /* _KERNEL */
#endif /* _ALPHA_H_ */
