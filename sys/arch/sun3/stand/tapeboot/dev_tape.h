/*	$NetBSD: dev_tape.h,v 1.1.1.1 1995/10/13 21:27:30 gwr Exp $	*/

int	tape_open __P((struct open_file *, ...));
int	tape_close __P((struct open_file *));
int	tape_strategy __P((void *, int, daddr_t, size_t, void *, size_t *));
int	tape_ioctl();

