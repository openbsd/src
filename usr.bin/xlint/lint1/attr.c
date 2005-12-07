/*	$OpenBSD: attr.c,v 1.2 2005/12/07 02:11:26 cloder Exp $	*/

/*
 * Copyright (c) 2005 Chad Loder
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: attr.c,v 1.2 2005/12/07 02:11:26 cloder Exp $";
#endif

#include "lint1.h"

attr_t
getattr(const char *attr)
{
	if (attr == NULL)
		return AT_UNKNOWN;

	if (!strcmp(attr, "__noreturn__")) {
		return AT_NORETURN;
	}
	else if (!strcmp(attr, "noreturn"))
		return AT_NORETURN;

	return AT_UNKNOWN;
}

attr_t
getqualattr(tqual_t q)
{
	if (q == VOLATILE)
		return AT_VOLATILE;

	return AT_UNKNOWN;
}

attrnode_t*
newattrnode(attr_t a)
{
	attrnode_t *an = xcalloc(1, sizeof(attrnode_t));
	an->an_attr = a;
	return an;
}

void
appendattr(attrnode_t *an, attr_t a)
{
	attrnode_t *nxt;

	while (nxt != NULL) {
		nxt = an->an_nxt;
		if (nxt)
			an = nxt;
	}

	an->an_nxt = newattrnode(a);
}

void
appendattrnode(attrnode_t *an, attrnode_t *apn)
{
	attrnode_t *nxt;

	while (nxt != NULL) {
		nxt = an->an_nxt;
		if (nxt)
			an = nxt;
	}

	an->an_nxt = apn;
}

void
addattr(type_t *t, attrnode_t *an)
{
	if (t->t_attr == NULL)
		t->t_attr = an;
	else
		appendattrnode(t->t_attr, an);
}

int
hasattr(type_t *t, attr_t a)
{
	attrnode_t *an = t->t_attr;

	while (an != NULL) {
		if (an->an_attr == a)
			return 1;

		an = an->an_nxt;
	}

	while (t != NULL) {
		t = t->t_subt;
	}

	return 0;
}
