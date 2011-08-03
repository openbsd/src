/*	$OpenBSD: uthread_machdep.c,v 1.6 2011/08/03 20:19:46 miod Exp $	*/

/*
 * Copyright (c) 2004 Dale Rahn. All rights reserved.
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
 */ 

#include <pthread.h>
#include "pthread_private.h"

#define ALIGNBYTES	0x7

/* Register save frame as it appears on the stack */
struct frame {
	int     r[12-4];
	int     fp; /* r12 */
	int     ip; /* r13 */
	int     lr; /* r14 */
	int	cpsr;
	double    fpr[6]; /* sizeof(fp)+sizeof(fs) == 52 */
	int	fs;
	/* The rest are only valid in the initial frame */
	int     next_fp;
	int     next_ip;
	int     next_lr;
	int	oldpc;
};

/*
 * Given a stack and an entry function, initialise a state
 * structure that can be later switched to.
 */
void
_thread_machdep_init(statep, base, len, entry)
	struct _machdep_state* statep;
	void *base;
	int len;
	void (*entry)(void);
{
	struct frame *f;
	int cpsr;

	/* Locate the initial frame, aligned at the top of the stack */
	f = (struct frame *)(((int)base + len - sizeof *f) & ~ALIGNBYTES);
	
	f->fp = (int)&f->next_fp;
	f->ip = (int)0;
	f->lr = (int)entry;
	f->next_fp = 0;		/* for gdb */
	f->next_lr = 0;		/* for gdb */

	/* Initialise the new thread with all the state from this thread. */

	__asm__ volatile ("mrs	%0, cpsr_all" : "=r" (cpsr));
	f->cpsr = cpsr;

	__asm__ volatile ("stmia %0, {r4-r12}":: "r"(&f->r[0]));

#ifndef __SOFTFP__
	__asm__ volatile ("sfm f4, 4, [%0], #0":: "r"(&f->fpr[0]));

	__asm__ volatile ("rfs 0; stfd 0, %0" : "=m"(f->fs));
#endif

	statep->frame = (int)f;
}


/*
 * No-op float saves.
 * (Floating point registers were saved in _thread_machdep_switch())
 */

void
_thread_machdep_save_float_state(statep)
	struct _machdep_state* statep;
{
#if !defined(__SOFTFP__) && !defined (__lint__)
#error finish FP save
#endif
}

void
_thread_machdep_restore_float_state(statep)
	struct _machdep_state* statep;
{
#if !defined(__SOFTFP__) && !defined (__lint__)
#error finish FP save
#endif
}
