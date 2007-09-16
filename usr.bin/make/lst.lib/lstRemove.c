/*	$OpenPackages$ */
/*	$OpenBSD: lstRemove.c,v 1.18 2007/09/16 09:46:14 espie Exp $	*/
/*	$NetBSD: lstRemove.c,v 1.5 1996/11/06 17:59:50 christos Exp $	*/

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
 * LstRemove.c --
 *	Remove an element from a list
 */

#include	"lstInt.h"
#include	<stdlib.h>


/*-
 *-----------------------------------------------------------------------
 * Lst_Remove --
 *	Remove the given node from the given list.
 *
 * Side Effects:
 *	The list's firstPtr will be set to NULL if ln is the last
 *	node on the list. firsPtr and lastPtr will be altered if ln is
 *	either the first or last node, respectively, on the list.
 *
 *-----------------------------------------------------------------------
 */
void
Lst_Remove(Lst l, LstNode ln)
{
	if (ln == NULL)
		return;

	/* unlink it from the list */
	if (ln->nextPtr != NULL)
		ln->nextPtr->prevPtr = ln->prevPtr;
	if (ln->prevPtr != NULL)
		ln->prevPtr->nextPtr = ln->nextPtr;

	/* if either the firstPtr or lastPtr of the list point to this node,
	 * adjust them accordingly */
	if (l->firstPtr == ln)
		l->firstPtr = ln->nextPtr;
	if (l->lastPtr == ln)
		l->lastPtr = ln->prevPtr;

	/* note that the datum is unmolested. The caller must free it as
	 * necessary and as expected.  */
	free(ln);
}

