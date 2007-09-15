/*      $Id: gcc_compat.c,v 1.1.1.1 2007/09/15 18:12:33 otto Exp $     */
/*
 * Copyright (c) 2004 Anders Magnusson (ragge@ludd.luth.se).
 * All rights reserved.
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
 *    derived from this software without specific prior written permission
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

/*
 * Routines to support some of the gcc extensions to C.
 */
#ifdef GCC_COMPAT

#include "pass1.h"
#include "cgram.h"

#include <string.h>

static struct kw {
	char *name, *ptr;
	int rv;
} kw[] = {
	{ "__asm", NULL, C_ASM },
	{ "__signed", NULL, 0 },
	{ "__inline", NULL, C_FUNSPEC },
	{ "__const", NULL, 0 },
	{ "__asm__", NULL, C_ASM },
	{ NULL, NULL, 0 },
};

void
gcc_init()
{
	struct kw *kwp;

	for (kwp = kw; kwp->name; kwp++)
		kwp->ptr = addname(kwp->name);

}

/*
 * See if a string matches a gcc keyword.
 */
int
gcc_keyword(char *str, NODE **n)
{
	struct kw *kwp;
	int i;

	for (i = 0, kwp = kw; kwp->name; kwp++, i++)
		if (str == kwp->ptr)
			break;
	if (kwp->name == NULL)
		return 0;
	if (kwp->rv)
		return kwp->rv;
	switch (i) {
	case 1: /* __signed */
		*n = mkty((TWORD)SIGNED, 0, MKSUE(SIGNED));
		return C_TYPE;
	case 3: /* __const */
		*n = block(QUALIFIER, NIL, NIL, CON, 0, 0);
		return C_QUALIFIER;
	}
	cerror("gcc_keyword");
	return 0;
}

static struct ren {
	struct ren *next;
	char *old, *new;
} *renp;
/*
 * Save a name for later renaming of a variable.
 */
void
gcc_rename(struct symtab *sp, char *newname)
{
	struct ren *ren = permalloc(sizeof(struct ren));

	sp->sflags |= SRENAME;
	ren->old = sp->sname;
	ren->new = newstring(newname, strlen(newname)+1);
	ren->next = renp;
	renp = ren;
}

/*
 * Get a renamed variable.
 */
char *
gcc_findname(struct symtab *sp)
{
	struct ren *w;

	if ((sp->sflags & SRENAME) == 0)
		return exname(sp->sname);

	for (w = renp; w; w = w->next) {
		if (w->old == sp->sname)
			return exname(w->new);
	}
	cerror("gcc_findname %s", sp->sname);
	return NULL;
}
#endif
