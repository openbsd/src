/*	$OpenBSD: build.c,v 1.1 2004/11/22 18:49:05 deraadt Exp $	*/

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
#include <dev/usb/if_kuevar.h>
#include <fcntl.h>

#include "kue_fw.h"

#define FILENAME "kue"

int
main(int argc, char *argv[])
{
	struct	kue_firmware kfproto, *kf;
	int len, fd, i;

	len = sizeof(*kf) - sizeof(kfproto.data) +
	    sizeof(kue_code_seg) + sizeof(kue_fix_seg) +
	    sizeof(kue_trig_seg);

	kf = (struct kue_firmware *)malloc(len);
	bzero(kf, len);

	kf->codeseglen = sizeof(kue_code_seg);
	kf->fixseglen = sizeof(kue_fix_seg);
	kf->trigseglen = sizeof(kue_trig_seg);

	bcopy(kue_code_seg, &kf->data[0], kf->codeseglen);
	bcopy(kue_fix_seg, &kf->data[kf->codeseglen], kf->fixseglen);
	bcopy(kue_trig_seg, &kf->data[kf->codeseglen + kf->fixseglen],
	    kf->trigseglen);

	printf("creating %s length %d [%d+%d+%d]\n",
	    FILENAME, len, kf->codeseglen, kf->fixseglen, kf->trigseglen);
	fd = open(FILENAME, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1)
		err(1, FILENAME);

	write(fd, kf, len);
	free(kf);
	close(fd);
	return 0;
}
