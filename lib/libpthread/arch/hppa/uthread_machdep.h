/*	$OpenBSD: uthread_machdep.h,v 1.6 2002/11/01 00:05:45 mickey Exp $	*/

struct _machdep_state {
	u_long	sp;
	u_long	fp;
	u_int64_t fpregs[32];
};
