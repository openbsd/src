/*	$OpenBSD: opt.h,v 1.10 2011/10/12 07:20:04 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
#ifndef OPT_H
#define OPT_H

#include <sys/queue.h>
#include "aparams.h"

struct dev;

struct opt {
	struct opt *next;
#define OPT_NAMEMAX 11
	char name[OPT_NAMEMAX + 1];
	int maxweight;		/* max dynamic range for clients */
	struct aparams wpar;	/* template for clients write params */
	struct aparams rpar;	/* template for clients read params */
	int mmc;		/* true if MMC control enabled */
	int join;		/* true if join/expand enabled */
	unsigned mode;		/* bitmap of MODE_XXX */
	struct dev *dev;	/* device to which we're attached */
};

extern struct opt *opt_list;

struct opt *opt_new(char *, struct dev *, struct aparams *, struct aparams *,
    int, int, int, unsigned);
int opt_bind(struct opt *);
struct opt *opt_byname(char *);

#endif /* !defined(OPT_H) */
