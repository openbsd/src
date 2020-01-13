#ifndef SUFF_H
#define SUFF_H
/*	$OpenBSD: suff.h,v 1.12 2020/01/13 14:05:21 espie Exp $ */

/*
 * Copyright (c) 2001-2019 Marc Espie.
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

extern void Suff_Init(void);

/* Suff_DisableAllSuffixes():
 *	disable current suffixes and the corresponding rules.
 *	They may be re-activated by adding a suffix anew.  */
extern void Suff_DisableAllSuffixes(void);
/* gn = Suff_ParseAsTransform(line, eline):
 *	Try parsing a [line,eline[ as a suffix transformation
 *	(.a.b or .a). If successful, returns a gn we can add
 *	commands to (this is actually a transform kept on a
 * 	separate hash from normal targets).  Otherwise returns NULL. */
extern GNode *Suff_ParseAsTransform(const char *, const char *);
/* Suff_AddSuffixi(name, ename):
 *	add the passed string interval [name,ename[ as a known
 *	suffix. */
extern void Suff_AddSuffixi(const char *, const char *);
/* process_suffixes_after_makefile_is_read():
 *	finish setting up the transformation graph for Suff_FindDep
 *	and the .PATH.sfx paths get the default path appended for
 *	find_suffix_path().  */
extern void process_suffixes_after_makefile_is_read(void);
/* Suff_FindDeps(gn):
 *	find implicit dependencies for gn and fill out corresponding
 *	fields. */
extern void Suff_FindDeps(GNode *);
/* l = find_suffix_path(gn):
 *	returns the path associated with a gn, either because of its
 *	suffix, or the default path.  */
extern Lst find_suffix_path(GNode *);
/* Suff_PrintAll():
 *	displays all suffix information. */
extern void Suff_PrintAll(void);
/* path = find_best_path(name):
 *	find the best path for the name, according to known suffixes.
 */
extern Lst find_best_path(const char *name);
#endif
