/*
 * OpenBSD/sparc machine-dependent thread macros
 *
 * $OpenBSD: uthread_machdep.h,v 1.4 2000/01/06 07:04:54 d Exp $
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

typedef long _machdep_jmp_buf[2];

int _thread_machdep_setjmp __P((_machdep_jmp_buf));
void _thread_machdep_longjmp __P((_machdep_jmp_buf, int));

/* initialise the jmpbuf stack frame so it continues from entry */
#define _thread_machdep_thread_create(thr, entry, pattr)	\
	{							\
		long stack = (long)(thr)->stack->base +		\
			(thr)->stack->size - 64;		\
		(thr)->saved_jmp_buf[0] = (long)stack;		\
		(thr)->saved_jmp_buf[1] = (long)entry - 8;	\
	}

struct _machdep_struct {
        /* char            saved_fp[???]; */
	int	dummy;
};

