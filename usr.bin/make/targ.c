/*	$OpenBSD: targ.c,v 1.62 2010/07/19 19:46:44 espie Exp $ */
/*	$NetBSD: targ.c,v 1.11 1997/02/20 16:51:50 christos Exp $	*/

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Extensive code changes for the OpenBSD project.
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
/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * targ.c --
 *		Target nodes are kept into a hash table.
 *
 * Interface:
 *	Targ_Init		Initialization procedure.
 *
 *	Targ_End		Cleanup the module
 *
 *	Targ_NewGN		Create a new GNode for the passed target
 *				(string). The node is *not* placed in the
 *				hash table, though all its fields are
 *				initialized.
 *
 *	Targ_FindNode		Find the node for a given target, creating
 *				and storing it if it doesn't exist and the
 *				flags are right (TARG_CREATE)
 *
 *	Targ_FindList		Given a list of names, find nodes for all
 *				of them, creating nodes if needed.
 *
 *	Targ_Ignore		Return true if errors should be ignored when
 *				creating the given target.
 *
 *	Targ_Silent		Return true if we should be silent when
 *				creating the given target.
 *
 *	Targ_Precious		Return true if the target is precious and
 *				should not be removed if we are interrupted.
 *
 * Debugging:
 *	Targ_PrintGraph 	Print out the entire graphm all variables
 *				and statistics for the directory cache. Should
 *				print something for suffixes, too, but...
 */

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "ohash.h"
#include "stats.h"
#include "suff.h"
#include "var.h"
#include "targ.h"
#include "memory.h"
#include "gnode.h"
#include "extern.h"
#include "timestamp.h"
#include "lst.h"
#include "node_int.h"
#include "nodehashconsts.h"
#ifdef CLEANUP
#include <stdlib.h>
#endif

static struct ohash targets;	/* hash table of targets */
struct ohash_info gnode_info = {
	offsetof(GNode, name), NULL, hash_alloc, hash_free, element_alloc
};

static void TargPrintOnlySrc(GNode *);
static void TargPrintName(void *);
static void TargPrintNode(GNode *, int);
#ifdef CLEANUP
static LIST allTargets;
static void TargFreeGN(void *);
#endif
#define Targ_FindConstantNode(n, f) Targ_FindNodeh(n, sizeof(n), K_##n, f)


GNode *begin_node, *end_node, *interrupt_node, *DEFAULT;

void
Targ_Init(void)
{
	/* A small make file already creates 200 targets.  */
	ohash_init(&targets, 10, &gnode_info);
#ifdef CLEANUP
	Lst_Init(&allTargets);
#endif
	begin_node = Targ_FindConstantNode(NODE_BEGIN, TARG_CREATE);
	begin_node->type |= OP_DUMMY | OP_NOTMAIN | OP_NODEFAULT;
	end_node = Targ_FindConstantNode(NODE_END, TARG_CREATE);
	end_node->type |= OP_DUMMY | OP_NOTMAIN | OP_NODEFAULT;
	interrupt_node = Targ_FindConstantNode(NODE_INTERRUPT, TARG_CREATE);
	interrupt_node->type |= OP_DUMMY | OP_NOTMAIN | OP_NODEFAULT;
	DEFAULT = Targ_FindConstantNode(NODE_DEFAULT, TARG_CREATE);
	DEFAULT->type |= OP_DUMMY | OP_NOTMAIN| OP_TRANSFORM | OP_NODEFAULT;

}

#ifdef CLEANUP
void
Targ_End(void)
{
	Lst_Every(&allTargets, TargFreeGN);
	ohash_delete(&targets);
}
#endif

GNode *
Targ_NewGNi(const char *name, const char *ename)
{
	GNode *gn;

	gn = ohash_create_entry(&gnode_info, name, &ename);
	gn->path = NULL;
	if (name[0] == '-' && name[1] == 'l')
		gn->type = OP_LIB;
	else
		gn->type = 0;

	gn->special = SPECIAL_NONE;
	gn->unmade = 0;
	gn->must_make = false;
	gn->built_status = UNKNOWN;
	gn->childMade =	false;
	gn->order = 0;
	ts_set_out_of_date(gn->mtime);
	ts_set_out_of_date(gn->cmtime);
	Lst_Init(&gn->cohorts);
	Lst_Init(&gn->parents);
	Lst_Init(&gn->children);
	Lst_Init(&gn->successors);
	Lst_Init(&gn->preds);
	SymTable_Init(&gn->context);
	gn->lineno = 0;
	gn->fname = NULL;
	gn->impliedsrc = NULL;
	Lst_Init(&gn->commands);
	Lst_Init(&gn->expanded);
	gn->suffix = NULL;
	gn->next = NULL;
	gn->basename = NULL;
	gn->sibling = gn;
	gn->build_lock = false;

#ifdef STATS_GN_CREATION
	STAT_GN_COUNT++;
#endif

#ifdef CLEANUP
	Lst_AtEnd(&allTargets, gn);
#endif
	return gn;
}

#ifdef CLEANUP
static void
TargFreeGN(void *gnp)
{
	GNode *gn = (GNode *)gnp;

	efree(gn->path);
	Lst_Destroy(&gn->cohorts, NOFREE);
	Lst_Destroy(&gn->parents, NOFREE);
	Lst_Destroy(&gn->children, NOFREE);
	Lst_Destroy(&gn->successors, NOFREE);
	Lst_Destroy(&gn->preds, NOFREE);
	Lst_Destroy(&gn->commands, NOFREE);
	SymTable_Destroy(&gn->context);
	free(gn);
}
#endif

