/*	$OpenBSD: dev_tape.h,v 1.3 2001/07/04 08:33:57 niklas Exp $	*/


int	tape_open __P((struct open_file *, ...));
int	tape_close __P((struct open_file *));
int	tape_strategy __P((void *, int, daddr_t, size_t, void *, size_t *));
int	tape_ioctl();

