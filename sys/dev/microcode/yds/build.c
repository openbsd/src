/*	$OpenBSD: build.c,v 1.1 2004/12/20 12:29:40 deraadt Exp $	*/

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
#include <dev/pci/ydsvar.h>
#include <fcntl.h>
#include <stdlib.h>

#include "yds_hwmcode.h"

#define FILENAME "yds"

int
main(int argc, char *argv[])
{
	struct	yds_firmware yfproto, *yf;
	int len, fd, i;

	len = sizeof(*yf) - sizeof(yfproto.data) +
	    sizeof(yds_dsp_mcode) + sizeof(yds_ds1_ctrl_mcode) +
	    sizeof(yds_ds1e_ctrl_mcode);

	yf = (struct yds_firmware *)malloc(len);
	bzero(yf, len);

	yf->dsplen = sizeof(yds_dsp_mcode);
	yf->ds1len = sizeof(yds_ds1_ctrl_mcode);
	yf->ds1elen = sizeof(yds_ds1e_ctrl_mcode);

	bcopy(yds_dsp_mcode, &yf->data[0], yf->dsplen);
	bcopy(yds_ds1_ctrl_mcode, &yf->data[yf->dsplen], yf->ds1len);
	bcopy(yds_ds1_ctrl_mcode, &yf->data[yf->dsplen + yf->ds1len],
	    yf->ds1elen);

	printf("creating %s length %d [%d+%d+%d]\n",
	    FILENAME, len, yf->dsplen, yf->ds1len, yf->ds1elen);
	fd = open(FILENAME, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1)
		err(1, FILENAME);

	write(fd, yf, len);
	free(yf);
	close(fd);
	return 0;
}
