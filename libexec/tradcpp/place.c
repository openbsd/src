/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David A. Holland.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "array.h"
#include "place.h"

struct placefile {
	struct place includedfrom;
	char *dir;
	char *name;
	int depth;
	bool fromsystemdir;
};
DECLARRAY(placefile, static UNUSED);
DEFARRAY(placefile, static);

static struct placefilearray placefiles;
static bool overall_failure;

static const char *myprogname;

////////////////////////////////////////////////////////////
// seenfiles

static
struct placefile *
placefile_create(const struct place *from, const char *name,
		 bool fromsystemdir)
{
	struct placefile *pf;
	const char *s;
	size_t len;

	pf = domalloc(sizeof(*pf));
	pf->includedfrom = *from;

	s = strrchr(name, '/');
	len = (s == NULL) ? 0 : s - name;
	pf->dir = dostrndup(name, len);

	pf->name = dostrdup(name);
	pf->fromsystemdir = fromsystemdir;

	if (from->file != NULL) {
		pf->depth = from->file->depth + 1;
	} else {
		pf->depth = 1;
	}
	return pf;
}

static
void
placefile_destroy(struct placefile *pf)
{
	dostrfree(pf->name);
	dofree(pf, sizeof(*pf));
}

DESTROYALL_ARRAY(placefile, );

const char *
place_getparsedir(const struct place *place)
{
	if (place->file == NULL) {
		return ".";
	}
	return place->file->dir;
}

const struct placefile *
place_addfile(const struct place *place, const char *file, bool issystem)
{
	struct placefile *pf;

	pf = placefile_create(place, file, issystem);
	placefilearray_add(&placefiles, pf, NULL);
	if (pf->depth > 120) {
		complain(place, "Maximum include nesting depth exceeded");
		die();
	}
	return pf;
}

////////////////////////////////////////////////////////////
// places

void
place_setnowhere(struct place *p)
{
	p->type = P_NOWHERE;
	p->file = NULL;
	p->line = 0;
	p->column = 0;
}

void
place_setbuiltin(struct place *p, unsigned num)
{
	p->type = P_BUILTIN;
	p->file = NULL;
	p->line = num;
	p->column = 1;
}

void
place_setcommandline(struct place *p, unsigned line, unsigned column)
{
	p->type = P_COMMANDLINE;
	p->file = NULL;
	p->line = line;
	p->column = column;
}

void
place_setfilestart(struct place *p, const struct placefile *pf)
{
	p->type = P_FILE;
	p->file = pf;
	p->line = 1;
	p->column = 1;
}

static
const char *
place_getname(const struct place *p)
{
	switch (p->type) {
	    case P_NOWHERE: return "<nowhere>";
	    case P_BUILTIN: return "<built-in>";
	    case P_COMMANDLINE: return "<command-line>";
	    case P_FILE: return p->file->name;
	}
	assert(0);
	return NULL;
}

static
void
place_printfrom(const struct place *p)
{
	const struct place *from;

	if (p->file == NULL) {
		return;
	}
	from = &p->file->includedfrom;
	if (from->type != P_NOWHERE) {
		place_printfrom(from);
		fprintf(stderr, "In file included from %s:%u:%u:\n",
			place_getname(from), from->line, from->column);
	}
}

////////////////////////////////////////////////////////////
// complaints

void
complain_init(const char *pn)
{
	myprogname = pn;
}

void
complain(const struct place *p, const char *fmt, ...)
{
	va_list ap;

	if (p != NULL) {
		place_printfrom(p);
		fprintf(stderr, "%s:%u:%u: ", place_getname(p),
			p->line, p->column);
	} else {
		fprintf(stderr, "%s: ", myprogname);
	}
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

void
complain_fail(void)
{
	overall_failure = true;
}

bool
complain_failed(void)
{
	return overall_failure;
}

////////////////////////////////////////////////////////////
// module init and cleanup

void
place_init(void)
{
	placefilearray_init(&placefiles);
}

void
place_cleanup(void)
{
	placefilearray_destroyall(&placefiles);
	placefilearray_cleanup(&placefiles);
}
