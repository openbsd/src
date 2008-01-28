/*	 $OpenBSD: uthread_machdep.c,v 1.6 2008/01/28 18:48:41 kettenis Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

/*
 * Machine-dependent thread state functions for OpenBSD/i386.
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
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

static int _thread_machdep_osfxsr(void);

static int
_thread_machdep_osfxsr(void)
{
	int mib[] = { CTL_MACHDEP, CPU_OSFXSR };
	static int sse = -1;
	size_t len;
	int val;

	if (sse == -1) {
		len = sizeof (val);
		if (sysctl(mib, 2, &val, &len, NULL, 0) == -1)
			return (0);
		if (val)
			sse = 1;
		else
			sse = 0;
	}
	return (sse);
}

/*
 * Given a stack and an entry function, initialise a state
 * structure that can be later switched to.
 */
void
_thread_machdep_init(struct _machdep_state* statep, void *base, int len,
		     void (*entry)(void))
{
	struct frame *f;

	/*
	 * Locate the initial frame at the top of the stack.  For the
	 * stack to end up properly (16-byte) aligned, we need to
	 * align the frame at an odd 8-byte boundary.
	 */
	f = (struct frame *)((((int)base + len - sizeof *f) & ~15) - 8);

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

	_thread_machdep_save_float_state(statep);
	/*
	 * The current thread float state is saved into the new thread stack.
	 * Later pthread_create calls _thread_kern_sched which saves the current
	 * thread float state again into its own stack. However all float state
	 * saves must be balanced with a restore on i386 due to the fninit().
	 * Restore the current thread float state here so that the next save
	 * gets the correct state. 
	 */
	_thread_machdep_restore_float_state(statep);
}

/*
 * Floating point save restore copied from code in npx.c
 * (without really understanding what it does).
 */
#define	fldcw(addr)		__asm("fldcw %0"  : : "m" (*addr))
#define	fnsave(addr)		__asm("fnsave %0" : "=m" (*addr))
#define	fninit()		__asm("fninit")
#define	frstor(addr)		__asm("frstor %0" : : "m" (*addr))
#define	fxsave(addr)		__asm("fxsave %0" : "=m" (*addr))
#define	fxrstor(addr)		__asm("fxrstor %0" : : "m" (*addr))
#define	fwait()			__asm("fwait")

void
_thread_machdep_save_float_state(struct _machdep_state *ms)
{
	union savefpu *addr = &ms->fpreg;

	if (_thread_machdep_osfxsr()) {
		fwait();
		fxsave(&addr->sv_xmm);
		fninit();
	} else
		fnsave(&addr->sv_87);
	fwait();
}

void
_thread_machdep_restore_float_state(struct _machdep_state *ms)
{
	union savefpu *addr = &ms->fpreg;

	if (_thread_machdep_osfxsr()) {
		fxrstor(&addr->sv_xmm);
		fwait();
	} else
		frstor(&addr->sv_87);

}
