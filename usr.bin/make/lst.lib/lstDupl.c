/*	$OpenBSD: lstDupl.c,v 1.22 2010/07/19 19:46:44 espie Exp $	*/
/*	$NetBSD: lstDupl.c,v 1.6 1996/11/06 17:59:37 christos Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * listDupl.c --
 *	Duplicate a list. This includes duplicating the individual
 *	elements.
 */

#include    "lstInt.h"
#include <stddef.h>

/*-
 *-----------------------------------------------------------------------
 * Lst_Clone --
 *	Duplicate an entire list. If a function to copy a void * is
 *	given, the individual client elements will be duplicated as well.
 *
 * Results:
 *	returns the new list.
 *
 * Side Effects:
 *	The new list is created.
 *-----------------------------------------------------------------------
 */
Lst
Lst_Clone(Lst nl, Lst l, DuplicateProc copyProc)
{
	LstNode ln;

	Lst_Init(nl);

	for (ln = l->firstPtr; ln != NULL; ln = ln->nextPtr) {
		if (copyProc != NOCOPY)
			Lst_AtEnd(nl, (*copyProc)(ln->datum));
		else
			Lst_AtEnd(nl, ln->datum);
	}
	return nl;
}

