/*	$OpenBSD: lstConcat.c,v 1.10 2000/06/17 14:43:38 espie Exp $	*/
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
static char rcsid[] = "$OpenBSD: lstConcat.c,v 1.10 2000/06/17 14:43:38 espie Exp $";
#endif
#endif /* not lint */

/*-
 * listConcat.c --
 *	Function to concatentate two lists.
 */

#include    "lstInt.h"

/*-
 *-----------------------------------------------------------------------
 * Lst_Concat --
 *	Concatenate two lists. New elements are created to hold the data
 *	elements, but the elements themselves are not copied.
 *	If the elements should be duplicated to avoid confusion with another
 *	list, the Lst_Duplicate function should be called first.
 *
 * Results:
 *	SUCCESS if all went well. FAILURE otherwise.
 *
 * Side Effects:
 *	New elements are created and appended to the first list.
 *-----------------------------------------------------------------------
 */
void
Lst_Concat(l1, l2)
    Lst    	  	l1; 	/* The list to which l2 is to be appended */
    Lst    	  	l2; 	/* The list to append to l1 */
{
    LstNode  	ln;     	/* original LstNode */
    LstNode  	nln;    	/* new LstNode */
    LstNode  	last;   	/* the last element in the list. Keeps
				 * bookkeeping until the end */

    if (l2->firstPtr != NULL) {
	/*
	 * We set the nextPtr of the last element of list 2 to be NULL to make
	 * the loop less difficult. The loop simply goes through the entire
	 * second list creating new LstNodes and filling in the nextPtr, and
	 * prevPtr to fit into l1 and its datum field from the
	 * datum field of the corresponding element in l2. The 'last' node
	 * follows the last of the new nodes along until the entire l2 has
	 * been appended. Only then does the bookkeeping catch up with the
	 * changes. During the first iteration of the loop, if 'last' is NULL,
	 * the first list must have been empty so the newly-created node is
	 * made the first node of the list.
	 */
	l2->lastPtr->nextPtr = NULL;
	for (last = l1->lastPtr, ln = l2->firstPtr; ln != NULL;
	     ln = ln->nextPtr) {
	    PAlloc(nln, LstNode);
	    nln->datum = ln->datum;
	    if (last != NULL)
		last->nextPtr = nln;
	    else
		l1->firstPtr = nln;
	    nln->prevPtr = last;
	    nln->flags = nln->useCount = 0;
	    last = nln;
	}

	/*
	 * Finish bookkeeping. The last new element becomes the last element
	 * of list one.
	 */
	l1->lastPtr = last;

	last->nextPtr = NULL;

	l2->lastPtr->nextPtr = l2->firstPtr;
    }
}

