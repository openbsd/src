/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: rwlock_test.c,v 1.20.206.2 2004/08/28 06:25:31 marka Exp $ */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <isc/print.h>
#include <isc/thread.h>
#include <isc/rwlock.h>
#include <isc/string.h>
#include <isc/util.h>

#ifdef ISC_PLATFORM_USETHREADS

isc_rwlock_t lock;

static void *
run1(void *arg) {
	char *message = arg;

	RUNTIME_CHECK(isc_rwlock_lock(&lock, isc_rwlocktype_read) ==
		      ISC_R_SUCCESS);
	printf("%s got READ lock\n", message);
	sleep(1);
	printf("%s giving up READ lock\n", message);
	RUNTIME_CHECK(isc_rwlock_unlock(&lock, isc_rwlocktype_read) ==
	       ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_rwlock_lock(&lock, isc_rwlocktype_read) ==
		      ISC_R_SUCCESS);
	printf("%s got READ lock\n", message);
	sleep(1);
	printf("%s giving up READ lock\n", message);
	RUNTIME_CHECK(isc_rwlock_unlock(&lock, isc_rwlocktype_read) ==
	       ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_rwlock_lock(&lock, isc_rwlocktype_write) ==
		      ISC_R_SUCCESS);
	printf("%s got WRITE lock\n", message);
	sleep(1);
	printf("%s giving up WRITE lock\n", message);
	RUNTIME_CHECK(isc_rwlock_unlock(&lock, isc_rwlocktype_write) ==
	       ISC_R_SUCCESS);
	return (NULL);
}

static void *
run2(void *arg) {
	char *message = arg;

	RUNTIME_CHECK(isc_rwlock_lock(&lock, isc_rwlocktype_write) ==
		      ISC_R_SUCCESS);
	printf("%s got WRITE lock\n", message);
	sleep(1);
	printf("%s giving up WRITE lock\n", message);
	RUNTIME_CHECK(isc_rwlock_unlock(&lock, isc_rwlocktype_write) ==
	       ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_rwlock_lock(&lock, isc_rwlocktype_write) ==
		      ISC_R_SUCCESS);
	printf("%s got WRITE lock\n", message);
	sleep(1);
	printf("%s giving up WRITE lock\n", message);
	RUNTIME_CHECK(isc_rwlock_unlock(&lock, isc_rwlocktype_write) ==
	       ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_rwlock_lock(&lock, isc_rwlocktype_read) ==
		      ISC_R_SUCCESS);
	printf("%s got READ lock\n", message);
	sleep(1);
	printf("%s giving up READ lock\n", message);
	RUNTIME_CHECK(isc_rwlock_unlock(&lock, isc_rwlocktype_read) ==
	       ISC_R_SUCCESS);
	return (NULL);
}

int
main(int argc, char *argv[]) {
	unsigned int nworkers;
	unsigned int i;
	isc_thread_t workers[100];
	char name[100];
	void *dupname;

	if (argc > 1)
		nworkers = atoi(argv[1]);
	else
		nworkers = 2;
	if (nworkers > 100)
		nworkers = 100;
	printf("%d workers\n", nworkers);

	RUNTIME_CHECK(isc_rwlock_init(&lock, 5, 10) == ISC_R_SUCCESS);

	for (i = 0; i < nworkers; i++) {
		snprintf(name, sizeof(name), "%02u", i);
		dupname = strdup(name);
		RUNTIME_CHECK(dupname != NULL);
		if (i != 0 && i % 3 == 0)
			RUNTIME_CHECK(isc_thread_create(run1, dupname,
							&workers[i]) ==
			       ISC_R_SUCCESS);
		else
			RUNTIME_CHECK(isc_thread_create(run2, dupname,
							&workers[i]) ==
			       ISC_R_SUCCESS);
	}

	for (i = 0; i < nworkers; i++)
		(void)isc_thread_join(workers[i], NULL);

	isc_rwlock_destroy(&lock);

	return (0);
}

#else

int
main(int argc, char *argv[]) {
	UNUSED(argc);
	UNUSED(argv);
	fprintf(stderr, "This test requires threads.\n");
	exit(1);
}

#endif
