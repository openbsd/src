/* int sigsuspend(int *mask) */

#include "SYS.h"

	.text
PREFIX(sigsuspend)
	lwz 3, 0(3)		/* load the mask */
	sc
	SUFFIX
