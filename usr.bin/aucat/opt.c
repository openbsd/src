/*	$OpenBSD: opt.c,v 1.1 2009/07/25 08:44:27 ratchov Exp $	*/
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

struct optlist opt_list = SLIST_HEAD_INITIALIZER(&opt_list);

void
opt_new(char *name, struct aparams *wpar, struct aparams *rpar, int maxweight)
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
	o->wpar = *wpar;
	o->rpar = *rpar;
	o->maxweight = maxweight;
#ifdef DEBUG
	if (debug_level > 0) {
		fprintf(stderr, "opt_new: %s: wpar=", o->name);
		aparams_print(&o->wpar);
		fprintf(stderr, ", rpar=");
		aparams_print(&o->rpar);
		fprintf(stderr, ", vol=%u\n", o->maxweight);
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
			DPRINTF("opt_byname: %s found\n", o->name);
			return o;
		}
	}
	DPRINTF("opt_byname: %s not found\n", name);
	return NULL;
}

