/*	$OpenBSD: conf.c,v 1.2 2001/07/04 08:06:55 niklas Exp $	*/
/*	$NetBSD: conf.c,v 1.2 1995/10/13 21:45:00 gwr Exp $	*/

#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"

struct devsw devsw[] = {
	{ "bugsc", bugscstrategy, bugscopen, bugscclose, bugscioctl },
};
int     ndevs = (sizeof(devsw)/sizeof(devsw[0]));

int debug;
