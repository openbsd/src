/*	$OpenBSD: conf.c,v 1.1 1998/08/22 08:27:08 smurph Exp $	*/

#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"

struct devsw devsw[] = {
	{ "bugsc", bugscstrategy, bugscopen, bugscclose, bugscioctl },
};
int     ndevs = (sizeof(devsw)/sizeof(devsw[0]));

int debug;
