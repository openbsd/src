/*	$OpenBSD: suspend_np1.c,v 1.1 2012/02/21 13:02:28 kurt Exp $	*/
/*
 * Copyright (c) 2012 Kurt Miller <kurt@intricatesoftware.com>
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
 * Test pthread_suspend_np().
 */

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "test.h"

#define BUSY_THREADS	5

volatile int done = 0;
volatile int counter[BUSY_THREADS] = { 0, 0, 0, 0, 0 };

static void *
deadlock_detector(void *arg)
{
	struct timespec rqtp;
	rqtp.tv_sec = 15;
	rqtp.tv_nsec = 0;

	while (nanosleep(&rqtp, &rqtp) == -1 && errno == EINTR);
	PANIC("deadlock detected");
}

static void *
busy_thread(void *arg)
{
	int i = (int)arg;
	struct timespec rqtp;
	rqtp.tv_sec = 0;
	rqtp.tv_nsec = 1000000;

	while (!done) {
		counter[i]++;
		nanosleep(&rqtp, NULL);
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	pthread_t busy_threads[BUSY_THREADS];
	int saved_counter[BUSY_THREADS];
	pthread_t deadlock_thread;
	int i;
	void *value_ptr;
	struct timespec rqtp;

	rqtp.tv_sec = 0;
	rqtp.tv_nsec = 100 * 1000000;

	CHECKr(pthread_create(&deadlock_thread, NULL,
	    deadlock_detector, NULL));

	for (i = 0; i < BUSY_THREADS; i++)
		CHECKr(pthread_create(&busy_threads[i], NULL,
		    busy_thread, (void *)i));

	/* sleep to ensure threads have time to start and run */
	nanosleep(&rqtp, NULL);

	/* stop and save counters */
	pthread_suspend_all_np();
	for (i = 0; i < BUSY_THREADS; i++)
		saved_counter[i] = counter[i];
	
	/* sleep and check counters have not moved */
	nanosleep(&rqtp, NULL);
	for (i = 0; i < BUSY_THREADS; i++)
		ASSERT(saved_counter[i] == counter[i]);

	/* resume all and check counters are moving again */
	pthread_resume_all_np();
	nanosleep(&rqtp, NULL);
	for (i = 0; i < BUSY_THREADS; i++)
		ASSERT(saved_counter[i] != counter[i]);

	done = 1;

	for (i = 0; i < BUSY_THREADS; i++) {
		CHECKr(pthread_join(busy_threads[i], &value_ptr));
		ASSERT(value_ptr == NULL);
	}

	SUCCEED;
}
