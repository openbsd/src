/*
 * OpenBSD/sparc machine-dependent thread macros
 *
 * $OpenBSD: uthread_machdep.h,v 1.3 1999/11/25 07:01:29 d Exp $
 */

#include <sys/signal.h>

/* save the floating point state of a thread */
#define _thread_machdep_save_float_state(thr) 		\
	{						\
		/* XXX tdb */				\
	}

/* restore the floating point state of a thread */
#define _thread_machdep_restore_float_state(thr) 	\
	{						\
		/* XXX tdb */				\
	}

/* initialise the jmpbuf stack frame so it continues from entry */

#define _thread_machdep_thread_create(thr, entry, pattr)	\
	{						\
	    /* entry */					\
	    (thr)->saved_jmp_buf[1] = (long) entry;	\
	    /* stack */					\
	    (thr)->saved_jmp_buf[0] = (long) (thr)->stack->base \
				+ (thr)->stack->size	\
				- sizeof(double);	\
	}

#define _thread_machdep_longjmp(a,v)	_longjmp(a,v)
#define _thread_machdep_setjmp(a)	_setjmp(a)
typedef jmp_buf _machdep_jmp_buf;

struct _machdep_struct {
        /* char            saved_fp[???]; */
	int	dummy;
};

