/*	$OpenBSD: dev_disk.h,v 1.2 1996/04/28 10:49:01 deraadt Exp $ */

int	disk_open __P((struct open_file *, ...));
int	disk_close __P((struct open_file *));
int	disk_strategy __P((void *, int, daddr_t, u_int, char *, u_int *));
int	disk_ioctl();

