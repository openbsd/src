/*	$OpenBSD: blocked_fifo.c,v 1.1 2012/03/19 17:41:57 oga Exp $	*/
/*
 * Copyright (c) 2012 Owain G. Ainsworth <oga@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Test fifo opening blocking the file descriptor table and thus all other
 * opens in the process.
 * Symptoms are that the main thread will sleep on fifor (in the fifo open) and
 * the deadlocker on fdlock (in open of any other file).
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include "test.h"

#define FIFO	"/tmp/pthread.fifo"
#define FILE	"/etc/services"	/* just any file to deadlock on */

static void *
deadlock_detector(void *arg)
{
	sleep(10);
	unlink(FIFO);
	PANIC("deadlock detected");
}

static void *
fifo_deadlocker(void *arg)
{
	int	fd;

	/* let other fifo open start */
	sleep(3);

	/* open a random temporary file, if we don't deadlock this'll succeed */
	CHECKe(fd = open(FILE, O_RDONLY));
	CHECKe(close(fd));

	/* open fifo to unblock other thread */
	CHECKe(fd = open(FIFO, O_WRONLY));

	return ((caddr_t)NULL + errno);
}

int
main(int argc, char *argv[])
{
	pthread_t	 deadlock_thread, deadlock_finder;
	int		 fd;

	CHECKe(mkfifo(FIFO, S_IRUSR | S_IWUSR));

	CHECKr(pthread_create(&deadlock_thread, NULL,
	    fifo_deadlocker, NULL));
	CHECKr(pthread_create(&deadlock_finder, NULL,
	    deadlock_detector, NULL));

	/* Open fifo (this will sleep until we have readers */
	CHECKe(fd = open(FIFO, O_RDONLY));

	CHECKr(pthread_join(deadlock_thread, NULL));

	unlink(FIFO);

	SUCCEED;
}
