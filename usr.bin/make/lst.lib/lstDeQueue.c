/*	$OpenBSD: lstDeQueue.c,v 1.19 2010/07/19 19:46:44 espie Exp $	*/
/*	$NetBSD: lstDeQueue.c,v 1.5 1996/11/06 17:59:36 christos Exp $	*/

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
 * LstDeQueue.c --
 *	Remove the node and return its datum from the head of the list
 */

#include	"lstInt.h"
#include 	<stdlib.h>

/*-
 *-----------------------------------------------------------------------
 * Lst_DeQueue --
 *	Remove and return the datum at the head of the given list.
 *
 * Results:
 *	The datum in the node at the head or NULL if the list is empty.
 *
 * Side Effects:
 *	The head node is removed from the list.
 *-----------------------------------------------------------------------
 */
void *
Lst_DeQueue(Lst l)
{
	void *rd;
	LstNode tln;

	tln = l->firstPtr;
	if (tln == NULL)
		return NULL;

	rd = tln->datum;
	l->firstPtr = tln->nextPtr;
	if (l->firstPtr)
		l->firstPtr->prevPtr = NULL;
	else
		l->lastPtr = NULL;
	free(tln);
	return rd;
}

