/*	$OpenBSD: dev_disk.h,v 1.3 2002/03/14 01:26:47 millert Exp $	*/


int	disk_open(struct open_file *, ...);
int	disk_close(struct open_file *);
int	disk_strategy(void *, int, daddr_t, size_t, void *, size_t *);
int	disk_ioctl();

