/*	$OpenBSD: landisk_installboot.c,v 1.9 2018/09/01 16:55:29 krw Exp $	*/

/*
 * Copyright (c) 2013 Joel Sing <jsing@openbsd.org>
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

#include <stdlib.h>
#include <unistd.h>

#include "installboot.h"

char	*bootldr;

void
md_init(void)
{
	stages = 2;
	stage1 = "/usr/mdec/xxboot";
	stage2 = "/usr/mdec/boot";

	bootldr = "/boot";
}

void
md_loadboot(void)
{
}

void
md_installboot(int devfd, char *dev)
{
	/* XXX - is this necessary? */
	sync();

	bootldr = fileprefix(root, bootldr);
	if (bootldr == NULL)
		exit(1);
	if (!nowrite)
		if (filecopy(stage2, bootldr) == -1)
			exit(1);

	/* Write bootblock into the superblock. */
	bootstrap(devfd, dev, stage1);
}
