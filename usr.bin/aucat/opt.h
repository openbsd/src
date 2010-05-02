/*	$OpenBSD: opt.h,v 1.7 2010/05/02 11:54:26 ratchov Exp $	*/
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

struct opt {
	SLIST_ENTRY(opt) entry;
#define OPT_NAMEMAX 11
	char name[OPT_NAMEMAX + 1];
	int maxweight;		/* max dynamic range for clients */
	struct aparams wpar;	/* template for clients write params */
	struct aparams rpar;	/* template for clients read params */
	int mmc;		/* true if MMC control enabled */
	int join;		/* true if join/expand enabled */
#define MODE_PLAY	0x1	/* allowed to play */
#define MODE_REC	0x2	/* allowed to rec */
#define MODE_MIDIIN	0x4	/* allowed to read midi */
#define MODE_MIDIOUT	0x8	/* allowed to write midi */
#define MODE_MON	0x10	/* allowed to monitor */
#define MODE_LOOP	0x20	/* deviceless mode */
#define MODE_RECMASK	(MODE_REC | MODE_MON)
	unsigned mode;		/* bitmap of above */
};

SLIST_HEAD(optlist,opt);

void opt_new(char *, struct aparams *, struct aparams *,
    int, int, int, unsigned);
struct opt *opt_byname(char *);

#endif /* !defined(OPT_H) */
