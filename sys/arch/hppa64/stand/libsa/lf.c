/*	$OpenBSD: lf.c,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "libsa.h"
#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/param.h>

#include "dev_hppa64.h"

int
lfopen(struct open_file *f, ...)
{
	struct hppa_dev *dp = f->f_devdata;

	if (!(dp->pz_dev = pdc_findev(-1, PCL_NET_MASK|PCL_SEQU)))
		return ENXIO;

	return 0;
}

int
lfclose(struct open_file *f)
{
	free(f->f_devdata, sizeof(struct hppa_dev));
	f->f_devdata = NULL;
	return 0;
}
