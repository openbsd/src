/*	$OpenBSD: shared_intr.c,v 1.6 1999/02/08 00:05:09 millert Exp $	*/
/*	$NetBSD: shared_intr.c,v 1.1 1996/11/17 02:03:08 cgd Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Common shared-interrupt-line functionality.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <machine/intr.h>

extern int cold;

static const char *intr_typename __P((int));

static const char *
intr_typename(type)
	int type;
{

	switch (type) {
	case IST_UNUSABLE:
		return ("disabled");
	case IST_NONE:
		return ("none");
	case IST_PULSE:
		return ("pulsed");
	case IST_EDGE:
		return ("edge-triggered");
	case IST_LEVEL:
		return ("level-triggered");
	}
	panic("intr_typename: unknown type %d", type);
}

struct alpha_shared_intr *
alpha_shared_intr_alloc(n)
	unsigned int n;
{
	struct alpha_shared_intr *intr;
	unsigned int i;

	intr = malloc(n * sizeof (struct alpha_shared_intr), M_DEVBUF,
	    cold ? M_NOWAIT : M_WAITOK);
	if (intr == NULL)
		panic("alpha_shared_intr_alloc: couldn't malloc intr");

	for (i = 0; i < n; i++) {
		TAILQ_INIT(&intr[i].intr_q);
		intr[i].intr_sharetype = IST_NONE;
		intr[i].intr_dfltsharetype = IST_NONE;
		intr[i].intr_nstrays = 0;
		intr[i].intr_maxstrays = 5;
	}

	return (intr);
}

int
alpha_shared_intr_dispatch(intr, num)
	struct alpha_shared_intr *intr;
	unsigned int num;
{
	struct alpha_shared_intrhand *ih;
	int rv, handled;

	ih = intr[num].intr_q.tqh_first;
	handled = 0;
	while (ih != NULL) {

		/*
		 * The handler returns one of three values:
		 *   0:	This interrupt wasn't for me.
		 *   1: This interrupt was for me.
		 *  -1: This interrupt might have been for me, but I can't say
		 *      for sure.
		 */
		rv = (*ih->ih_fn)(ih->ih_arg);

		handled = handled || (rv != 0);
		ih = ih->ih_q.tqe_next;
	}

	return (handled);
}

/*
 * Just check to see if an IRQ is available/can be shared.
 * 0 = interrupt not available
 * 1 = interrupt shareable
 * 2 = interrupt all to ourself
 */
int
alpha_shared_intr_check(intr, num, type)
	struct alpha_shared_intr *intr;
	unsigned int num;
	int type;
{

	switch (intr[num].intr_sharetype) {
	case IST_UNUSABLE:
		return (0);
		break;
	case IST_NONE:
		return (2);
		break;
	case IST_LEVEL:
		if (type == intr[num].intr_sharetype)
			break;
	case IST_EDGE:
	case IST_PULSE:
		if ((type != IST_NONE) && (intr[num].intr_q.tqh_first != NULL))
			return (0);
	}

	return (1);
}

void *
alpha_shared_intr_establish(intr, num, type, level, fn, arg, basename)
	struct alpha_shared_intr *intr;
	unsigned int num;
	int type, level;
	int (*fn) __P((void *));
	void *arg;
	const char *basename;
{
	struct alpha_shared_intrhand *ih;

	if (intr[num].intr_sharetype == IST_UNUSABLE) {
		printf("alpha_shared_intr_establish: %s %d: unusable\n",
		    basename, num);
		return NULL;
	}

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("alpha_shared_intr_establish: can't malloc intrhand");

#ifdef DIAGNOSTIC
	if (type == IST_NONE)
		panic("alpha_shared_intr_establish: bogus type");
#endif

	switch (intr[num].intr_sharetype) {
	case IST_EDGE:
	case IST_LEVEL:
		if (type == intr[num].intr_sharetype)
			break;
	case IST_PULSE:
		if (type != IST_NONE) {
			if (intr[num].intr_q.tqh_first == NULL) {
				printf("alpha_shared_intr_establish: %s %d: warning: using %s on %s\n",
				    basename, num, intr_typename(type),
				    intr_typename(intr[num].intr_sharetype));
				type = intr[num].intr_sharetype;
			} else {
				panic("alpha_shared_intr_establish: %s %d: can't share %s with %s",
				    basename, num, intr_typename(type),
				    intr_typename(intr[num].intr_sharetype));
			}
		}
		break;

	case IST_NONE:
		/* not currently used; safe */
		break;
	}

	ih->ih_fn = fn;
	ih->ih_arg = arg;
	ih->ih_level = level;

	intr[num].intr_sharetype = type;
	TAILQ_INSERT_TAIL(&intr[num].intr_q, ih, ih_q);

	return (ih);
}

int
alpha_shared_intr_get_sharetype(intr, num)
	struct alpha_shared_intr *intr;
	unsigned int num;
{

	return (intr[num].intr_sharetype);
}

int
alpha_shared_intr_isactive(intr, num)
	struct alpha_shared_intr *intr;
	unsigned int num;
{

	return (intr[num].intr_q.tqh_first != NULL);
}

void
alpha_shared_intr_set_dfltsharetype(intr, num, newdfltsharetype)
	struct alpha_shared_intr *intr;
	unsigned int num;
	int newdfltsharetype;
{

#ifdef DIAGNOSTIC
	if (alpha_shared_intr_isactive(intr, num))
		panic("alpha_shared_intr_set_dfltsharetype on active intr");
#endif

	intr[num].intr_dfltsharetype = newdfltsharetype;
	intr[num].intr_sharetype = intr[num].intr_dfltsharetype;
}

void
alpha_shared_intr_set_maxstrays(intr, num, newmaxstrays)
	struct alpha_shared_intr *intr;
	unsigned int num;
	int newmaxstrays;
{

#ifdef DIAGNOSTIC
	if (alpha_shared_intr_isactive(intr, num))
		panic("alpha_shared_intr_set_maxstrays on active intr");
#endif

	intr[num].intr_maxstrays = newmaxstrays;
	intr[num].intr_nstrays = 0;
}

void
alpha_shared_intr_stray(intr, num, basename)
	struct alpha_shared_intr *intr;
	unsigned int num;
	const char *basename;
{

	intr[num].intr_nstrays++;
	if (intr[num].intr_nstrays <= intr[num].intr_maxstrays)
		log(LOG_ERR, "stray %s %d%s\n", basename, num,
		    intr[num].intr_nstrays >= intr[num].intr_maxstrays ?
		      "; stopped logging" : "");
}
