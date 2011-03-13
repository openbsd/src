/*	$OpenBSD: dev_tape.h,v 1.4 2011/03/13 00:13:53 deraadt Exp $	*/


int	tape_open(struct open_file *, ...);
int	tape_close(struct open_file *);
int	tape_strategy(void *, int, daddr32_t, size_t, void *, size_t *);
int	tape_ioctl();