GNode *
Targ_FindNodei(const char *name, const char *ename, int flags)
{
	uint32_t hv;

	hv = ohash_interval(name, &ename);
	return Targ_FindNodeih(name, ename, hv, flags);
}

GNode *
Targ_FindNodeih(const char *name, const char *ename, uint32_t hv, int flags)
{
	GNode *gn;
	unsigned int slot;

	slot = ohash_lookup_interval(&targets, name, ename, hv);

	gn = ohash_find(&targets, slot);

	if (gn == NULL && (flags & TARG_CREATE)) {
		gn = Targ_NewGNi(name, ename);
		ohash_insert(&targets, slot, gn);
	}

	return gn;
}

void
Targ_FindList(Lst nodes, Lst names)
{
	LstNode ln;
	GNode *gn;
	char *name;

	for (ln = Lst_First(names); ln != NULL; ln = Lst_Adv(ln)) {
		name = (char *)Lst_Datum(ln);
		gn = Targ_FindNode(name, TARG_CREATE);
		/* Note: Lst_AtEnd must come before the Lst_Concat so the nodes
		 * are added to the list in the order in which they were
		 * encountered in the makefile.  */
		Lst_AtEnd(nodes, gn);
		if (gn->type & OP_DOUBLEDEP)
			Lst_Concat(nodes, &gn->cohorts);
	}
}

bool
Targ_Ignore(GNode *gn)
{
	if (ignoreErrors || gn->type & OP_IGNORE)
		return true;
	else
		return false;
}

bool
Targ_Silent(GNode *gn)
{
	if (beSilent || gn->type & OP_SILENT)
		return true;
	else
		return false;
}

bool
Targ_Precious(GNode *gn)
{
	if (allPrecious || (gn->type & (OP_PRECIOUS|OP_DOUBLEDEP|OP_PHONY)))
		return true;
	else
		return false;
}

static void
TargPrintName(void *gnp)
{
	GNode *gn = (GNode *)gnp;
	printf("%s ", gn->name);
}


void
Targ_PrintCmd(void *cmd)
{
	printf("\t%s\n", (char *)cmd);
}

void
Targ_PrintType(int type)
{
	int    tbit;

#define PRINTBIT(attr)	case CONCAT(OP_,attr): printf("." #attr " "); break
#define PRINTDBIT(attr) case CONCAT(OP_,attr): if (DEBUG(TARG)) printf("." #attr " "); break

	type &= ~OP_OPMASK;

	while (type) {
		tbit = 1 << (ffs(type) - 1);
		type &= ~tbit;

		switch (tbit) {
		PRINTBIT(OPTIONAL);
		PRINTBIT(USE);
		PRINTBIT(EXEC);
		PRINTBIT(IGNORE);
		PRINTBIT(PRECIOUS);
		PRINTBIT(SILENT);
		PRINTBIT(MAKE);
		PRINTBIT(JOIN);
		PRINTBIT(INVISIBLE);
		PRINTBIT(NOTMAIN);
		PRINTDBIT(LIB);
		/*XXX: MEMBER is defined, so CONCAT(OP_,MEMBER) gives OP_"%" */
		case OP_MEMBER:
			if (DEBUG(TARG))
				printf(".MEMBER ");
			break;
		PRINTDBIT(ARCHV);
		}
    }
}
const char *
status_to_string(GNode *gn)
{
	switch (gn->built_status) {
	case UNKNOWN:
		return "unknown";
	case MADE:
		return "made";
	case UPTODATE:
		return "up-to-date";
	case ERROR:
		return "error when made";
	case ABORTED:
		return "aborted";
	default:
		return "other status";
	}
}

static void
TargPrintNode(GNode *gn, int pass)
{
	if (OP_NOP(gn->type))
		return;
	printf("#\n");
	if (pass == 2) {
		printf("# %d unmade children\n", gn->unmade);
		if (! (gn->type & (OP_JOIN|OP_USE|OP_EXEC))) {
			if (!is_out_of_date(gn->mtime)) {
				printf("# last modified %s: %s\n",
				      time_to_string(gn->mtime),
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
		printf("# parents: ");
		Lst_Every(&gn->parents, TargPrintName);
		fputc('\n', stdout);
	}
	if (gn->impliedsrc)
		printf("# implied source: %s\n", gn->impliedsrc->name);

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
			TargPrintNode((GNode *)Lst_Datum(ln), pass);
	}
}

static void
TargPrintOnlySrc(GNode *gn)
{
	if (OP_NOP(gn->type) && gn->special == SPECIAL_NONE &&
	    !(gn->type & OP_DUMMY))
		printf("#\t%s [%s]\n", gn->name,
		    gn->path != NULL ? gn->path : gn->name);
}

void
Targ_PrintGraph(int pass)	/* Which pass this is. 1 => no processing
				 * 2 => processing done */
{
	GNode		*gn;
	unsigned int	i;

	printf("#*** Input graph:\n");
	for (gn = ohash_first(&targets, &i); gn != NULL;
	    gn = ohash_next(&targets, &i))
		TargPrintNode(gn, pass);
	printf("\n\n");
	printf("#\n#   Files that are only sources:\n");
	for (gn = ohash_first(&targets, &i); gn != NULL;
	    gn = ohash_next(&targets, &i))
		TargPrintOnlySrc(gn);
	Var_Dump();
	printf("\n");
#ifdef DEBUG_DIRECTORY_CACHE
	Dir_PrintDirectories();
	printf("\n");
#endif
	Suff_PrintAll();
}

struct ohash *
targets_hash()
{
	return &targets;
}

GNode *
Targ_FindNodeh(const char *name, size_t n, uint32_t hv, int flags)
{
	return Targ_FindNodeih(name, name + n - 1, hv, flags);
}
