/*
 * OpenBSD/sparc machine-dependent thread macros
 *
 * $OpenBSD: uthread_machdep.h,v 1.1 1998/11/20 11:15:37 d Exp $
 */

#include <sys/signal.h>

/* save the floating point state of a thread */
#define _thread_machdep_save_float_state(thr) 		\
	{						\
		/* XXX tdb */		\
	}

/* restore the floating point state of a thread */
#define _thread_machdep_restore_float_state(thr) 	\
	{						\
		/* XXX tdb */		\
	}

/* initialise the jmpbuf stack frame so it continues from entry */

#define _thread_machdep_thread_create(thr, entry, pattr)	\
	{						\
	    /* entry */					\
	    (thr)->saved_jmp_buf[1] = (long) entry;	\
	    /* stack */					\
	    (thr)->saved_jmp_buf[0] = (long) (thr)->stack \
				+ (pattr)->stacksize_attr \
				- sizeof(double);	\
	}

/*
 * XXX high chance of longjmp botch (see libc/arch/sparc/gen/_setjmp.S)
 * because it uses the frame pointer to pop off frames.. we don't want
 * that.. what to do? fudge %fp? do our own setjmp?
 */
#define	_thread_machdep_longjmp(a,v)	_longjmp(a,v)
#define	_thread_machdep_setjmp(a)	_setjmp(a)

struct _machdep_struct {
        /* char            saved_fp[???]; */
	int	dummy;
};

