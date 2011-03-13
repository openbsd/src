/*	$OpenBSD: netdev.h,v 1.2 2011/03/13 00:13:53 deraadt Exp $	*/

int net_open(struct open_file *, ...);
int net_close(struct open_file *);
int net_ioctl(struct open_file *, u_long, void *);
int net_strategy(void *, int, daddr32_t, size_t, void *, size_t *);
