/*	$OpenBSD: strlcpytest.c,v 1.1 2014/12/02 20:23:05 millert Exp $ */

/*
 * Copyright (c) 2014 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	char *buf, *buf2, *cp, *ep;
	int failures = 0;
	size_t len, bufsize;

	/* Enable malloc security options. */
	setenv("MALLOC_OPTIONS", "S", 0);

	bufsize = getpagesize(); /* trigger guard pages easily */
	buf = malloc(bufsize);
	buf2 = malloc(bufsize);
	if (buf == NULL || buf2 == NULL) {
		fprintf(stderr, "unable to allocate memory\n");
		return 1;
	}
	memset(buf, 'a', bufsize);
	ep = buf + bufsize;

	/* Test copying to a zero-length NULL buffer. */
	len = strlcpy(NULL, "abcd", 0);
	if (len != 4) {
		fprintf(stderr, "strlcpy: failed NULL buffer test (1a)");
		failures++;
	}

	/* Test copying small string to a large buffer. */
	len = strlcpy(buf, "abcd", bufsize);
	if (len != 4) {
		fprintf(stderr, "strlcpy: failed large buffer test (2a)");
		failures++;
	}
	/* Make sure we only wrote where expected. */
	if (memcmp(buf, "abcd", sizeof("abcd")) != 0) {
		fprintf(stderr, "strlcpy: failed large buffer test (2b)");
		failures++;
	}
	for (cp = buf + len + 1; cp < ep; cp++) {
		if (*cp != 'a') {
			fprintf(stderr, "strlcpy: failed large buffer test (2c)");
			failures++;
			break;
		}
	}

	/* Test copying large string to a small buffer. */
	memset(buf, 'a', bufsize);
	memset(buf2, 'x', bufsize - 1);
	buf2[bufsize - 1] = '\0';
	len = strlcpy(buf, buf2, bufsize / 2);
	if (len != bufsize - 1) {
		fprintf(stderr, "strlcpy: failed small buffer test (3a)");
		failures++;
	}
	/* Make sure we only wrote where expected. */
	len = (bufsize / 2) - 1;
	if (memcmp(buf, buf2, len) != 0 || buf[len] != '\0') {
		fprintf(stderr, "strlcpy: failed small buffer test (3b)");
		failures++;
	}
	for (cp = buf + len + 1; cp < ep; cp++) {
		if (*cp != 'a') {
			fprintf(stderr, "strlcpy: failed small buffer test (3c)");
			failures++;
			break;
		}
	}

	/* Test copying to a 1-byte buffer. */
	memset(buf, 'a', bufsize);
	len = strlcpy(buf, "abcd", 1);
	if (len != 4) {
		fprintf(stderr, "strlcpy: failed 1-byte buffer test (4a)");
		failures++;
	}
	/* Make sure we only wrote where expected. */
	if (buf[0] != '\0') {
		fprintf(stderr, "strlcpy: failed 1-byte buffer test (4b)");
		failures++;
	}
	for (cp = buf + 1; cp < ep; cp++) {
		if (*cp != 'a') {
			fprintf(stderr, "strlcpy: failed 1-byte buffer test (4c)");
			failures++;
			break;
		}
	}

	return failures;
}
