/*	$OpenBSD: stree.c,v 1.11 2009/05/09 12:02:17 chl Exp $	*/

/*
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * stree.c -- SUP Tree Routines
 *
 **********************************************************************
 * HISTORY
 * Revision 1.4  92/08/11  12:06:32  mrt
 * 	Added copyright. Delinted
 * 	[92/08/10            mrt]
 * 
 * 
 * Revision 1.3  89/08/15  15:30:57  bww
 * 	Changed code in Tlookup to Tsearch for each subpart of path.
 * 	Added indent formatting code to Tprint.
 * 	From "[89/06/24            gm0w]" at CMU.
 * 	[89/08/15            bww]
 * 
 * 20-May-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code to please lint.
 *
 * 29-Dec-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code to initialize new fields.  Added Tfree routine.
 *
 * 27-Sep-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Created.
 *
 **********************************************************************
 */

#include <libc.h>
#include <c.h>
#include <sys/param.h>
#include "supcdefs.h"
#include "supextern.h"

#define Static		static		/* comment for debugging */

Static TREE *Tmake(char *);
Static TREE *Trotll(TREE *, TREE *);
Static TREE *Trotlh(TREE *, TREE *);
Static TREE *Trothl(TREE *, TREE *);
Static TREE *Trothh(TREE *, TREE *);
Static void Tbalance(TREE **);
Static TREE *Tinsertavl(TREE **, char *, int, int *);
Static int Tsubprocess(TREE *, int, int (*f )(TREE *, void *), void *);
#ifdef DEBUG
Static int Tprintone(TREE *, void *);
#endif


/*************************************************************
 ***    T R E E   P R O C E S S I N G   R O U T I N E S    ***
 *************************************************************/

void
Tfree(t)
	TREE **t;
{
	if (*t == NULL)
		return;
	Tfree(&((*t)->Tlink));
	Tfree(&((*t)->Texec));
	Tfree(&((*t)->Tlo));
	Tfree(&((*t)->Thi));
	if ((*t)->Tname)
		free((*t)->Tname);
	if ((*t)->Tuser)
		free((*t)->Tuser);
	if ((*t)->Tgroup)
		free((*t)->Tgroup);
	free(*t);
	*t = NULL;
}

Static TREE *
Tmake(p)
	char *p;
{
	TREE *t;

	t = (TREE *) malloc(sizeof(TREE));
	if (t != NULL) {
		t->Tname = (p == NULL) ? NULL : strdup(p);
		t->Tflags = 0;
		t->Tuid = 0;
		t->Tgid = 0;
		t->Tuser = NULL;
		t->Tgroup = NULL;
		t->Tmode = 0;
		t->Tctime = 0;
		t->Tmtime = 0;
		t->Tlink = NULL;
		t->Texec = NULL;
		t->Tbf = 0;
		t->Tlo = NULL;
		t->Thi = NULL;
	}
	return (t);
}

Static TREE *
Trotll(tp, tl)
	TREE *tp, *tl;
{

	tp->Tlo = tl->Thi;
	tl->Thi = tp;
	tp->Tbf = tl->Tbf = 0;
	return(tl);
}

Static TREE *
Trotlh (tp,tl)
	TREE *tp, *tl;
{
	TREE *th;

	th = tl->Thi;
	tp->Tlo = th->Thi;
	tl->Thi = th->Tlo;
	th->Thi = tp;
	th->Tlo = tl;
	tp->Tbf = tl->Tbf = 0;
	if (th->Tbf == 1)
		tp->Tbf = -1;
	else if (th->Tbf == -1)
		tl->Tbf = 1;
	th->Tbf = 0;
	return (th);
}

Static TREE *
Trothl(tp, th)
	TREE *tp, *th;
{
	TREE *tl;

	tl = th->Tlo;
	tp->Thi = tl->Tlo;
	th->Tlo = tl->Thi;
	tl->Tlo = tp;
	tl->Thi = th;
	tp->Tbf = th->Tbf = 0;
	if (tl->Tbf == -1)
		tp->Tbf = 1;
	else if (tl->Tbf == 1)
		th->Tbf = -1;
	tl->Tbf = 0;
	return (tl);
}

Static TREE *
Trothh(tp, th)
	TREE *tp, *th;
{

	tp->Thi = th->Tlo;
	th->Tlo = tp;
	tp->Tbf = th->Tbf = 0;
	return (th);
}

Static void
Tbalance(t)
	TREE **t;
{

	if ((*t)->Tbf < 2 && (*t)->Tbf > -2)
		return;
	if ((*t)->Tbf > 0) {
		if ((*t)->Tlo->Tbf > 0)
			*t = Trotll(*t, (*t)->Tlo);
		else
			*t = Trotlh(*t, (*t)->Tlo);
	} else {
		if ((*t)->Thi->Tbf > 0)
			*t = Trothl(*t, (*t)->Thi);
		else
			*t = Trothh(*t, (*t)->Thi);
	}
}

