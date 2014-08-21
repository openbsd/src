/*	$OpenBSD: conf.c,v 1.3 2014/08/21 14:24:08 mpi Exp $ */

#include <sys/types.h>
#include <netinet/in.h>

#include <stand.h>
#include <ufs.h>
#include <nfs.h>

struct fs_ops file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat },
	{ nfs_open, nfs_close, nfs_read, nfs_write, nfs_seek, nfs_stat }
};
int nfsys = sizeof(file_system) / sizeof(file_system[0]);

extern int sdstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
extern int sdopen(struct open_file *, ...);
extern int sdclose(struct open_file *);

extern int net_strategy(void *, int, daddr32_t, size_t, void *, size_t *);
extern int net_open(struct open_file *, ...);
extern int net_close(struct open_file *);

struct devsw devsw[] = {
	{ "sd", sdstrategy, sdopen, sdclose,  noioctl },
	{ "inen", net_strategy, net_open, net_close, noioctl }
};
int ndevs = sizeof(devsw) / sizeof(devsw[0]);

extern struct netif_driver le_driver;

struct netif_driver *netif_drivers[] = {
	&le_driver
};
int n_netif_drivers = sizeof(netif_drivers) / sizeof(netif_drivers[0]);

/* XXX */
int netif_debug;
int debug;
int errno;
