/*	$OpenBSD: dev_net.h,v 1.1 2006/05/16 22:48:18 miod Exp $ */

int	net_open(struct open_file *, ...);
int	net_close(struct open_file *);
int	net_ioctl();
int	net_strategy();

