/*	$OpenBSD: opt.c,v 1.5 2010/04/03 17:40:33 ratchov Exp $	*/
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "opt.h"
#ifdef DEBUG
#include "dbg.h"
#endif

struct optlist opt_list = SLIST_HEAD_INITIALIZER(&opt_list);

void
opt_new(char *name, struct aparams *wpar, struct aparams *rpar,
    int maxweight, int mmc, unsigned mode)
{
	struct opt *o;
	unsigned len;
	char c;

	for (len = 0; name[len] != '\0'; len++) {
		if (len == OPT_NAMEMAX) {
			fprintf(stderr, "%s: name too long\n", name);
			exit(1);
		}
		c = name[len];
		if (c < 'a' && c > 'z' &&
		    c < 'A' && c > 'Z' &&
		    c < '0' && c > '9' && 
		    c != '_') {
			fprintf(stderr, "%s: '%c' not allowed\n", name, c);
			exit(1);
		}
	}
	o = malloc(sizeof(struct opt));
	if (o == NULL) {
		perror("opt_new: malloc");
		exit(1);
	}
	memcpy(o->name, name, len + 1);
	if (mode & MODE_RECMASK)
		o->wpar = (mode & MODE_MON) ? *rpar : *wpar;
	if (mode & MODE_PLAY)
		o->rpar = *rpar;
	o->maxweight = maxweight;
	o->mmc = mmc;
	o->mode = mode;
#ifdef DEBUG
	if (debug_level >= 2) {
		dbg_puts(o->name);
		dbg_puts(":");
		if (mode & MODE_REC) {
			dbg_puts(" rec=");
			dbg_putu(o->wpar.cmin);
			dbg_puts(":");
			dbg_putu(o->wpar.cmax);
		}
		if (mode & MODE_PLAY) {
			dbg_puts(" play=");
			dbg_putu(o->rpar.cmin);
			dbg_puts(":");
			dbg_putu(o->rpar.cmax);
			dbg_puts(" vol=");
			dbg_putu(o->maxweight);
		}
		if (mode & MODE_MON) {
			dbg_puts(" mon=");
			dbg_putu(o->wpar.cmin);
			dbg_puts(":");
			dbg_putu(o->wpar.cmax);
		}
		if (o->mmc)
			dbg_puts(" mmc");
		dbg_puts("\n");
	}
#endif
	SLIST_INSERT_HEAD(&opt_list, o, entry);
}

struct opt *
opt_byname(char *name)
{
	struct opt *o;

	SLIST_FOREACH(o, &opt_list, entry) {
		if (strcmp(name, o->name) == 0) {
#ifdef DEBUG
			if (debug_level >= 3) {
				dbg_puts(o->name);
				dbg_puts(": option found\n");
			}
#endif
			return o;
		}
	}
#ifdef DEBUG
	if (debug_level >= 3) {
		dbg_puts(name);
		dbg_puts(": option not found\n");
	}
#endif
	return NULL;
}

