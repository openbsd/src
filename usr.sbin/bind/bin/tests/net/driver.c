/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: driver.c,v 1.7 2001/01/09 21:42:05 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <isc/string.h>
#include <isc/util.h>

#include "driver.h"

#include "testsuite.h"

#define NTESTS (sizeof(tests) / sizeof(test_t))

const char *gettime(void);
const char *test_result_totext(test_result_t);

/*
 * Not thread safe.
 */
const char *
gettime(void) {
	static char now[512];
	time_t t;

	(void)time(&t);

	strftime(now, sizeof(now) - 1,
		 "%A %d %B %H:%M:%S %Y",
		 localtime(&t));

	return (now);
}

const char *
test_result_totext(test_result_t result) {
	const char *s;
	switch (result) {
	case PASSED:
		s = "PASS";
		break;
	case FAILED:
		s = "FAIL";
		break;
	case UNTESTED:
		s = "UNTESTED";
		break;
	case UNKNOWN:
	default:
		s = "UNKNOWN";
		break;
	}

	return (s);
}

int
main(int argc, char **argv) {
	test_t *test;
	test_result_t result;
	unsigned int n_failed;
	unsigned int testno;

	UNUSED(argc);
	UNUSED(argv);

	printf("S:%s:%s\n", SUITENAME, gettime());

	n_failed = 0;
	for (testno = 0 ; testno < NTESTS ; testno++) {
		test = &tests[testno];
		printf("T:%s:%u:A\n", test->tag, testno + 1);
		printf("A:%s\n", test->description);
		result = test->func();
		printf("R:%s\n", test_result_totext(result));
		if (result != PASSED)
			n_failed++;
	}

	printf("E:%s:%s\n", SUITENAME, gettime());

	if (n_failed > 0)
		exit(1);

	return (0);
}

