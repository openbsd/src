/*	$OpenBSD: getsecs.c,v 1.1 1996/10/30 22:40:55 niklas Exp $	*/

#include <sys/param.h>
#include "include/prom.h"
#include "include/rpb.h"

int
getsecs()
{
	static long tnsec;
	static long lastpcc, wrapsecs;
	long curpcc, pccdiff;

	if (tnsec == 0) {
		tnsec = 1;
		lastpcc = alpha_rpcc() & 0xffffffff;
		wrapsecs = (0xffffffff /
		    ((struct rpb *)HWRPB_ADDR)->rpb_cc_freq) + 1;

#if 0
		printf("getsecs: cc freq = %d, time to wrap = %d\n",
		    ((struct rpb *)HWRPB_ADDR)->rpb_cc_freq, wrapsecs);
#endif
	}

	curpcc = alpha_rpcc() & 0xffffffff;
	if (curpcc < lastpcc)
		curpcc += 0x100000000;

	tnsec += ((curpcc - lastpcc) * 1000000000) / ((struct rpb *)HWRPB_ADDR)->rpb_cc_freq;
	lastpcc = curpcc;

	return (tnsec / 1000000000);
}
