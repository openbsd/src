/*	$OpenBSD: __syscall.c,v 1.4 2003/11/04 07:28:05 mickey Exp $	*/

/*
 * Written by Michael Shalayeff <mickey@openbsd.org> 2003 Public Domain.
 */

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>

int
main(void)
{
	extern off_t __syscall();
	off_t off;
	int fd;

	if ((fd = open("/etc/passwd", O_RDONLY)) < 0)
		err(1, "/etc/passwd");

	off = __syscall((u_int64_t)SYS_lseek, fd, 0, (off_t)1, SEEK_SET);
	if (off < 0)
		err(1, "lseek");

	off = __syscall((u_int64_t)SYS_lseek, fd, 0, (off_t)0, SEEK_CUR);
	if (off != 1)
		errx(1, "lseek: wrong position %llu", off);

	return 0;
}
