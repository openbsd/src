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

/* $ISC: fsaccess_test.c,v 1.8 2001/01/09 21:41:03 bwelling Exp $ */

#include <config.h>

#include <stdio.h>

#include <sys/types.h>		/* Non-portable. */
#include <sys/stat.h>		/* Non-portable. */

#include <isc/fsaccess.h>
#include <isc/result.h>

#define PATH "/tmp/fsaccess"

int
main(void) {
	isc_fsaccess_t access;
	isc_result_t result;

	remove(PATH);
	fopen(PATH, "w");
	chmod(PATH, 0);

	access = 0;

	isc_fsaccess_add(ISC_FSACCESS_OWNER | ISC_FSACCESS_GROUP,
			 ISC_FSACCESS_READ | ISC_FSACCESS_WRITE,
			 &access);

	printf("fsaccess=%d\n", access);

	isc_fsaccess_add(ISC_FSACCESS_OTHER, ISC_FSACCESS_READ, &access);

	printf("fsaccess=%d\n", access);

	result = isc_fsaccess_set(PATH, access);
	if (result != ISC_R_SUCCESS)
		fprintf(stderr, "result = %s\n", isc_result_totext(result));

	return (0);
}
