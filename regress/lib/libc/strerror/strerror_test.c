/* $OpenBSD: strerror_test.c,v 1.4 2005/05/13 07:06:20 otto Exp $ */
/*
 * Copyright (c) 2004 Marc Espie <espie@cvs.openbsd.org>
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
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

void
check_strerror_r(int val)
{
	char buffer[NL_TEXTMAX + 1];
	int i, r;

	memset(buffer, 'X', sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = '\0';
	(void)strerror_r(val, NULL, 0);	/* XXX */
	for (i = 0; i < 25; i++) {
		r = strerror_r(val, buffer, i);
		printf("%d %d %zu: %s\n", i, r, strlen(buffer), buffer);
	}
}

void
check_strsignal(int val)
{
	char buffer[NL_TEXTMAX + 1];
	int i, r;

	memset(buffer, 'X', sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = '\0';
	(void)__strsignal(val, buffer);
	printf("%s\n", buffer);
}

int 
main()
{
	printf("%s\n", strerror(21345));
	printf("%s\n", strerror(-21345));
	printf("%s\n", strerror(0));
	printf("%s\n", strerror(INT_MAX));
	printf("%s\n", strerror(INT_MIN));
	printf("%s\n", strerror(EPERM));
	check_strerror_r(EPERM);
	check_strerror_r(21345);
	check_strsignal(-1);
	check_strsignal(0);
	check_strsignal(10);
	check_strsignal(NSIG-1);
	check_strsignal(NSIG);
	check_strsignal(100);
	check_strsignal(INT_MAX);
	check_strsignal(INT_MIN);
	check_strsignal(UINT_MAX);
	return 0;
}
