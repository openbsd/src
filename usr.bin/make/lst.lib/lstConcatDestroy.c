/*	$OpenBSD: lstConcatDestroy.c,v 1.1 2000/06/17 14:44:26 espie Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)lstConcat.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: lstConcatDestroy.c,v 1.1 2000/06/17 14:44:26 espie Exp $";
#endif
#endif /* not lint */

/*-
 * listConcatDestroy.c --
 *	Function to concatentate two lists.
 */

#include    "lstInt.h"

/*-
 *-----------------------------------------------------------------------
 * Lst_ConcatDestroy --
 *	Concatenate two lists. The elements of the second list are
 *	destructively added to the first list.  If the elements should 
 *	be duplicated to avoid confusion with another list, the 
 *	Lst_Duplicate function should be called first.
 *
 * Results:
 *	SUCCESS if all went well. FAILURE otherwise.
 *
 * Side Effects:
 *	The second list is destroyed
 *-----------------------------------------------------------------------
 */
void
Lst_ConcatDestroy(l1, l2)
    Lst    	  	l1; 	/* The list to which l2 is to be appended */
    Lst    	  	l2; 	/* The list to append to l1 */
{
    LstNode  	ln;     	/* original LstNode */
    LstNode  	nln;    	/* new LstNode */
    LstNode  	last;   	/* the last element in the list. Keeps
				 * bookkeeping until the end */

    if (l2->firstPtr != NULL) {
	/*
	 * We set the nextPtr of the
	 * last element of list two to be NULL to make the loop easier and
	 * so we don't need an extra case should the first list turn
	 * out to be non-circular -- the final element will already point
	 * to NULL space and the first element will be untouched if it
	 * existed before and will also point to NULL space if it didn't.
	 */
	l2->lastPtr->nextPtr = NULL;
	/*
	 * So long as the second list isn't empty, we just link the
	 * first element of the second list to the last element of the
	 * first list. If the first list isn't empty, we then link the
	 * last element of the list to the first element of the second list
	 * The last element of the second list, if it exists, then becomes
	 * the last element of the first list.
	 */
	l2->firstPtr->prevPtr = l1->lastPtr;
	if (l1->lastPtr != NULL) {
	    l1->lastPtr->nextPtr = l2->firstPtr;
	} else {
	    l1->firstPtr = l2->firstPtr;
	}
	l1->lastPtr = l2->lastPtr;
    }
}

