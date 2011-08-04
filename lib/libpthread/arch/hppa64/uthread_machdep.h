/*	$OpenBSD: uthread_machdep.h,v 1.1 2011/08/04 14:23:36 kettenis Exp $	*/

struct _machdep_state {
	u_long	sp;
	u_long	fp;
	u_int64_t fpregs[32];
};
