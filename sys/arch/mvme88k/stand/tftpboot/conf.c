/*	$OpenBSD: conf.c,v 1.1 2004/01/26 19:48:34 miod Exp $	*/

#include <sys/types.h>
#include <machine/prom.h>

#include <stand.h>
#include "tftpfs.h"
#include "netdev.h"
#include "libsa.h"

struct fs_ops file_system[] = {
	{ tftpfs_open, tftpfs_close, tftpfs_read, tftpfs_write, tftpfs_seek, tftpfs_stat },
};

int nfsys = sizeof(file_system) / sizeof(file_system[0]);

struct devsw devsw[] = {
	{ "net", net_strategy, net_open, net_close, net_ioctl },
};

int     ndevs = (sizeof(devsw)/sizeof(devsw[0]));

