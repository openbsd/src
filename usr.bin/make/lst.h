/*	$OpenBSD: lst.h,v 1.18 2000/09/14 13:32:07 espie Exp $	*/
/*	$NetBSD: lst.h,v 1.7 1996/11/06 17:59:12 christos Exp $	*/

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
 *
 *	from: @(#)lst.h	8.1 (Berkeley) 6/6/93
 */

/*-
 * lst.h --
 *	Header for using the list library
 */
#ifndef _LST_H_
#define _LST_H_

#include	"sprite.h"
#include	<sys/param.h>
#ifdef __STDC__
#include	<stdlib.h>
#endif

/* These data structures are PRIVATE !!!
 * Here for efficiency, so that some functions can be recoded as inlines,
 * and so that lst headers don't need dynamic allocation most of the time.  */
typedef struct ListNode_ {
	struct ListNode_    *prevPtr;   /* previous element in list */
	struct ListNode_    *nextPtr;   /* next in list */
	short	    	    useCount:8, /* Count of functions using the node.
			    	         * node may not be deleted until count
				         * goes to 0 */
 	    	    	    flags:8;    /* Node status flags */
	void		    *datum;	/* datum associated with this element */
} *LstNode;

typedef enum {
    Head, Middle, Tail, Unknown
} Where;

typedef struct	{
	LstNode  	firstPtr; /* first node in list */
	LstNode  	lastPtr;  /* last node in list */
/*
 * fields for sequential access
 */
	Where	  	atEnd;	  /* Where in the list the last access was */
	Boolean	  	isOpen;	  /* true if list has been Lst_Open'ed */
	LstNode  	curPtr;	  /* current node, if open. NULL if
				   * *just* opened */
	LstNode  	prevPtr;  /* Previous node, if open. Used by
				   * Lst_Remove */
} LIST;

typedef LIST *Lst;
/*
 * basic typedef. This is what the Lst_ functions handle
 */

typedef int (*FindProc) __P((void *, void *));
typedef void (*SimpleProc) __P((void *));
typedef void (*ForEachProc) __P((void *, void *));
typedef void * (*DuplicateProc) __P((void *));

/*
 * NOFREE can be used as the freeProc to Lst_Destroy when the elements are
 *	not to be freed.
 * NOCOPY performs similarly when given as the copyProc to Lst_Clone.
 */
#define NOFREE		((SimpleProc)0)
#define NOCOPY		((DuplicateProc)0)

/*
 * Constructors/destructors
 */
/* Create a new list */
extern void		Lst_Init __P((Lst));
/* Destroy an old one */
extern void		Lst_Destroy __P((Lst, SimpleProc));

/* Duplicate an existing list */
extern Lst		Lst_Clone __P((Lst, Lst, DuplicateProc));
/* True if list is empty */
extern Boolean		Lst_IsEmpty __P((Lst));

/*
 * List modifications
 */
/* Insert an element before another */
extern void		Lst_Insert __P((Lst, LstNode, void *));
/* Insert an element after another */
extern void		Lst_Append __P((Lst, LstNode, void *));
/* Place an element at the front of a lst. */
extern void		Lst_AtFront __P((Lst, void *));
/* Place an element at the end of a lst. */
extern void		Lst_AtEnd __P((Lst, void *));
/* Remove an element */
extern void		Lst_Remove __P((Lst, LstNode));
/* Replace a node with a new value */
extern void		Lst_Replace __P((LstNode, void *));
/* Concatenate two lists, destructive.  */
extern void		Lst_ConcatDestroy __P((Lst, Lst));
/* Concatenate two lists, non destructive */
extern void		Lst_Concat __P((Lst, Lst));

/*
 * Node handling
 */
/* Return first element in list */
#define	Lst_First(l)	((l)->firstPtr)
/* Return last element in list */
#define Lst_Last(l)	((l)->lastPtr)
/* Return successor to given element */
extern LstNode		Lst_Succ __P((LstNode));
/* Return successor to existing element */
#define Lst_Adv(ln)	((ln)->nextPtr)
/* Get datum from LstNode */
#define Lst_Datum(ln)	((ln)->datum)

/*
 * Apply to entire lists
 */

/* Find an element in a list */
#define Lst_Find(l, cProc, d)	Lst_FindFrom(Lst_First(l), cProc, d)

/* Find an element starting from somewhere */
extern LstNode		Lst_FindFrom __P((LstNode, FindProc, void *));

/* Apply a function to all elements of a lst */
#define Lst_ForEach(l, proc, d)	Lst_ForEachFrom(Lst_First(l), proc, d)
/* Apply a function to all elements of a lst starting from a certain point.  */
extern void		Lst_ForEachFrom __P((LstNode, ForEachProc, void *));
extern void		Lst_Every __P((Lst, SimpleProc));


/* Find datum in a list. Returns the LstNode containing the datum */
extern LstNode		Lst_Member __P((Lst, void *));

/*
 * Visitor-like pattern.  Except the visitor is kept in the list.
 * Error-prone and wasteful (used by only a few lists), to be killed.
 */
/* Open the list */
extern void		Lst_Open __P((Lst));
/* Next element please */
extern LstNode		Lst_Next __P((Lst));
/* Done yet? */
extern Boolean		Lst_IsAtEnd __P((Lst));
/* Finish table access */
extern void		Lst_Close __P((Lst));

/*
 * Queue manipulators
 */
/* Place an element at tail of queue */
extern void		Lst_EnQueue __P((Lst, void *));
/* Remove an element from head of queue */
extern void *	Lst_DeQueue __P((Lst));

#endif /* _LST_H_ */
