/*	$OpenBSD: pwrite.c,v 1.4 2011/11/05 15:43:04 guenther Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{	
	char temp[] = "/tmp/pwriteXXXXXXXXX";
	const char magic[10] = "0123456789";
	const char zeroes[10] = "0000000000";
	char buf[10];
	char c;
	int fd, ret;

	if ((fd = mkstemp(temp)) < 0)
		err(1, "mkstemp");
	remove(temp);

	if (write(fd, zeroes, sizeof(zeroes)) != sizeof(zeroes))
		err(1, "write");

	if (lseek(fd, 5, SEEK_SET) != 5)
		err(1, "lseek");

	if (pwrite(fd, &magic[1], 4, 4) != 4)
		err(1, "pwrite");

	if (read(fd, &c, 1) != 1)
		err(1, "read");

	if (c != '2')
		errx(1, "read %c != %c", c, '2');

	c = '5';
	if (write(fd, &c, 1) != 1)
		err(1, "write");

	if (pread(fd, buf, 10, 0) != 10)
		err(1, "pread");

	if (memcmp(buf, "0000125400", 10) != 0)
		errx(1, "data mismatch: %s != %s", buf, "0000125400");

	close(fd);

	/* also, verify that pwrite fails on ttys */
	fd = open("/dev/tty", O_RDWR);
	if (fd < 0)
		printf("skipping tty test\n");
	else if ((ret = pwrite(fd, &c, 1, 7)) != -1)
		errx(1, "pwrite succeeded on tty, returning %d", ret);
	else if (errno != ESPIPE)
		err(1, "pwrite");

	return 0;
}
