/*	$OpenBSD: build.c,v 1.1 2004/12/22 12:02:47 grange Exp $	*/

/*
 * Copyright (c) 2004 Theo de Raadt <deraadt@openbsd.org>
 * Copyright (c) 2004 Dmitry Bogdan
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
#include <stdlib.h>

#include "rcvbundl.h"

const u_int32_t fxp_ucode_d101a[] = D101_A_RCVBUNDLE_UCODE;
const u_int32_t fxp_ucode_d101b0[] = D101_B0_RCVBUNDLE_UCODE;
const u_int32_t fxp_ucode_d101ma[] = D101M_B_RCVBUNDLE_UCODE;
const u_int32_t fxp_ucode_d101s[] = D101S_RCVBUNDLE_UCODE;
const u_int32_t fxp_ucode_d102[] = D102_B_RCVBUNDLE_UCODE;
const u_int32_t fxp_ucode_d102c[] = D102_C_RCVBUNDLE_UCODE;

#define UCODE(x)	x, sizeof(x)

static void
output(const char *name, const u_int32_t *ucode, const int ucode_len)
{
	int fd, i;
	u_int32_t dword;

	printf("creating %s length %d (microcode: %d DWORDS)\n",
	    name, ucode_len, ucode_len / sizeof(u_int32_t));
	fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1)
		err(1, "%s", name);
	for (i = 0; i < ucode_len / sizeof(u_int32_t); i++) {
		dword = htole32(ucode[i]);
		write(fd, &dword, sizeof(dword));
	}
	close(fd);
}

int
main(int argc, char *argv[])
{
	output("fxp-d101a", UCODE(fxp_ucode_d101a));
	output("fxp-d101b0", UCODE(fxp_ucode_d101b0));
	output("fxp-d101ma", UCODE(fxp_ucode_d101ma));
	output("fxp-d101s", UCODE(fxp_ucode_d101s));
	output("fxp-d102", UCODE(fxp_ucode_d102));
	output("fxp-d102c", UCODE(fxp_ucode_d102c));

	return (0);
}
