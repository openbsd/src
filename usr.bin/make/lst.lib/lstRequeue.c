/*	$OpenBSD: lstRequeue.c,v 1.1 2008/01/12 13:05:57 espie Exp $	*/
/*
 * Copyright (c) 2008 Marc Espie.
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
#include	"lstInt.h"
#include	<stdlib.h>


/* Lst_Requeue(l, ln): takes node ln from the list and requeue it at front.
 */
void
Lst_Requeue(Lst l, LstNode ln)
{

	/* already at front */
	if (l->firstPtr == ln)
		return;
	/* unlink element */
	if (ln->nextPtr != NULL)
		ln->nextPtr->prevPtr = ln->prevPtr;
	if (ln->prevPtr != NULL)
		ln->prevPtr->nextPtr = ln->nextPtr;

	if (l->lastPtr == ln)
		l->lastPtr = ln->prevPtr;

	/* relink at front */
	ln->nextPtr = l->firstPtr;
	ln->prevPtr = NULL;
	l->firstPtr->prevPtr = ln;
	l->firstPtr = ln;
}

