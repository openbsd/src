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

/* $ISC: keyboard_test.c,v 1.8 2001/01/09 21:41:11 bwelling Exp $ */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <isc/keyboard.h>
#include <isc/util.h>

static void
CHECK(const char *msg, isc_result_t result) {
	if (result != ISC_R_SUCCESS) {
		printf("FAILURE:  %s:  %s\n", msg, isc_result_totext(result));
		exit(1);
	}
}

int
main(int argc, char **argv) {
	isc_keyboard_t kbd;
	unsigned char c;
	isc_result_t res;
	unsigned int count;

	UNUSED(argc);
	UNUSED(argv);

	printf("Type Q to exit.\n");

	res = isc_keyboard_open(&kbd);
	CHECK("isc_keyboard_open()", res);

	c = 'x';
	count = 0;
	while (res == ISC_R_SUCCESS && c != 'Q') {
		res = isc_keyboard_getchar(&kbd, &c);
		printf(".");
		fflush(stdout);
		count++;
		if (count % 64 == 0)
			printf("\r\n");
	}
	printf("\r\n");
	if (res != ISC_R_SUCCESS) {
		printf("FAILURE:  keyboard getchar failed:  %s\r\n",
		       isc_result_totext(res));
		goto errout;
	}

 errout:
	res = isc_keyboard_close(&kbd, 3);
	CHECK("isc_keyboard_close()", res);

	return (0);
}

