/*	$OpenBSD: timerreg.h,v 1.4 2001/07/04 08:09:25 niklas Exp $	*/

struct ticktimer {
	u_int ttcmpreg;		/* Timer compare register */
	u_int ttcounter;	/* Timer counter */
	u_int tticr;		/* Timer control register */
};

struct timers {
};
