/* $OpenBSD: misc.c,v 1.1 2001/08/19 13:05:57 deraadt Exp $ */

/*
 * Miscellaneous syscall wrappers. See misc.h for the descriptions.
 */

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/file.h>

#include "params.h"

int sleep_select(int sec, int usec)
{
	struct timeval timeout;

	timeout.tv_sec = sec;
	timeout.tv_usec = usec;

	return select(0, NULL, NULL, NULL, &timeout);
}

int lock_fd(int fd, int shared)
{
#if LOCK_FCNTL
	struct flock l;

	memset(&l, 0, sizeof(l));
	l.l_whence = SEEK_SET;
	l.l_type = shared ? F_RDLCK : F_WRLCK;
	while (fcntl(fd, F_SETLKW, &l)) {
		if (errno != EBUSY) return -1;
		sleep_select(1, 0);
	}
#endif

#if LOCK_FLOCK
	while (flock(fd, shared ? LOCK_SH : LOCK_EX)) {
		if (errno != EBUSY) return -1;
		sleep_select(1, 0);
	}
#endif

	return 0;
}

int unlock_fd(int fd)
{
#if LOCK_FCNTL
	struct flock l;

	memset(&l, 0, sizeof(l));
	l.l_whence = SEEK_SET;
	l.l_type = F_UNLCK;
	if (fcntl(fd, F_SETLK, &l)) return -1;
#endif

#if LOCK_FLOCK
	if (flock(fd, LOCK_UN)) return -1;
#endif

	return 0;
}

int write_loop(int fd, char *buffer, int count)
{
	int offset, block;

	offset = 0;
	while (count > 0) {
		block = write(fd, &buffer[offset], count);

/* If any write(2) fails, we consider that the entire write_loop() has
 * failed to do its job. We don't even ignore EINTR here. We also don't
 * retry when a write(2) returns zero, as we could start eating up the
 * CPU if we did. */
		if (block < 0) return block;
		if (!block) return offset;

		offset += block;
		count -= block;
	}

/* Should be equal to the requested size, unless our kernel got crazy. */
	return offset;
}
