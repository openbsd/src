/*	$OpenBSD: dev_net.h,v 1.4 2003/08/20 00:26:00 deraadt Exp $ */

int	net_open(struct open_file *, char *);
int	net_close(struct open_file *);
int	net_ioctl(struct open_file *, u_long, void *);
int	net_strategy(void *, int, daddr_t, size_t, void *, size_t *);

void	machdep_common_ether(u_char *);
