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

/* $ISC: driver.h,v 1.5 2001/01/09 21:42:06 bwelling Exp $ */

/*
 * PASSED and FAILED mean the particular test passed or failed.
 *
 * UNKNOWN means that for one reason or another, the test process itself
 * failed.  For instance, missing files, error when parsing files or
 * IP addresses, etc.  That is, the test itself is broken, not what is
 * being tested.
 *
 * UNTESTED means the test was unable to be run because a prerequisite test
 * failed, the test is disabled, or the test needs a system component
 * (for instance, Perl) and cannot run.
 */
typedef enum {
	PASSED = 0,
	FAILED = 1,
	UNKNOWN = 2,
	UNTESTED = 3
} test_result_t;

typedef test_result_t (*test_func_t)(void);

typedef struct {
	const char *tag;
	const char *description;
	test_func_t func;
} test_t;

#define TESTDECL(name)	test_result_t name(void)

