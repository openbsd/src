/*	$NetBSD: conf.c,v 1.1.1.1 1995/06/01 20:38:08 gwr Exp $	*/

#include <stand.h>
#include <ufs.h>
#include <dev_disk.h>

struct fs_ops file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat },
};
int nfsys = 1;

struct devsw devsw[] = {
	{ "disk", disk_strategy, disk_open, disk_close, disk_ioctl },
};
int	ndevs = 1;

