/*	$OpenBSD: dev_tape.h,v 1.4 2002/03/14 01:26:47 millert Exp $	*/


int	tape_open(struct open_file *, ...);
int	tape_close(struct open_file *);
int	tape_strategy(void *, int, daddr_t, size_t, void *, size_t *);
int	tape_ioctl();

