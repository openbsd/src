/*	$OpenBSD: clock.c,v 1.2 2001/07/04 08:33:47 niklas Exp $	*/


#include <sys/types.h>
#include <machine/mon.h>

#include "clock.h"

int hz = 1000;

time_t getsecs()
{
	register int ticks = getticks();
	return ((time_t)(ticks / hz));
}

int getticks()
{
	register MachMonRomVector * romvec;
	register int ticks;

	romvec = romVectorPtr;
	ticks = *(romvec->nmiClock);

	return (ticks);
}
