/*	$NetBSD: dev_net.h,v 1.6 2009/01/17 14:00:36 tsutsui Exp $	*/

int	net_open(struct open_file *, ...);
int	net_close(struct open_file *);
int	net_ioctl(struct open_file *, u_long, void *);
int	net_strategy(void *, int , daddr32_t , size_t, void *, size_t *);

#ifdef SUPPORT_BOOTP
extern int try_bootp;
#endif