Static TREE *
Tinsertavl(t, p, find, dh)
	TREE **t;
	char *p;
	int find;
	int *dh;
{
	TREE *newt;
	int cmp;
	int deltah;

	if (*t == NULL) {
		if ((*t = Tmake(p)) != NULL)
			*dh = 1;
		return (*t);
	}
	if ((cmp = strcmp(p, (*t)->Tname)) == 0) {
		if (!find)
			return (NULL);	/* node already exists */
		*dh = 0;
		return (*t);
	} else if (cmp < 0) {
	    if ((newt = Tinsertavl(&((*t)->Tlo), p, find, &deltah)) == NULL)
		    return (NULL);
	    (*t)->Tbf += deltah;
	} else {
		if ((newt = Tinsertavl(&((*t)->Thi), p, find, &deltah)) == NULL)
			return (NULL);
		(*t)->Tbf -= deltah;
	}
	Tbalance(t);
	if ((*t)->Tbf == 0)
		deltah = 0;
	*dh = deltah;
	return (newt);
}

TREE *
Tinsert(t, p, find)
	TREE **t;
	char *p;
	int find;
{
	int deltah;

	if (p != NULL && p[0] == '.' && p[1] == '/') {
		p += 2;
		while (*p == '/')
			p++;
		if (*p == 0)
			p = ".";
	}
	return (Tinsertavl(t, p, find, &deltah));
}

TREE *
Tsearch(t, p)
	TREE *t;
	char *p;
{
	TREE *x;
	int cmp;

	x = t;
	while (x) {
		cmp = strcmp(p, x->Tname);
		if (cmp == 0)
			return (x);
		if (cmp < 0)
			x = x->Tlo;
		else
			x = x->Thi;
	}
	return (NULL);
}

TREE *
Tlookup (t, p)
	TREE *t;
	char *p;
{
	TREE *x;
	char buf[MAXPATHLEN];

	if (p == NULL)
		return (NULL);
	if (p[0] == '.' && p[1] == '/') {
		p += 2;
		while (*p == '/')
			p++;
		if (*p == 0)
			p = ".";
	}
	if ((x = Tsearch(t, p)) != NULL)
		return (x);
	if (*p != '/' && (x = Tsearch(t, ".")) != NULL)
		return (x);
	(void) strlcpy(buf, p, sizeof(buf));
	while ((p = strrchr(buf, '/')) != NULL) {
		while (p >= buf && *(p-1) == '/')
			p--;
		if (p == buf)
			*(p+1) = '\0';
		else
			*p = '\0';
		if ((x = Tsearch(t, buf)) != NULL)
			return (x);
		if (p == buf)
			break;
	}
	return (NULL);
}

Static int process_level;

Static int
Tsubprocess (t, reverse, f, argp)
	TREE *t;
	int reverse;
	int (*f)(TREE *, void *);
	void *argp;
{
	int x = SCMOK;

	process_level++;
	if (reverse ? t->Thi : t->Tlo)
		x = Tsubprocess(reverse ? t->Thi : t->Tlo, reverse, f, argp);
	if (x == SCMOK) {
		x = (*f)(t, argp);
		if (x == SCMOK) {
			if (reverse ? t->Tlo : t->Thi)
				x = Tsubprocess (reverse ? t->Tlo : t->Thi,
						 reverse, f, argp);
		}
	}
	process_level--;
	return (x);
}

/* VARARGS2 */
int
Trprocess(t, f, args)
	TREE *t;
	int (*f)(TREE *, void *);
	void *args;
{
	if (t == NULL)
		return (SCMOK);
	process_level = 0;
	return (Tsubprocess(t, TRUE, f, args));
}

/* VARARGS2 */
int
Tprocess(t, f, args)
	TREE *t;
	int (*f)(TREE *, void *);
	void *args;
{
	if (t == NULL)
		return (SCMOK);
	process_level = 0;
	return (Tsubprocess(t, FALSE, f, args));
}

#if DEBUG
Static int
Tprintone(t, v)
	TREE *t;
	void *v;
{
	int i;
	for (i = 0; i < (process_level*2); i++)
		(void) putchar(' ');
	printf("Node at %p name '%s' flags %o hi %p lo %p\n",
	    t, t->Tname, t->Tflags, t->Thi, t->Tlo);
	return (SCMOK);
}

void
Tprint(t, p)		/* print tree -- for debugging */
	TREE *t;
	char *p;
{

	printf("%s\n",p);
	(void) Tprocess(t,Tprintone, NULL);
	printf("End of tree\n");
	(void) fflush(stdout);
}
#endif
