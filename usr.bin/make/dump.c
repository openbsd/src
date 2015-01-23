/* $OpenBSD: dump.c,v 1.7 2015/01/23 13:18:40 espie Exp $ */
/*
 * Copyright (c) 2012 Marc Espie.
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
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ohash.h>
#include "defines.h"
#include "gnode.h"
#include "dump.h"
#include "targ.h"
#include "var.h"
#include "memory.h"
#include "suff.h"
#include "lst.h"
#include "timestamp.h"
#include "dir.h"

/* since qsort doesn't have user data, this needs to be a global... */
static ptrdiff_t cmp_offset;
static void targ_dump(bool);

static int 
compare_names(const void *a, const void *b)
{
	const char **pa = (const char **)a;
	const char **pb = (const char **)b;
	return strcmp((*pa) + cmp_offset, (*pb) + cmp_offset);
}

void *
sort_ohash_by_name(struct ohash *h)
{
	cmp_offset = h->info.key_offset;

	return sort_ohash(h, compare_names);
}

void *
sort_ohash(struct ohash *h, int (*comparison)(const void *, const void *))
{
	unsigned int i, j;
	void *e;
	size_t n = ohash_entries(h);
	void **t = ereallocarray(NULL, n+1, sizeof(void *));
	cmp_offset = h->info.key_offset;

	for (i = 0, e = ohash_first(h, &j); e != NULL; e = ohash_next(h, &j))
	    	t[i++] = e;
	qsort(t, n, sizeof(void *), comparison);
	/* add an extra entry to be able to figure out the end without needing
	 * to keep a counter */
	t[n] = NULL;
	return t;
}

static void
TargPrintName(void *gnp)
{
	GNode *gn = gnp;
	printf("%s ", gn->name);
}

static void
TargPrintOnlySrc(GNode *gn)
{
	if (OP_NOP(gn->type) && gn->special == SPECIAL_NONE &&
	    !(gn->type & OP_DUMMY)) {
	    	if (gn->path != NULL)
			printf("#\t%s [%s]\n", gn->name, 
			    strcmp(gn->path, gn->name) == 0 ? "=" : gn->path);
		else
			printf("#\t%s\n", gn->name);
    	}
}

static void
TargPrintNode(GNode *gn, bool full)
{
	if (OP_NOP(gn->type))
		return;
	switch((gn->special & SPECIAL_MASK)) {
	case SPECIAL_SUFFIXES:
	case SPECIAL_PHONY:
	case SPECIAL_ORDER:
	case SPECIAL_NOTHING:
	case SPECIAL_MAIN:
	case SPECIAL_IGNORE:
		return;
	default:
		break;
	}
	if (full) {
		printf("# %d unmade prerequisites\n", gn->unmade);
		if (! (gn->type & (OP_JOIN|OP_USE|OP_EXEC))) {
			if (!is_out_of_date(gn->mtime)) {
				printf("# last modified %s: %s\n",
				      time_to_string(&gn->mtime),
				      status_to_string(gn));
			} else if (gn->built_status != UNKNOWN) {
				printf("# non-existent (maybe): %s\n",
				    status_to_string(gn));
			} else {
				printf("# unmade\n");
			}
		}
	}
	if (!Lst_IsEmpty(&gn->parents)) {
		printf("# parent targets: ");
		Lst_Every(&gn->parents, TargPrintName);
		fputc('\n', stdout);
	}
	if (gn->impliedsrc)
		printf("# implied prerequisite: %s\n", gn->impliedsrc->name);

	printf("%-16s", gn->name);
	switch (gn->type & OP_OPMASK) {
	case OP_DEPENDS:
		printf(": "); break;
	case OP_FORCE:
		printf("! "); break;
	case OP_DOUBLEDEP:
		printf(":: "); break;
	}
	Targ_PrintType(gn->type);
	Lst_Every(&gn->children, TargPrintName);
	fputc('\n', stdout);
	Lst_Every(&gn->commands, Targ_PrintCmd);
	printf("\n\n");
	if (gn->type & OP_DOUBLEDEP) {
		LstNode ln;

		for (ln = Lst_First(&gn->cohorts); ln != NULL; ln = Lst_Adv(ln))
			TargPrintNode((GNode *)Lst_Datum(ln), full);
	}
}

static void
dump_special(GNode **t, const char *name, int prop)
{
	unsigned int i;
	bool first = true;

	for (i = 0; t[i] != NULL; i++)
		if (t[i]->type & prop) {
			if (first) {
				printf("%s:", name);
				first = false;
			}
			printf(" %s", t[i]->name);
	    	}
	if (!first)
		printf("\n\n");
}

static void
targ_dump(bool full)
{
	GNode **t = sort_ohash_by_name(targets_hash());
	unsigned int i;

	printf("#   Input graph:\n");
	for (i = 0; t[i] != NULL; i++)
		TargPrintNode(t[i], full);
	printf("\n\n");

	dump_special(t, ".PHONY", OP_PHONY);
	dump_special(t, ".PRECIOUS", OP_PRECIOUS);
	dump_special(t, ".SILENT", OP_SILENT);
	dump_special(t, ".IGNORE", OP_IGNORE);
	printf("#   Other target names:\n");
	for (i = 0; t[i] != NULL; i++)
		TargPrintOnlySrc(t[i]);
	printf("\n");
	free(t);
}

static bool dumped_once = false;

void
dump_data(void)
{
	Var_Dump();
	Suff_PrintAll();
	targ_dump(false);
	dumped_once = true;
}

void
post_mortem(void)
{
	if (!dumped_once) {
		Var_Dump();
		Suff_PrintAll();
	}
	targ_dump(true);
}
