#ifndef _LST_H_
#define _LST_H_

/*	$OpenBSD: lst.h,v 1.29 2010/07/19 19:46:44 espie Exp $ */
/*	$NetBSD: lst.h,v 1.7 1996/11/06 17:59:12 christos Exp $ */

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
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
 *
 *	from: @(#)lst.h 8.1 (Berkeley) 6/6/93
 */

/*-
 * lst.h --
 *	Header for using the list library
 */

/* These data structures are PRIVATE !!!
 * Here for efficiency, so that some functions can be recoded as inlines,
 * and so that lst headers don't need dynamic allocation most of the time.  */
struct ListNode_ {
	struct ListNode_    *prevPtr;	/* previous element in list */
	struct ListNode_    *nextPtr;	/* next in list */
	void		    *datum;	/* datum associated with this element */
};

#ifndef LIST_TYPE
#include "lst_t.h"
#endif

typedef void (*SimpleProc)(void *);
typedef int (*FindProc)(void *, void *);
typedef int (*ForEachNodeWhileProc)(LstNode, void *);
typedef int (*FindProcConst)(void *, const void *);
typedef void (*ForEachProc)(void *, void *);
typedef void *(*DuplicateProc)(void *);

/*
 * NOFREE can be used as the freeProc to Lst_Destroy when the elements are
 *	not to be freed.
 * NOCOPY performs similarly when given as the copyProc to Lst_Duplicate.
 */
#define NOFREE		((SimpleProc) 0)
#define NOCOPY		((DuplicateProc) 0)

/*
 * Creation/destruction functions
 */
/* Create a new list */
#define Lst_Init(l)	(l)->firstPtr = (l)->lastPtr = NULL
/* Static lists are already okay */
#define Static_Lst_Init(l)

/* Duplicate an existing list */
extern Lst		Lst_Clone(Lst, Lst, DuplicateProc);
/* Destroy an old one */
extern void		Lst_Destroy(LIST *, SimpleProc);
/* True if list is empty */
#define 	Lst_IsEmpty(l)	((l)->firstPtr == NULL)

/*
 * Functions to modify a list
 */
/* Insert an element before another */
extern void		Lst_Insert(Lst, LstNode, void *);
extern void		Lst_AtFront(Lst, void *);
/* Insert an element after another */
extern void		Lst_Append(Lst, LstNode, void *);
extern void		Lst_AtEnd(Lst, void *);
/* Remove an element */
extern void		Lst_Remove(Lst, LstNode);
/* Replace a node with a new value */
extern void		Lst_Replace(LstNode, void *);
/* Concatenate two lists, destructive.	*/
extern void		Lst_ConcatDestroy(Lst, Lst);
/* Concatenate two lists, non-destructive.  */
extern void		Lst_Concat(Lst, Lst);
/* requeue element already in list at front of list */
extern void		Lst_Requeue(Lst, LstNode);

/*
 * Node-specific functions
 */
/* Return first element in list */
/* Return last element in list */
/* Return successor to given element */
extern LstNode		Lst_Succ(LstNode);

/*
 * Functions for entire lists
 */
/* Find an element starting from somewhere */
extern LstNode		Lst_FindFrom(LstNode, FindProc, void *);
/*
 * See if the given datum is on the list. Returns the LstNode containing
 * the datum
 */
extern LstNode		Lst_Member(Lst, void *);
/* Apply a function to elements of a lst starting from a certain point.  */
extern void		Lst_ForEachFrom(LstNode, ForEachProc, void *);
extern void		Lst_Every(Lst, SimpleProc);

extern void		Lst_ForEachNodeWhile(Lst, ForEachNodeWhileProc, void *);

extern bool		Lst_AddNew(Lst, void *);
/*
 * for using the list as a queue
 */
/* Place an element at tail of queue */
#define Lst_EnQueue	Lst_AtEnd
#define Lst_QueueNew	Lst_AddNew

/*
 * for using the list as a stack
 */
#define Lst_Push	Lst_AtFront
#define Lst_Pop		Lst_DeQueue

/* Remove an element from head of queue */
extern void *	Lst_DeQueue(Lst);

#define Lst_Datum(ln)	((ln)->datum)
#define Lst_First(l)	((l)->firstPtr)
#define Lst_Last(l)	((l)->lastPtr)
#define Lst_ForEach(l, proc, d) Lst_ForEachFrom(Lst_First(l), proc, d)
#define Lst_Find(l, cProc, d)	Lst_FindFrom(Lst_First(l), cProc, d)
#define Lst_Adv(ln)	((ln)->nextPtr)
#define Lst_Rev(ln)	((ln)->prevPtr)


/* Inlines are preferable to macros here because of the type checking. */
#ifdef HAS_INLINES
static INLINE LstNode
Lst_FindConst(Lst l, FindProcConst cProc, const void *d)
{
	return Lst_FindFrom(Lst_First(l), (FindProc)cProc, (void *)d);
}

static INLINE LstNode
Lst_FindFromConst(LstNode ln, FindProcConst cProc, const void *d)
{
	return Lst_FindFrom(ln, (FindProc)cProc, (void *)d);
}
#else
#define Lst_FindConst(l, cProc, d) \
	Lst_FindFrom(Lst_First(l), (FindProc)cProc, (void *)d)
#define Lst_FindFromConst(ln, cProc, d) \
	Lst_FindFrom(ln, (FindProc)cProc, (void *)d)
#endif

#endif /* _LST_H_ */
