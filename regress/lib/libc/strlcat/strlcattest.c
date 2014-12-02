/*	$OpenBSD: strlcattest.c,v 1.1 2014/12/02 17:48:34 millert Exp $ */

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
	char *buf, *cp, *ep;
	int failures = 0;
	size_t len, bufsize;

	/* Enable malloc security options. */
	setenv("MALLOC_OPTIONS", "S", 0);

	bufsize = getpagesize(); /* trigger guard pages easily */
	buf = malloc(bufsize);
	if (buf == NULL) {
		fprintf(stderr, "unable to allocate memory\n");
		return 1;
	}
	memset(buf, 'a', bufsize);
	ep = buf + bufsize;

	/* Test appending to an unterminated string. */
	len = strlcat(buf, "abcd", bufsize);
	if (len != 4 + bufsize) {
		fprintf(stderr, "strlcat: failed unterminated buffer test (1a)");
		failures++;
	}
	/* Make sure we only wrote where expected. */
	for (cp = buf; cp < ep; cp++) {
		if (*cp != 'a') {
			fprintf(stderr, "strlcat: failed unterminated buffer test (1b)");
			failures++;
			break;
		}
	}

	/* Test appending to a full string. */
	ep[-1] = '\0';
	len = strlcat(buf, "abcd", bufsize);
	if (len != 4 + bufsize - 1) {
		fprintf(stderr, "strlcat: failed full buffer test (2a)");
		failures++;
	}
	/* Make sure we only wrote where expected. */
	for (cp = buf; cp < ep - 1; cp++) {
		if (*cp != 'a') {
			fprintf(stderr, "strlcat: failed full buffer test (2b)");
			failures++;
			break;
		}
	}

	/* Test appending to an empty string. */
	ep[-1] = 'a';
	buf[0] = '\0';
	len = strlcat(buf, "abcd", bufsize);
	if (len != 4) {
		fprintf(stderr, "strlcat: failed empty buffer test (3a)");
		failures++;
	}
	/* Make sure we only wrote where expected. */
	if (memcmp(buf, "abcd", sizeof("abcd")) != 0) {
		fprintf(stderr, "strlcat: failed empty buffer test (3b)");
		failures++;
	}
	for (cp = buf + len + 1; cp < ep; cp++) {
		if (*cp != 'a') {
			fprintf(stderr, "strlcat: failed empty buffer test (3c)");
			failures++;
			break;
		}
	}

	/* Test appending to a NUL-terminated string. */
	memcpy(buf, "abcd", sizeof("abcd"));
	len = strlcat(buf, "efgh", bufsize);
	if (len != 8) {
		fprintf(stderr, "strlcat: failed empty buffer test (4a)");
		failures++;
	}
	/* Make sure we only wrote where expected. */
	if (memcmp(buf, "abcdefgh", sizeof("abcdefgh")) != 0) {
		fprintf(stderr, "strlcat: failed empty buffer test (4b)");
		failures++;
	}
	for (cp = buf + len + 1; cp < ep; cp++) {
		if (*cp != 'a') {
			fprintf(stderr, "strlcat: failed empty buffer test (4c)");
			failures++;
			break;
		}
	}

	return failures;
}
