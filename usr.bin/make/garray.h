#ifndef GARRAY_H
#define GARRAY_H

/* $OpenPackages$ */
/* $OpenBSD: garray.h,v 1.3 2007/09/17 10:12:35 espie Exp $ */
/* Growable array implementation */

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

struct growableArray {
	GNode 	     **a;	/* Only used for gnodes right now */
	unsigned int size;	/* Total max size */
	unsigned int n;		/* Current number of members */
};

#define AppendList2Array(l1, l2)				\
do {								\
    	LstNode ln;						\
	for (ln = Lst_First((l1)); ln != NULL; ln = Lst_Adv(ln))\
		Array_AtEnd((l2), Lst_Datum(ln));		\
} while (0)

#ifdef STATS_GROW
#define MAY_INCREASE_STATS	STAT_GROWARRAY++
#else
#define MAY_INCREASE_STATS
#endif

#define Array_AtEnd(l, gn) 				\
do { 							\
	if ((l)->n >= (l)->size) { 			\
		(l)->size *= 2; 			\
		(l)->a = erealloc((l)->a, 		\
		    sizeof(struct GNode *) * (l)->size);\
		MAY_INCREASE_STATS;			\
	} 						\
	(l)->a[(l)->n++] = (gn); 			\
} while (0)

#define Array_Find(l, func, v)			\
do {						\
	unsigned int i;				\
	for (i = 0; i < (l)->n; i++)		\
		if ((func)((l)->a[i], (v)) == 0)\
		    break;			\
} while (0)

#define Array_FindP(l, func, v)				\
do {							\
	unsigned int i;					\
	for (i = 0; i < (l)->n; i++)			\
		if ((func)(&((l)->a[i]), (v)) == 0)	\
		    break;				\
} while (0)

#define Array_ForEach(l, func, v)		\
do {						\
	unsigned int i;				\
	for (i = 0; i < (l)->n; i++)		\
		(func)((l)->a[i], (v));		\
} while (0)

#define Array_Every(l, func)			\
do {						\
	unsigned int i;				\
	for (i = 0; i < (l)->n; i++)		\
		(func)((l)->a[i]);		\
} while (0)

#define Array_Init(l, sz)				\
do {							\
	(l)->size = (sz);				\
	(l)->n = 0;					\
	(l)->a = emalloc(sizeof(GNode *) * (l)->size);	\
} while (0)

#define Array_Reset(l)		\
do {				\
	(l)->n = 0;		\
} while (0)

#define Array_IsEmpty(l)	((l)->n == 0)

#endif
