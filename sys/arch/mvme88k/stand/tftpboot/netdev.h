/*	$OpenBSD: netdev.h,v 1.1 2004/01/26 19:48:34 miod Exp $	*/

int net_open(struct open_file *, ...);
int net_close(struct open_file *);
int net_ioctl(struct open_file *, u_long, void *);
int net_strategy(void *, int, daddr_t, size_t, void *, size_t *);
