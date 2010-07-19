/*	$OpenBSD: lstConcatDestroy.c,v 1.11 2010/07/19 19:46:44 espie Exp $	*/
/*	$NetBSD: lstConcat.c,v 1.6 1996/11/06 17:59:34 christos Exp $	*/

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
 * listConcat.c --
 *	Function to concatentate two lists.
 */

#include    "lstInt.h"
#include    <stddef.h>

/*-
 *-----------------------------------------------------------------------
 * Lst_ConcatDestroy --
 *	Concatenate two lists.	The elements of the second list are
 *	destructively added to the first list.	If the elements should
 *	be duplicated to avoid confusion with another list, the
 *	Lst_Duplicate function should be called first.
 *
 * Side Effects:
 *	The second list is destroyed.
 *-----------------------------------------------------------------------
 */
void
Lst_ConcatDestroy(Lst l1, Lst l2)
{
	if (l2->firstPtr != NULL) {
		/*
		 * So long as the second list isn't empty, we just link the
		 * first element of the second list to the last element of the
		 * first list. If the first list isn't empty, we then link the
		 * last element of the list to the first element of the second
		 * list The last element of the second list, if it exists, then
		 * becomes the last element of the first list.
		 */
		l2->firstPtr->prevPtr = l1->lastPtr;
		if (l1->lastPtr != NULL)
			l1->lastPtr->nextPtr = l2->firstPtr;
		else
			l1->firstPtr = l2->firstPtr;
		l1->lastPtr = l2->lastPtr;
	}
}

