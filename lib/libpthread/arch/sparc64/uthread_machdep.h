/*	$OpenBSD: uthread_machdep.h,v 1.3 2003/01/24 21:05:45 jason Exp $	*/
/* Arutr Grabowski <art@openbsd.org>. Public domain. */

struct _machdep_state {
	long	fp;		/* frame pointer */
	long	pc;		/* program counter */

	/* floating point state */
	u_int64_t	fs_fprs;	/* fp register window status */
	u_int64_t	fs_fsr;		/* fp status */
	u_int64_t	fs_regs[32];	/* 32 64 bit registers */
};

extern void _thread_machdep_fpsave(u_int64_t *);
extern void _thread_machdep_fprestore(u_int64_t *);
