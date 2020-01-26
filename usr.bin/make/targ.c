/*	$OpenBSD: targ.c,v 1.85 2020/01/26 12:41:21 espie Exp $ */
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ohash.h>
#include "config.h"
#include "defines.h"
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
#include "dump.h"

static struct ohash targets;	/* hash table of targets */
struct ohash_info gnode_info = {
	offsetof(GNode, name), NULL, hash_calloc, hash_free, element_alloc
};

static GNode *Targ_mk_node(const char *, const char *, unsigned int,
    unsigned char, unsigned int);

#define Targ_mk_constant(n, type) \
    Targ_mk_special_node(n, sizeof(n), K_##n, type, SPECIAL_NONE, 0)

GNode *begin_node, *end_node, *interrupt_node, *DEFAULT;

void
Targ_Init(void)
{
	/* A small make file already creates 200 targets.  */
	ohash_init(&targets, 10, &gnode_info);
	begin_node = Targ_mk_constant(NODE_BEGIN, 
	    OP_DUMMY | OP_NOTMAIN | OP_NODEFAULT);
	end_node = Targ_mk_constant(NODE_END, 
	    OP_DUMMY | OP_NOTMAIN | OP_NODEFAULT);
	interrupt_node = Targ_mk_constant(NODE_INTERRUPT,
	    OP_DUMMY | OP_NOTMAIN | OP_NODEFAULT);
	DEFAULT = Targ_mk_constant(NODE_DEFAULT, 
	    OP_DUMMY | OP_NOTMAIN| OP_TRANSFORM | OP_NODEFAULT);

}

static GNode *
Targ_mk_node(const char *name, const char *ename, 
    unsigned int type, unsigned char special, unsigned int special_op)
{
	GNode *gn;

	gn = ohash_create_entry(&gnode_info, name, &ename);
	gn->path = NULL;
	gn->type = type;
	gn->special = special;
	gn->special_op = special_op;
	gn->children_left = 0;
	gn->must_make = false;
	gn->built_status = UNKNOWN;
	gn->in_cycle = false;
	gn->child_rebuilt = false;
	gn->order = 0;
	ts_set_out_of_date(gn->mtime);
	gn->youngest = gn;
	Lst_Init(&gn->cohorts);
	Lst_Init(&gn->parents);
	Lst_Init(&gn->children);
	Lst_Init(&gn->predecessors);
	Lst_Init(&gn->successors);
	SymTable_Init(&gn->localvars);
	gn->impliedsrc = NULL;
	Lst_Init(&gn->commands);
	gn->suffix = NULL;
	gn->next = NULL;
	gn->basename = NULL;
	gn->sibling = gn;
	gn->groupling = NULL;

#ifdef STATS_GN_CREATION
	STAT_GN_COUNT++;
#endif

	return gn;
}

GNode *
Targ_NewGNi(const char *name, const char *ename)
{
	return Targ_mk_node(name, ename, OP_ZERO, SPECIAL_NONE, 0);
}

GNode *
Targ_FindNodei(const char *name, const char *ename, int flags)
{
	uint32_t hv;
	GNode *gn;
	unsigned int slot;

	hv = ohash_interval(name, &ename);

	slot = ohash_lookup_interval(&targets, name, ename, hv);

	gn = ohash_find(&targets, slot);

	if (gn == NULL && (flags & TARG_CREATE)) {
		gn = Targ_NewGNi(name, ename);
		ohash_insert(&targets, slot, gn);
	}

	return gn;
}

GNode *
Targ_mk_special_node(const char *name, size_t n, uint32_t hv,
    unsigned int type, unsigned char special, unsigned int special_op)
{
	GNode *gn;
	unsigned int slot;
	const char *ename = name + n - 1;

	slot = ohash_lookup_interval(&targets, name, ename, hv);

	assert(ohash_find(&targets, slot) == NULL);

	gn = Targ_mk_node(name, ename, type, special, special_op);
	ohash_insert(&targets, slot, gn);

	return gn;
}

void
Targ_FindList(Lst nodes, Lst names)
{
	LstNode ln;
	GNode *gn;
	char *name;

	for (ln = Lst_First(names); ln != NULL; ln = Lst_Adv(ln)) {
		name = Lst_Datum(ln);
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

bool
node_is_real(GNode *gn)
{
	return (gn->type & OP_DUMMY) == 0;
}

void
Targ_PrintCmd(void *p)
{
	const struct command *cmd = p;
	printf("\t%s\n", cmd->string);
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
		PRINTBIT(IGNORE);
		PRINTBIT(PRECIOUS);
		PRINTBIT(SILENT);
		PRINTBIT(MAKE);
		PRINTBIT(INVISIBLE);
		PRINTBIT(NOTMAIN);
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
	case REBUILT:
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

struct ohash *
targets_hash()
{
	return &targets;
}
