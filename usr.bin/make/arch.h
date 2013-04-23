#ifndef ARCH_H
#define ARCH_H
/*	$OpenBSD: arch.h,v 1.8 2013/04/23 14:32:53 espie Exp $ */

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

#include <sys/time.h>

/* Initialization and cleanup */
extern void Arch_Init(void);

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
/* stamp = Arch_MTime(node);
 *	Find the modification time of a member of an archive *in the
 *	archive*, and returns it.
 *	The time is also stored in the member's GNode.  */
extern struct timespec Arch_MTime(GNode *);
/* stamp = Arch_MemMTime(node);
 *	Find the modification time of a member of an archive and returns it.
 *	To use when the member only exists within the archive.  */
extern struct timespec Arch_MemMTime(GNode *);

#endif
