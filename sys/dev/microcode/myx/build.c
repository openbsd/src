/*	$OpenBSD: build.c,v 1.1 2007/05/31 18:27:59 reyk Exp $	*/

/*
 * Copyright (c) 2007 Reyk Floeter <reyk@openbsd.org>
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

#include <dev/pci/if_myxreg.h>

#include <fcntl.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "myxfw.h"

void
myx_build_firmware(u_int32_t *fw, size_t len, const char *file)
{
	int		fd, rlen;
	size_t		i, total = 0;
	u_int32_t	data;

	printf("creating %s", file);
	fd = open(file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1)
		err(1, file);

	for (i = 0; i < len; i++) {
		data = letoh32(fw[i]);
		rlen = write(fd, &data, sizeof(u_int32_t));
		if (rlen == -1) {
			printf("\n");
			err(1, "%s", file);
		}
		if (rlen != sizeof(u_int32_t)) {
			printf("\n");
			errx(1, "%s: short write", file);
		}
		total += rlen;
	}

	printf(" total %d\n", total);
	close(fd);
}

int
main(int argc, char *argv[])
{
	myx_build_firmware(myxfw_eth_z8e,
	    MYXFW_ETH_Z8E_SIZE, MYXFW_ALIGNED);
	myx_build_firmware(myxfw_ethp_z8e,
	    MYXFW_ETHP_Z8E_SIZE, MYXFW_UNALIGNED);
	return (0);
}
