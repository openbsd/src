/*
 * OpenBSD/alpha machine-dependent thread macros
 *
 * $OpenBSD: uthread_machdep.h,v 1.4 1999/02/04 23:37:39 niklas Exp $
 */

/* save the floating point state of a thread */
#define _thread_machdep_save_float_state(thr)				      \
do {									      \
	__asm__(							      \
	     "stt $f8,  ( 0 * 8) + %0\n\tstt $f9,  ( 1 * 8) + %0\n\t"	      \
	     "stt $f10, ( 2 * 8) + %0\n\tstt $f11, ( 3 * 8) + %0\n\t"	      \
	     "stt $f12, ( 4 * 8) + %0\n\tstt $f13, ( 5 * 8) + %0\n\t"	      \
	     "stt $f14, ( 6 * 8) + %0\n\tstt $f15, ( 7 * 8) + %0\n\t"	      \
	     "stt $f16, ( 8 * 8) + %0\n\tstt $f17, ( 9 * 8) + %0\n\t"	      \
	     "stt $f18, (10 * 8) + %0\n\tstt $f19, (11 * 8) + %0\n\t"	      \
	     "stt $f20, (12 * 8) + %0\n\tstt $f21, (13 * 8) + %0\n\t"	      \
	     "stt $f22, (14 * 8) + %0\n\tstt $f23, (15 * 8) + %0\n\t"	      \
	     "stt $f24, (16 * 8) + %0\n\tstt $f25, (17 * 8) + %0\n\t"	      \
	     "stt $f26, (18 * 8) + %0\n\tstt $f27, (19 * 8) + %0\n\t"	      \
	     "stt $f28, (20 * 8) + %0\n\tstt $f29, (21 * 8) + %0\n\t"	      \
	     "stt $f30, (22 * 8) + %0" 			      \
	     : : "o" ((thr)->_machdep.saved_fp) : "memory");		      \
} while(0)

/* restore the floating point state of a thread */
#define _thread_machdep_restore_float_state(thr)			      \
do {									      \
	__asm__(							      \
	     "ldt $f8,  ( 0 * 8) + %0\n\tldt $f9,  ( 1 * 8) + %0\n\t"	      \
	     "ldt $f10, ( 2 * 8) + %0\n\tldt $f11, ( 3 * 8) + %0\n\t"	      \
	     "ldt $f12, ( 4 * 8) + %0\n\tldt $f13, ( 5 * 8) + %0\n\t"	      \
	     "ldt $f14, ( 6 * 8) + %0\n\tldt $f15, ( 7 * 8) + %0\n\t"	      \
	     "ldt $f16, ( 8 * 8) + %0\n\tldt $f17, ( 9 * 8) + %0\n\t"	      \
	     "ldt $f18, (10 * 8) + %0\n\tldt $f19, (11 * 8) + %0\n\t"	      \
	     "ldt $f20, (12 * 8) + %0\n\tldt $f21, (13 * 8) + %0\n\t"	      \
	     "ldt $f22, (14 * 8) + %0\n\tldt $f23, (15 * 8) + %0\n\t"	      \
	     "ldt $f24, (16 * 8) + %0\n\tldt $f25, (17 * 8) + %0\n\t"	      \
	     "ldt $f26, (18 * 8) + %0\n\tldt $f27, (19 * 8) + %0\n\t"	      \
	     "ldt $f28, (20 * 8) + %0\n\tldt $f29, (21 * 8) + %0\n\t"	      \
	     "ldt $f30, (22 * 8) + %0\n\t"				      \
	     : : "o" (thr->_machdep.saved_fp) :				      \
	     "$f8",  "$f9",  "$f10", "$f11", "$f12", "$f13", "$f14", "$f15",  \
	     "$f16", "$f17", "$f18", "$f19", "$f20", "$f21", "$f22", "$f23",  \
	     "$f24", "$f25", "$f26", "$f27", "$f28", "$f29", "$f30"); \
} while(0)

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

#define _thread_machdep_longjmp(a,v)    _longjmp(a,v)
#define _thread_machdep_setjmp(a)       _setjmp(a)

typedef jmp_buf _machdep_jmp_buf;

struct _machdep_struct {
	char		saved_fp[23 * 8];
};
