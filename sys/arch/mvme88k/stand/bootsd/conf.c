/*	$OpenBSD: conf.c,v 1.2 1998/08/22 08:57:29 smurph Exp $	*/

#include <sys/types.h>
#include <machine/prom.h>

#include <stand.h>
#include <ufs.h>
#include "libsa.h"

struct fs_ops file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat },
};
int nfsys = sizeof(file_system)/sizeof(struct fs_ops);

struct devsw devsw[] = {
        { "bugsc", bugscstrategy, bugscopen, bugscclose, bugscioctl },
};
int     ndevs = (sizeof(devsw)/sizeof(devsw[0]));


