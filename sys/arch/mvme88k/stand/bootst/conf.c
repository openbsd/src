/*	$OpenBSD: conf.c,v 1.1 1998/12/15 06:09:51 smurph Exp $ */

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

