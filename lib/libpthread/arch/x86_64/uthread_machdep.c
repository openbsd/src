#include <machine/param.h>
#include <pthread.h>
#include "pthread_private.h"

struct frame {
	long	fr_gs;
	long	fr_fs;
	long	fr_es;
	long	fr_ds;

	long    flag;
	long	fr_r15;
	long	fr_r14;
	long	fr_r13;
	long	fr_r12;

	long	fr_r11;
	long	fr_r10;
	long	fr_r9;
	long	fr_r8;

	long	fr_rdi;
	long	fr_rsi;
	long	fr_rbp;

	long	fr_rbx;
	long	fr_rdx;
	long	fr_rcx;
	long	fr_rax;

	long	fr_rip;
	int	fr_cs;		/* XXX unreachable? */
	int	pad;
};

#define copyreg(reg, lval) \
	__asm__("mov %%" #reg ", %0" : "=g"(lval))

/*
 * Given a stack and an entry function, initialise a state
 * structure that can be later switched to.
 */
void
_thread_machdep_init(struct _machdep_state* statep, void *base, int len,
    void (*entry)(void))
{
        struct frame *f;
	int foo;

	/* Locate the initial frame, aligned at the top of the stack */
	f = (struct frame *)(((long)base + len - sizeof *f) & ~ALIGNBYTES);

	copyreg(cs, foo);
	f->fr_cs = foo;
	copyreg(ds, foo);
	f->fr_ds = foo;
	copyreg(es, foo);
	f->fr_es = foo;
	copyreg(fs, foo);
	f->fr_fs = foo;
	copyreg(gs, foo);
	f->fr_gs = foo;

	f->fr_rbp = (long)-1;
	f->fr_rip = (long)entry;

	statep->rsp = (long)f;

	_thread_machdep_save_float_state(statep);
}

#define fxsave(addr)		__asm("fxsave %0" : "=m" (*addr))
#define fxrstor(addr)		__asm("fxrstor %0" : : "m" (*addr))
#define fwait()			__asm("fwait")
#define fninit()		__asm("fninit")

void
_thread_machdep_save_float_state(struct _machdep_state *ms)
{
	fxsave(&ms->fpreg);
	fninit();
	fwait();
}

void
_thread_machdep_restore_float_state(struct _machdep_state *ms)
{
	fxrstor(&ms->fpreg);
}
