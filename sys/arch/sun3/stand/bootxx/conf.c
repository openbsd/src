/*	$OpenBSD: conf.c,v 1.2 2001/07/04 08:33:44 niklas Exp $	*/
/*	$NetBSD: conf.c,v 1.2 1995/10/13 21:45:00 gwr Exp $	*/

#include <stand.h>
#include <dev_disk.h>

struct devsw devsw[] = {
	{ "disk", disk_strategy, disk_open, disk_close, disk_ioctl },
};
int	ndevs = 1;

int debug;
