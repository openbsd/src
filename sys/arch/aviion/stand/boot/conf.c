/*	$OpenBSD: conf.c,v 1.1 2013/10/08 21:55:20 miod Exp $ */

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <stand.h>
#include <ufs.h>

struct fs_ops file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat }
};
int nfsys = sizeof(file_system) / sizeof(file_system[0]);

extern int sdstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
extern int sdopen(struct open_file *, ...);
extern int sdclose(struct open_file *);

struct devsw devsw[] = {
	{ "sd", sdstrategy, sdopen, sdclose,  noioctl },
};
int	ndevs = sizeof(devsw) / sizeof(devsw[0]);

/* XXX */
#if 0
int netif_debug;
int debug;
#endif
int errno;
