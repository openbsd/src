/*	$OpenBSD: pwritev.c,v 1.3 2003/09/02 23:52:17 david Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{	
	char temp[] = "/tmp/pwritevXXXXXXXXX";
	char magic[10] = "0123456789";
	const char zeroes[10] = "0000000000";
	char buf[10];
	struct iovec iov[2];
	char c;
	int fd;

	if ((fd = mkstemp(temp)) < 0)
		err(1, "mkstemp");
	remove(temp);

	if (write(fd, zeroes, sizeof(zeroes)) != sizeof(zeroes))
		err(1, "write");

	if (lseek(fd, 5, SEEK_SET) != 5)
		err(1, "lseek");

	iov[0].iov_base = &magic[8];
	iov[0].iov_len = 2;
	iov[1].iov_base = &magic[7];
	iov[1].iov_len = 2;

	if (pwritev(fd, iov, 2, 4) != 4)
		err(1, "pwrite");

	if (read(fd, &c, 1) != 1)
		err(1, "read");

	if (c != '9')
		errx(1, "read %c != %c", c, '9');

	c = '5';
	if (write(fd, &c, 1) != 1)
		err(1, "write");

	if (pread(fd, buf, 10, 0) != 10)
		err(1, "pread");

	if (memcmp(buf, "0000895800", 10) != 0)
		errx(1, "data mismatch: %s != %s", buf, "0000895800");

	return 0;
}
