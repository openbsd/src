/*
 * OpenBSD/alpha machine-dependent thread macros
 *
 * $OpenBSD: uthread_machdep.h,v 1.3 1999/02/03 05:25:01 d Exp $
 */

/* save the floating point state of a thread */
#define _thread_machdep_save_float_state(thr) 		\
	{						\
/*	    char *fdata = (char*)((thr)->_machdep.saved_fp);	\
	    __asm__("fsave %0"::"m" (*fdata)); */		\
	}

/* restore the floating point state of a thread */
#define _thread_machdep_restore_float_state(thr) 	\
	{						\
/*	    char *fdata = (char*)((thr)->_machdep.saved_fp);	\
	    __asm__("frstor %0"::"m" (*fdata));	*/	\
	}

/* initialise the jmpbuf stack frame so it continues from entry */
#define _thread_machdep_thread_create(thr, entry, pattr)	\
	{						\
	    /* entry */					\
	    (thr)->saved_jmp_buf[2] = (long) entry;	\
	    (thr)->saved_jmp_buf[4+R_RA] = 0;		\
	    (thr)->saved_jmp_buf[4+R_T12] = (long) entry; \
	    /* stack */					\
	    (thr)->saved_jmp_buf[4 + R_SP] = (long) (thr)->stack \
				+ (pattr)->stacksize_attr \
				- sizeof(double);	\
	}

#define _thread_machdep_longjmp(a,v)    longjmp(a,v)
#define _thread_machdep_setjmp(a)       setjmp(a)

typedef jmp_buf _machdep_jmp_buf;

struct _machdep_struct {
	char		saved_fp[108];
};
