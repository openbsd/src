/*
 * OpenBSD/i386 machine-dependent thread macros
 *
 * $OpenBSD: uthread_machdep.h,v 1.4 1999/01/17 23:49:49 d Exp $
 */

#include <machine/reg.h>

/* 
 * We need to extend jmp_buf to hold extended segment registers
 * for WINE.
 */
typedef struct _machdep_jmp_buf {
	union {
		jmp_buf		jb;
		struct {
			int		eip;
			int		ebx;
			int		esp;
			int		ebp;
			int		esi;
			int		edi;
		} jbs;
	} u;
#define mjb_eip	u.jbs.eip
#define mjb_ebx	u.jbs.ebx
#define mjb_esp	u.jbs.esp
#define mjb_ebp	u.jbs.ebp
#define mjb_esi	u.jbs.esi
#define mjb_edi	u.jbs.edi
	int		mjb_ds;	/* informational only - not restored */
	int		mjb_es;	/* informational only - not restored */
	int		mjb_fs;
	int		mjb_gs;
} _machdep_jmp_buf;

/* Extra machine-dependent information */
struct _machdep_struct {
	struct fpreg	saved_fp;	/* only saved with on signals */
};

/* Save the floating point state of a thread: */
#define _thread_machdep_save_float_state(thr) 			\
	{							\
	    char *fdata = (char*)(&(thr)->_machdep.saved_fp);	\
	    __asm__("fsave %0"::"m" (*fdata));			\
	}

/* Restore the floating point state of a thread: */
#define _thread_machdep_restore_float_state(thr) 		\
	{							\
	    char *fdata = (char*)(&(thr)->_machdep.saved_fp);	\
	    __asm__("frstor %0"::"m" (*fdata));			\
	}

/* Initialise the jmpbuf stack frame so it continues from entry: */
#define _thread_machdep_thread_create(thr, entry, pattr)	\
	{							\
	    /* entry */						\
	    (thr)->saved_jmp_buf.mjb_eip = (long) entry;	\
	    /* stack */						\
	    (thr)->saved_jmp_buf.mjb_esp = (long) (thr)->stack	\
				+ (pattr)->stacksize_attr 	\
				- sizeof(double);		\
	}

static __inline int
_thread_machdep_setjmp_helper(a)
	_machdep_jmp_buf *a;
{
	int v;

	__asm__("mov %%ds, %0\n" : "=m" (a->mjb_ds) );
	__asm__("mov %%es, %0\n" : "=m" (a->mjb_es) );
	__asm__("mov %%fs, %0\n" : "=m" (a->mjb_fs) );
	__asm__("mov %%gs, %0\n" : "=m" (a->mjb_gs) );
	v = setjmp(a->u.jb);
	if (v) {
		__asm__("mov %0, %%gs\n" :: "m" (a->mjb_gs) );
		__asm__("mov %0, %%fs\n" :: "m" (a->mjb_fs) );
		/* __asm__("mov %0, %%es\n" :: "m" (a->mjb_es) ); */
		/* __asm__("mov %0, %%ds\n" :: "m" (a->mjb_ds) ); */
	}
	return (v);
}

#define _thread_machdep_longjmp(a,v)	longjmp((a).u.jb,v)
#define _thread_machdep_setjmp(a)	_thread_machdep_setjmp_helper(&(a))
