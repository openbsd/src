
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

/*
 * Internal definitions unique to sun3/68k cpu support.
 * These are the "private" declarations - those needed
 * only here in machine-independent code.  The "public"
 * definitions are in cpu.h (used by common code).
 */

/* 680X0 function codes */
#define	FC_USERD	1	/* user data space */
#define	FC_USERP	2	/* user program space */
#define	FC_CONTROL	3	/* sun control space */
#define	FC_SUPERD	5	/* supervisor data space */
#define	FC_SUPERP	6	/* supervisor program space */
#define	FC_CPU		7	/* CPU space */

/* fields in the 68020 cache control register */
#define	IC_ENABLE	0x0001	/* enable instruction cache */
#define	IC_FREEZE	0x0002	/* freeze instruction cache */
#define	IC_CE		0x0004	/* clear instruction cache entry */
#define	IC_CLR		0x0008	/* clear entire instruction cache */

#define IC_CLEAR (IC_CLR|IC_ENABLE)


/* Prototypes... */

struct frame;
struct fpframe;
struct pcb;
struct proc;
struct reg;
struct trapframe;
struct pmap;

extern int cache_size;
extern int cold;
extern int fpu_type;

extern label_t *nofault;

extern vm_offset_t vmmap;	/* XXX - See mem.c */

/* Kernel virtual address space available: */
extern vm_offset_t virtual_avail, virtual_end;
/* Physical address space available: */
extern vm_offset_t avail_start, avail_end;
/* The "hole" (used to skip the Sun3/50 video RAM) */
extern vm_offset_t hole_start, hole_size;

void	ICIA __P((void));
void	DCIA __P((void));
void	DCIU __P((void));

void	cache_enable __P((void));
void	cache_flush_page(vm_offset_t pgva);
void	cache_flush_segment(vm_offset_t sgva);
void	cache_flush_context(void);

int 	cachectl __P((int req, caddr_t addr, int len));

void	child_return __P((struct proc *));

void	configure __P((void));
void	cninit __P((void));

void	dumpconf __P((void));
void	dumpsys __P((void));

int 	fpu_emulate __P((struct frame *, struct fpframe *));

int 	getdfc __P((void));
int 	getsfc __P((void));

void**	getvbr __P((void));

vm_offset_t high_segment_alloc __P((int npages));

void	initfpu __P((void));

void	isr_init __P((void));
void	isr_config __P((void));

void	m68881_save __P((struct fpframe *));
void	m68881_restore __P((struct fpframe *));

void	netintr __P((void));
void	proc_trampoline __P((void));

void	pmap_bootstrap __P((void));
int 	pmap_fault_reload __P((struct pmap *, vm_offset_t, int));
void	pmap_get_ksegmap __P((u_char *));
void	pmap_get_pagemap __P((int *pt, int off));

void	regdump __P((struct frame *, int));

void	savectx __P((struct pcb *));

void	setvbr __P((void **));

void	sun3_mon_abort __P((void));
void	sun3_mon_halt __P((void));
void	sun3_mon_reboot __P((char *));
void	sun3_pmeg_init __P((void));
void	sun3_reserve_pmeg __P((int pmeg_num));

void	swapconf __P((void));
void	swapgeneric __P((void));

void	switch_exit __P((struct proc *));

#endif	/* _KERNEL */
