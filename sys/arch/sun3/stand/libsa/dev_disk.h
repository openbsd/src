/*	$OpenBSD: dev_disk.h,v 1.2 2001/07/04 08:33:48 niklas Exp $	*/


int	disk_open __P((struct open_file *, ...));
int	disk_close __P((struct open_file *));
int	disk_strategy __P((void *, int, daddr_t, size_t, void *, size_t *));
int	disk_ioctl();

