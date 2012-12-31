/*	$OpenBSD: conf.c,v 1.3 2012/12/31 21:35:32 miod Exp $	*/
/*	$NetBSD: conf.c,v 1.2 1995/10/17 22:58:17 gwr Exp $	*/

#include <stand.h>
#include <rawfs.h>
#include <dev_tape.h>

struct fs_ops file_system[] = {
	{
		rawfs_open, rawfs_close, rawfs_read,
		rawfs_write, rawfs_seek, rawfs_stat,
	},
};
int nfsys = sizeof(file_system) / sizeof(file_system[0]);

struct devsw devsw[] = {
	{ "tape", tape_strategy, tape_open, tape_close, tape_ioctl },
};
int ndevs = sizeof(devsw) / sizeof(devsw[0]);
