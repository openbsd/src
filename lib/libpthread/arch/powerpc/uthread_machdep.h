/*
 * OpenBSD/powerpc machine-dependent thread macros
 *
 * $OpenBSD: uthread_machdep.h,v 1.4 1999/11/25 07:01:29 d Exp $
 */

/* save the floating point state of a thread */
#define _thread_machdep_save_float_state(thr) 		\
	{						\
		/* rahnds to fill in */			\
	}

/* restore the floating point state of a thread */
#define _thread_machdep_restore_float_state(thr) 	\
	{						\
		/* rahnds to fill in */			\
	}

#define JMP_r1  (0x04/4)
#define JMP_lr  (0x50/4)
/* initialise the jmpbuf stack frame so it continues from entry */
#define _thread_machdep_thread_create(thr, entry, pattr)	\
	{						\
		(thr)->saved_jmp_buf[JMP_lr] =		\
			(unsigned int) entry;		\
		(thr)->saved_jmp_buf[JMP_r1] =		\
			((unsigned int) (thr)->stack->base	\
			+ (thr)->stack->size		\
			- 0x4) & ~0xf;			\
		{					\
			unsigned int *pbacklink =	\
				(thr)->saved_jmp_buf[JMP_r1]; \
			*pbacklink = 0;			\
		}					\
	}

#define	_thread_machdep_longjmp(a,v)	longjmp(a,v)
#define	_thread_machdep_setjmp(a)	setjmp(a)

typedef jmp_buf _machdep_jmp_buf;

struct _machdep_struct {
        char            xxx;
};

