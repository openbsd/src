/*	$OpenBSD: dup2_self.c,v 1.1 2002/02/08 18:47:46 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>

int
main()
{
	int orgfd, fd1, fd2;
	char temp[] = "/tmp/dup2XXXXXXXXX";

	if ((orgfd = mkstemp(temp)) < 0)
		err(1, "mkstemp");
	remove(temp);

	if (ftruncate(orgfd, 1024) != 0)
		err(1, "ftruncate");

	if ((fd1 = dup(orgfd)) < 0)
		err(1, "dup");

	/* Set close-on-exec */
	if (fcntl(fd1, F_SETFD, 1) != 0)
		err(1, "fcntl(F_SETFD)");

	if ((fd2 = dup2(fd1, fd1)) < 0)
		err(1, "dup2");

	/* Test 1: Do we get the right fd? */
	if (fd2 != fd1)
		errx(1, "dup2 didn't give us the right fd");

	/* Test 2: Was close-on-exec cleared? */
	if (fcntl(fd2, F_GETFD) == 0)
		errx(1, "dup2 cleared close-on-exec");

	return 0;
}
