/*	$OpenBSD: build.c,v 1.2 2005/04/14 19:01:08 damien Exp $	*/

/*
 * Copyright (c) 2004 Theo de Raadt <deraadt@openbsd.org>
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
#include <fcntl.h>
#include <sys/param.h>

#include <dev/usb/ezload.h>
#include "uyap_firmware.h"
#define FILENAME "uyap"

int
main(int argc, char *argv[])
{
	const struct ezdata *ptr;
	u_int16_t address;
	int fd;

	printf("creating %s length %d\n", FILENAME, sizeof uyap_firmware);
	fd = open(FILENAME, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1)
		err(1, "%s", FILENAME);

	for (ptr = uyap_firmware; ; ptr++) {
		write(fd, &ptr->length, 1);
		address = htole16(ptr->address);
		write(fd, &address, 2);
		write(fd, ptr->data, ptr->length);

		if (ptr->length == 0)
			break;
	}

	return 0;
}
