/*	$OpenBSD: conf.c,v 1.1 2001/06/26 21:58:07 smurph Exp $	*/

#include <sys/types.h>
#include <machine/prom.h>

#include <stand.h>
#include <ufs.h>
#include "tftpfs.h"
#include "rawfs.h"
#include "libsa.h"

struct fs_ops ufs_file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat },
};

struct fs_ops tftp_file_system[] = {
	{ tftpfs_open, tftpfs_close, tftpfs_read, tftpfs_write, tftpfs_seek, tftpfs_stat },
};

struct fs_ops raw_file_system[] = {
	{ rawfs_open, rawfs_close, rawfs_read, rawfs_write, rawfs_seek, rawfs_stat },
};

int nfsys = 1; /* devopen will choose the correct one. */

struct devsw devsw[] = {
        { "dsk", dsk_strategy, dsk_open, dsk_close, dsk_ioctl },
	{ "net", net_strategy, net_open, net_close, net_ioctl },
	{ "tape", tape_strategy, tape_open, tape_close, tape_ioctl },
};

int     ndevs = (sizeof(devsw)/sizeof(devsw[0]));

