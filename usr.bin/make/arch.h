#ifndef ARCH_H
#define ARCH_H
/*	$OpenPackages$ */
/*	$OpenBSD: arch.h,v 1.4 2007/09/17 10:12:35 espie Exp $ */

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

/*-
 * arch --
 *	Functions to manipulate libraries, archives and their members.
 */

#ifndef TIMESTAMP_TYPE
#include "timestamp_t.h"
#endif

/* Initialization and cleanup */
extern void Arch_Init(void);
#ifdef CLEANUP
extern void Arch_End(void);
#else
#define Arch_End()
#endif

/* ok = Arch_ParseArchive(&begin, nodeLst, ctxt);
 *	Given an archive specification, add list of corresponding GNodes to
 *	nodeLst, one for each member in the spec.
 *	false is returned if the specification is invalid for some reason.
 *	Side-effect: begin is bumped to the end of the specification.  */
extern bool Arch_ParseArchive(const char **, Lst, SymTable *);
/* Arch_Touch(node);
 *	Alter the modification time of the archive member described by node
 *	to the current time.  */
extern void Arch_Touch(GNode *);
/* Arch_TouchLib(node);
 *	Update the modification time of the library described by node.
 *	This is distinct from Arch_Touch, as it also updates the mtime
 *	of the library's table of contents.  */
extern void Arch_TouchLib(GNode *);
/* stamp = Arch_MTime(node);
 *	Find the modification time of a member of an archive *in the
 *	archive*, and returns it.
 *	The time is also stored in the member's GNode.  */
extern TIMESTAMP Arch_MTime(GNode *);
/* stamp = Arch_MemMTime(node);
 *	Find the modification time of a member of an archive and returns it.
 *	To use when the member only exists within the archive.  */
extern TIMESTAMP Arch_MemMTime(GNode *);
/* Arch_FindLib(node, path);
 *	Search for a library node along a path, and fills the gnode's path
 *	field to the actual complete path. If we don't find it, we set the
 *	library name to libname.a, assuming some other mechanism will take
 *	care of finding it.  The library name should be in -l<name> format.  */
extern void Arch_FindLib(GNode *, Lst);
/* bool = Arch_LibOODate(node);
 *	Decide whether a library node is out-of-date. */
extern bool Arch_LibOODate(GNode *);

/* bool = Arch_IsLib(node);
 *	Decide whether a node is a library.  */
extern bool Arch_IsLib(GNode *);

#endif
