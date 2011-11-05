/*	$OpenBSD: preadv.c,v 1.3 2011/11/05 15:43:04 guenther Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{	
	char temp[] = "/tmp/dup2XXXXXXXXX";
	const char magic[10] = "0123456789";
	char c, c1, c2;
	int fd, ret;
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
		err(1, "preadv");

	if (c1 != magic[7])
		errx(1, "preadv %c != %c", c1, magic[7]);

	if (c2 != magic[8])
		errx(1, "preadv %c != %c", c2, magic[7]);

	if (read(fd, &c, 1) != 1)
		err(1, "read2");

	if (c != magic[1])
		errx(1, "read2 %c != %c", c, magic[1]);

	close(fd);

	/* also, verify that preadv fails on ttys */
	fd = open("/dev/tty", O_RDWR);
	if (fd < 0)
		printf("skipping tty test\n");
	else if ((ret = preadv(fd, iv, 2, 7)) != -1)
		errx(1, "preadv succeeded on tty, returning %d", ret);
	else if (errno != ESPIPE)
		err(1, "preadv");

	return 0;
}
