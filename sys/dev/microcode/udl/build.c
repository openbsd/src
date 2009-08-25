/*	$OpenBSD: build.c,v 1.3 2009/08/25 21:45:26 deraadt Exp $ */

/*
 * Copyright (c) 2009 Marcus Glocker <mglocker@openbsd.org>
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

#include <err.h>
#include <fcntl.h>
#include <unistd.h>

#include "udl_huffman.h"

#define FILENAME "udl_huffman"

int
main(void)
{
	int fd, i;
	uint8_t size;
	uint32_t value;

	fd = open(FILENAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
		err(1, "%s", FILENAME);

	for (i = 0; i < UDL_HUFFMAN_RECORDS; i++) {
		size = udl_huffman[i].size;
		value = htobe32(udl_huffman[i].value);
		if (write(fd, &size, sizeof(size)) == -1)
			err(1, "write");
		if (write(fd, &value, sizeof(value)) == -1)
			err(1, "write");
	}

	close(fd);

	return (0);
}
