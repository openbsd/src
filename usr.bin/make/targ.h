#ifndef TARG_H
#define TARG_H
/*	$OpenPackages$ */
/*	$OpenBSD: targ.h,v 1.5 2007/11/02 17:27:24 espie Exp $ */

/*
 * Copyright (c) 2001 Marc Espie.
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

#ifndef TIMESTAMP_TYPE
#include "timestamp_t.h"
#endif
/*
 * The TARG_ constants are used when calling the Targ_FindNode functions.
 * They simply tell the function what to do if the desired node is not found.
 * If the TARG_CREATE constant is given, a new, empty node will be created
 * for the target, placed in the table of all targets and its address returned.
 * If TARG_NOCREATE is given, a NULL pointer will be returned.
 */
#define TARG_CREATE	0x01	  /* create node if not found */
#define TARG_NOCREATE	0x00	  /* don't create it */

extern void Targ_Init(void);
#ifdef CLEANUP
extern void Targ_End(void);
#else
#define Targ_End()
#endif
extern GNode *Targ_NewGNi(const char *, const char *);
#define Targ_NewGN(n)	Targ_NewGNi(n, NULL);
extern GNode *Targ_FindNodei(const char *, const char *, int);
#define Targ_FindNode(n, i)	Targ_FindNodei(n, NULL, i)



/* set of helpers for constant nodes */
extern GNode *Targ_FindNodeih(const char *, const char *, uint32_t, int);

extern inline GNode *
Targ_FindNodeh(const char *, size_t, uint32_t, int);
extern inline GNode *
Targ_FindNodeh(const char *name, size_t n, uint32_t hv, int flags)
{
	return Targ_FindNodeih(name, name + n - 1, hv, flags);
}
extern void Targ_FindList(Lst, Lst);
extern bool Targ_Ignore(GNode *);
extern bool Targ_Silent(GNode *);
extern bool Targ_Precious(GNode *);
extern void Targ_PrintCmd(void *);
extern void Targ_PrintType(int);
extern void Targ_PrintGraph(int);

extern GNode *begin_node, *end_node, *interrupt_node, *DEFAULT;
struct ohash_info;

extern struct ohash_info gnode_info;

extern void look_harder_for_target(GNode *);
extern void Targ_setdirs(const char *, const char *);
#endif
