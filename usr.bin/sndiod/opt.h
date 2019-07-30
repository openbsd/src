/*	$OpenBSD: opt.h,v 1.1 2012/11/23 07:03:28 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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

struct dev;

struct opt {
	struct opt *next;
#define OPT_NAMEMAX 11
	char name[OPT_NAMEMAX + 1];
	int maxweight;		/* max dynamic range for clients */
	int pmin, pmax;		/* play channels */
	int rmin, rmax;		/* recording channels */
	int mmc;		/* true if MMC control enabled */
	int dup;		/* true if join/expand enabled */
	int mode;		/* bitmap of MODE_XXX */
	struct dev *dev;	/* device to which we're attached */
};

extern struct opt *opt_list;

struct opt *opt_new(char *, struct dev *, int, int, int, int,
    int, int, int, unsigned int);
void opt_del(struct opt *);
struct opt *opt_byname(char *, unsigned int);

#endif /* !defined(OPT_H) */
