/*	$OpenBSD: rlim-file.c,v 1.1 2002/02/05 16:19:49 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> (2002) Public Domain.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <err.h>
#include <unistd.h>
#include <fcntl.h>

int
main()
{
	int lim, fd, fds[2];
	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
		err(1, "getrlimit");

	lim = rl.rlim_cur;

	fd = -1;
	while (fd < lim - 2)
		if ((fd = open("/dev/null", O_RDONLY)) < 0)
			err(1, "open");

	if (pipe(fds) == 0)
		errx(1, "pipe was allowed");

	return 0;
}

