/*	$NetBSD: conf.c,v 1.1.1.1 1995/10/13 21:27:30 gwr Exp $	*/

#include <stand.h>
#include <dev_tape.h>

struct devsw devsw[] = {
	{ "tape", tape_strategy, tape_open, tape_close, tape_ioctl },
};
int	ndevs = 1;

int debug;
