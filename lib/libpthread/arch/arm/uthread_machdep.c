/*	$OpenBSD: uthread_machdep.c,v 1.2 2004/02/21 05:29:16 drahn Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain */

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
	int scratch;

	/* Locate the initial frame, aligned at the top of the stack */
	f = (struct frame *)(((int)base + len - sizeof *f) & ~ALIGNBYTES);
	
	f->fp = (int)&f->next_fp;
	f->ip = (int)0;
	f->lr = (int)entry;
	f->next_fp = 0;		/* for gdb */
	f->next_lr = 0;		/* for gdb */

	/* Initialise the new thread with all the state from this thread. */

	__asm__ volatile ("mrs	%0, cpsr_all; str %0, [%2, #0]"
	    : "=r"(scratch) : "0"(scratch), "r" (&f->cpsr));

	__asm__ volatile ("stmia %0, {r4-r12}":: "r"(&f->r[0]));

#ifndef __VFP_FP__
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
#ifndef __VFP_FP__
#error finish FP save
#endif
}

void
_thread_machdep_restore_float_state(statep)
	struct _machdep_state* statep;
{
#ifndef __VFP_FP__
#error finish FP save
#endif
}
