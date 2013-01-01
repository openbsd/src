/*	$OpenBSD: open_memstreamtest.c,v 1.1 2013/01/01 17:43:07 mpi Exp $ */
/*
 * Copyright (c) 2011 Martin Pieuchot <mpi@openbsd.org>
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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define OFFSET 16384

int
main(void)
{
	FILE	*fp;
	char	*buf = (char *)0xff;
	size_t	 size = 0;
	off_t	 off;
	int	 i = 0, failures = 0;

	if ((fp = open_memstream(&buf, &size)) == NULL) {
		warn("open_memstream failed");
		return (1);
	}

	off = ftello(fp);
	if (off != 0) {
		warnx("ftello failed. (1)");
		failures++;
	}

	if (fflush(fp) != 0) {
		warnx("fflush failed. (2)");
		failures++;
	}

	if (size != 0) {
		warnx("string should be empty. (3)");
		failures++;
	}

	if (buf == (char *)0xff) {
		warnx("buf not updated. (4)");
		failures++;
	}

	if (fseek(fp, OFFSET, SEEK_SET) != 0) {
		warnx("failed to fseek. (5)");
		failures++;
	}

	if (fprintf(fp, "hello") == EOF) {
		warnx("fprintf failed. (6)");
		failures++;
	}

	if (fclose(fp) == EOF) {
		warnx("fclose failed. (7)");
		failures++;
	}

	if (size != OFFSET + 5) {
		warnx("failed, size %zu should be %u\n", size, OFFSET + 5);
		failures++;
	}

	/* Needed for sparse files */
	while (i != OFFSET)
		if (buf[i++] != '\0') {
			warnx("failed, buffer non zero'ed (offset %d). (8)", i);
			failures++;
			break;
		}

	if (memcmp(buf + OFFSET, "hello", 5) != 0) {
		warnx("written string incorrect. (9)");
		failures++;
	}

	free(buf);

	return (failures);
}
