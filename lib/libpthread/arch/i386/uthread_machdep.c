/*	 $OpenBSD: uthread_machdep.c,v 1.1 2000/09/25 01:16:40 d Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

/*
 * Machine-dependent thread state functions for OpenBSD/sparc.
 */

#include <machine/param.h>
#include <pthread.h>
#include "pthread_private.h"

struct frame {
	int	fr_gs;
	int	fr_fs;
	int	fr_es;
	int	fr_ds;

	int	fr_edi;
	int	fr_esi;
	int	fr_ebp;
	int	fr_esp;
	int	fr_ebx;
	int	fr_edx;
	int	fr_ecx;
	int	fr_eax;

	int	fr_eip;
	int	fr_cs;		/* XXX unreachable? */
};

#define copyreg(reg, lval) \
	__asm__("mov %%" #reg ", %0" : "=g"(lval))

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

	/* Locate the initial frame, aligned at the top of the stack */
	f = (struct frame *)(((int)base + len - sizeof *f) & ~ALIGNBYTES);

	/* Set up initial frame */
	f->fr_esp = (int)&f->fr_edi;
	copyreg(cs, f->fr_cs);
	copyreg(ds, f->fr_ds);
	copyreg(es, f->fr_es);
	copyreg(fs, f->fr_fs);
	copyreg(gs, f->fr_gs);
	f->fr_ebp = (int)-1;
	f->fr_eip = (int)entry;

	statep->esp = (int)f;
}

void
_thread_machdep_save_float_state(ms)
	struct _machdep_state *ms;
{
	char *fdata = (char *)&ms->fpreg;

	__asm__("fsave %0"::"m" (*fdata));
}

void
_thread_machdep_restore_float_state(ms)
	struct _machdep_state *ms;
{
	char *fdata = (char *)&ms->fpreg;

	__asm__("frstor %0"::"m" (*fdata));
}

