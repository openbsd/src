/*	$NetBSD: support.c,v 1.1 1995/02/13 23:07:13 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
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
 * Some C support functions that aren't (yet) in libkern or assembly.
 */
#include <sys/param.h>
#include <sys/errno.h>

void
blkclr(void *b, size_t len)
{
	bzero(b, len);
}

void
ovbcopy(void *a, void *b, size_t len)
{
	if (b < a) {
		while (len--)
			*((char *)b)++ = *((char *)a)++;
	} else {
		((char *)a) += len;
		((char *)b) += len;
		while (len--)
			*--((char *)b) = *--((char *)a);
	}
}

struct qelem {
	struct qelem *q_forw;
	struct qelem *q_back;
};

void
_insque(entry, pred)
	void *entry;
	void *pred;
{
	struct qelem *e = (struct qelem *) entry;
	struct qelem *p = (struct qelem *) pred;

	e->q_forw = p->q_forw;
	e->q_back = p;
	p->q_forw->q_back = e;
	p->q_forw = e;
}

void
_remque(element)
	void *element;
{
	struct qelem *e = (struct qelem *) element;
	e->q_forw->q_back = e->q_back;
	e->q_back->q_forw = e->q_forw;
}
