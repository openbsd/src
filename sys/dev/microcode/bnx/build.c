/*	$OpenBSD: build.c,v 1.1 2006/09/20 22:16:04 deraadt Exp $	*/

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
#include <dev/pci/if_bnxreg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "bnxfw.h"

#define FILENAME "bnx"

int
main(int argc, char *argv[])
{
	struct	bnx_firmware_header bfproto, *bf;
	int len, fd;
	ssize_t rlen;

	len = sizeof(*bf);
	bf = (struct bnx_firmware_header *)malloc(len);
	bzero(bf, len);

	/* initialize the file header */
	bf->bnx_COM_b06FwReleaseMajor = bnx_COM_b06FwReleaseMajor;
	bf->bnx_COM_b06FwReleaseMinor = bnx_COM_b06FwReleaseMinor;
	bf->bnx_COM_b06FwReleaseFix = bnx_COM_b06FwReleaseFix;
	bf->bnx_COM_b06FwStartAddr = bnx_COM_b06FwStartAddr;
	bf->bnx_COM_b06FwTextAddr = bnx_COM_b06FwTextAddr;
	bf->bnx_COM_b06FwTextLen = bnx_COM_b06FwTextLen;
	bf->bnx_COM_b06FwDataAddr = bnx_COM_b06FwDataAddr;
	bf->bnx_COM_b06FwDataLen = bnx_COM_b06FwDataLen;
	bf->bnx_COM_b06FwRodataAddr = bnx_COM_b06FwRodataAddr;
	bf->bnx_COM_b06FwRodataLen = bnx_COM_b06FwRodataLen;
	bf->bnx_COM_b06FwBssAddr = bnx_COM_b06FwBssAddr;
	bf->bnx_COM_b06FwBssLen = bnx_COM_b06FwBssLen;
	bf->bnx_COM_b06FwSbssAddr = bnx_COM_b06FwSbssAddr;
	bf->bnx_COM_b06FwSbssLen = bnx_COM_b06FwSbssLen;

	memcpy(bf->bnx_TXP_b06FwData, bnx_TXP_b06FwData, sizeof bnx_TXP_b06FwData);
	memcpy(bf->bnx_TXP_b06FwRodata, bnx_TXP_b06FwRodata, sizeof bnx_TXP_b06FwRodata);
	memcpy(bf->bnx_TXP_b06FwBss, bnx_TXP_b06FwBss, sizeof bnx_TXP_b06FwBss);
	memcpy(bf->bnx_TXP_b06FwSbss, bnx_TXP_b06FwSbss, sizeof bnx_TXP_b06FwSbss);

	bf->firmlength = sizeof bnx_COM_b06FwText;

	printf("creating %s length %d [%d+%d]\n",
	    FILENAME, len + bf->firmlength, len, bf->firmlength); 
	fd = open(FILENAME, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1)
		err(1, FILENAME);

	rlen = write(fd, bf, len);
	if (rlen == -1)
		err(1, "%s", FILENAME);
	if (rlen != len)
		errx(1, "%s: short write", FILENAME);

	rlen = write(fd, bnx_COM_b06FwText, sizeof bnx_COM_b06FwText);
	if (rlen == -1)
		err(1, "%s", FILENAME);
	if (rlen != sizeof bnx_COM_b06FwText)
		errx(1, "%s: short write", FILENAME);

	free(bf);
	close(fd);
	return 0;
}
