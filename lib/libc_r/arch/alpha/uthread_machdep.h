/*
 * OpenBSD/alpha machine-dependent thread macros
 *
 * $OpenBSD: uthread_machdep.h,v 1.1 1998/08/28 01:54:57 d Exp $
 */

/* save the floating point state of a thread */
#define _thread_machdep_save_float_state(thr) 		\
	{						\
	    char *fdata = (char*)((thr)->_machdep.saved_fp);	\
	    __asm__("fsave %0"::"m" (*fdata));		\
	}

/* restore the floating point state of a thread */
#define _thread_machdep_restore_float_state(thr) 	\
	{						\
	    char *fdata = (char*)((thr)->_machdep.saved_fp);	\
	    __asm__("frstor %0"::"m" (*fdata));		\
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

struct _machdep_struct {
	char		saved_fp[108];
};
