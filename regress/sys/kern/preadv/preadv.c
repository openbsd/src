/*	$OpenBSD: preadv.c,v 1.2 2003/07/31 21:48:09 deraadt Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{	
	char temp[] = "/tmp/dup2XXXXXXXXX";
	const char magic[10] = "0123456789";
	char c, c1, c2;
	int fd;
	struct iovec iv[2];

	if ((fd = mkstemp(temp)) < 0)
		err(1, "mkstemp");
	remove(temp);

	if (write(fd, magic, sizeof(magic)) != sizeof(magic))
		err(1, "write");

	if (lseek(fd, 0, SEEK_SET) != 0)
		err(1, "lseek");

	if (read(fd, &c, 1) != 1)
		err(1, "read1");

	if (c != magic[0])
		errx(1, "read1 %c != %c", c, magic[0]);

	iv[0].iov_base = &c1;
	iv[0].iov_len = 1;
	iv[1].iov_base = &c2;
	iv[1].iov_len = 1;

	if (preadv(fd, iv, 2, 7) != 2)
		err(1, "pread");

	if (c1 != magic[7])
		errx(1, "pread %c != %c", c1, magic[7]);

	if (c2 != magic[8])
		errx(1, "pread %c != %c", c2, magic[7]);

	if (read(fd, &c, 1) != 1)
		err(1, "read2");

	if (c != magic[1])
		errx(1, "read2 %c != %c", c, magic[1]);

	return 0;
}
