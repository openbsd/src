/*	$OpenBSD: dev_net.h,v 1.6 2012/12/31 21:35:32 miod Exp $ */

int	net_open(struct open_file *, ...);
int	net_close(struct open_file *);
int	net_ioctl(struct open_file *, u_long, void *);
int	net_strategy(void *, int, daddr32_t, size_t, void *, size_t *);

void	machdep_common_ether(u_char *);
