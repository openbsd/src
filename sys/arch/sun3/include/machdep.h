/*	$OpenBSD: machdep.h,v 1.18 2002/03/14 03:16:01 millert Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
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
 *	from: Utah Hdr: cpu.h 1.16 91/03/25
 *	from: @(#)cpu.h	7.7 (Berkeley) 6/27/91
 *	cpu.h,v 1.2 1993/05/22 07:58:17 cgd Exp
 */

#ifdef _KERNEL

#define	FC_CONTROL	3	/* sun control space
				   XXX HP uses FC_PURGE instead */

/* Prototypes... */

struct frame;
struct fpframe;
struct pcb;
struct proc;
struct reg;
struct trapframe;
struct pmap;

extern int cache_size;
extern int fputype;

extern label_t *nofault;

extern vm_offset_t vmmap;	/* XXX - See mem.c */

/* Kernel virtual address space available: */
extern vm_offset_t virtual_avail, virtual_end;
/* Physical address space available: */
extern vm_offset_t avail_start, avail_end;
/* The "hole" (used to skip the Sun3/50 video RAM) */
extern vm_offset_t hole_start, hole_size;

void	ICIA(void);
void	DCIA(void);
void	DCIU(void);

void	cache_enable(void);
void	cache_flush_page(vm_offset_t pgva);
void	cache_flush_segment(vm_offset_t sgva);
void	cache_flush_context(void);

int 	cachectl(int req, caddr_t addr, int len);

void	cninit(void);

void	dumpconf(void);
void	dumpsys(void);

void	fb_unblank(void);

int 	fpu_emulate(struct frame *, struct fpframe *);

/* Backward compatibility... */
#define getsr	_getsr

void  **getvbr(void);

vm_offset_t high_segment_alloc(int npages);

void	initfpu(void);

void	intreg_init(void);

void	isr_init(void);
void	isr_config(void);

void	m68881_save(struct fpframe *);
void	m68881_restore(struct fpframe *);

void	proc_do_uret(void);
void	proc_trampoline(void);

void	pmap_bootstrap(void);
vm_offset_t pmap_map(vm_offset_t, vm_offset_t, vm_offset_t, int);
int 	pmap_fault_reload(struct pmap *, vm_offset_t, int);
void	pmap_get_ksegmap(u_char *);
void	pmap_get_pagemap(int *pt, int off);

int	reboot2(int, char *);

void	savectx(struct pcb *);

void	setvbr(void **);

void	sun3_mon_abort(void);
void	sun3_mon_halt(void);
void	sun3_mon_reboot(char *);
void	sun3_pmeg_init(void);
void	sun3_reserve_pmeg(int pmeg_num);

void	swapconf(void);
void	swapgeneric(void);

void	switch_exit(struct proc *);

#endif	/* _KERNEL */
