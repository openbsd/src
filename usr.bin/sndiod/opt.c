/*	$OpenBSD: opt.c,v 1.3 2016/01/08 10:54:07 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2011 Alexandre Ratchov <alex@caoua.org>
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
#include <string.h>

#include "dev.h"
#include "opt.h"
#include "utils.h"

struct opt *opt_list = NULL;

/*
 * create a new audio sub-device "configuration"
 */
struct opt *
opt_new(char *name, struct dev *dev,
    int pmin, int pmax, int rmin, int rmax,
    int maxweight, int mmc, int dup, unsigned int mode)
{
	struct opt *o;
	unsigned int len;
	char c;

	if (opt_byname(name, dev->num)) {
		log_puts(name);
		log_puts(": already defined\n");
		return NULL;
	}
	for (len = 0; name[len] != '\0'; len++) {
		if (len == OPT_NAMEMAX) {
			log_puts(name);
			log_puts(": too long\n");
			return NULL;
		}
		c = name[len];
		if ((c < 'a' || c > 'z') &&
		    (c < 'A' || c > 'Z')) {
			log_puts(name);
			log_puts(": only alphabetic chars allowed\n");
			return NULL;
		}
	}
	o = xmalloc(sizeof(struct opt));
	if (mode & MODE_PLAY) {
		o->pmin = pmin;
		o->pmax = pmax;
	}
	if (mode & MODE_RECMASK) {
		o->rmin = rmin;
		o->rmax = rmax;
	}
	o->maxweight = maxweight;
	o->mmc = mmc;
	o->dup = dup;
	o->mode = mode;
	o->dev = dev;
	memcpy(o->name, name, len + 1);
	o->next = opt_list;
	opt_list = o;
	if (log_level >= 2) {
		dev_log(o->dev);
		log_puts(".");
		log_puts(o->name);
		log_puts(":");
		if (o->mode & MODE_REC) {
			log_puts(" rec=");
			log_putu(o->rmin);
			log_puts(":");
			log_putu(o->rmax);
		}
		if (o->mode & MODE_PLAY) {
			log_puts(" play=");
			log_putu(o->pmin);
			log_puts(":");
			log_putu(o->pmax);
			log_puts(" vol=");
			log_putu(o->maxweight);
		}
		if (o->mode & MODE_MON) {
			log_puts(" mon=");
			log_putu(o->rmin);
			log_puts(":");
			log_putu(o->rmax);
		}
		if (o->mode & (MODE_RECMASK | MODE_PLAY)) {
			if (o->mmc)
				log_puts(" mmc");
			if (o->dup)
				log_puts(" dup");
		}
		log_puts("\n");
	}
	return o;
}

struct opt *
opt_byname(char *name, unsigned int num)
{
	struct opt *o;

	for (o = opt_list; o != NULL; o = o->next) {
		if (o->dev->num != num)
			continue;
		if (strcmp(name, o->name) == 0)
			return o;
	}
	return NULL;
}

void
opt_del(struct opt *o)
{
	struct opt **po;

	for (po = &opt_list; *po != o; po = &(*po)->next) {
#ifdef DEBUG
		if (*po == NULL) {
			log_puts("opt_del: not on list\n");
			panic();
		}
#endif
	}
	*po = o->next;
	xfree(o);
}
