/*
 * GSP assembler - symbol table
 *
 * Copyright (c) 1993 Paul Mackerras.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Mackerras.
 * 4. The name of the author may not be used to endorse or promote products
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

#include <string.h>
#include "gsp_ass.h"

#define NHASH	64		/* must be power of 2 */

symbol symbol_hash[NHASH];

symbol
lookup(char *id, bool makeit)
{
	register symbol ptr, p, *pp;
	register int h;
	register char *ip;

	h = 0;
	for( ip = id; *ip != 0; )
		h = (h << 1) + *ip++;
	h &= NHASH-1;
	for( pp = &symbol_hash[h]; (p = *pp) != NULL; pp = &p->next )
		if( (h = strcmp(id, p->name)) == 0 )
			return p;
		else if( h < 0 )
			break;
	if( !makeit )
		return NULL;
	ptr = (symbol) alloc (sizeof(struct symbol) + strlen(id));
	ptr->ndefn = 0;
	ptr->flags = 0;
	ptr->value = 0;
	ptr->lineno = NOT_YET;
	strcpy(ptr->name, id);
	*pp = ptr;
	ptr->next = p;
	ptr->nlab = NULL;
	return ptr;
}

void
define_sym(char *id, unsigned val, unsigned lineno, int flags)
{
	register symbol ptr;

	ptr = lookup(id, TRUE);
	if( (ptr->flags & SET_LABEL) == 0 ){
		if( ptr->ndefn >= 2 ){
			perr("Multiply defined label %s", id);
			if( (flags & SET_LABEL) != 0 )
				return;
		} else if( pass2 && ptr->value != val )
			perr("Phase error on label %s (%#x -> %#x)",
				id, ptr->value, val);
	}
	ptr->flags = flags;
	ptr->ndefn += 1;
	ptr->value = val;
	ptr->lineno = lineno;
}

void
set_label(char *id)
{
	if( id != NULL ){
		define_sym(id, pc, lineno, DEFINED);
		if( pass2 )
			do_list_pc();
	}
}

void
do_asg(char *name, expr value, int flags)
{
	int32_t val;
	unsigned line;

	if( eval_expr(value, &val, &line) )
		flags |= DEFINED;
	if( line < lineno )
		line = lineno;
	define_sym(name, val, line, flags);
	if( pass2 )
		do_show_val(val);
}

void
set_numeric_label(int lnum)
{
	register symbol bp, fp;
	register struct numlab *nl;
	char id[32];

	/* define the backward reference symbol */
	sprintf(id, "%dB", lnum);
	bp = lookup(id, TRUE);
	bp->flags = NUMERIC_LABEL | DEFINED;
	bp->value = pc;
	bp->lineno = lineno;

	/* look up the forward reference symbol */
	id[strlen(id) - 1] = 'F';
	fp = lookup(id, TRUE);

	if( !pass2 ){
		/* Record a new numeric label and link it into the
		   chain.  fp->nlab points to the head of the chain,
		   bp->nlab points to the tail.  */
		ALLOC(nl, struct numlab *);
		nl->value = pc;
		nl->lineno = lineno;
		nl->next = NULL;
		if( bp->nlab == NULL )
			fp->nlab = nl;
		else
			bp->nlab->next = nl;
		bp->nlab = nl;
		fp->flags = NUMERIC_LABEL;
	} else {
		/* Advance to the next numeric label entry in the chain
		   and update the value of the forward reference symbol. */
		if( pc != fp->value )
			perr("Phase error on numeric label %d (%#x -> %#x)",
				lnum, fp->value, pc);
		nl = fp->nlab;
		nl = nl->next;
		if( nl == NULL ){
			/* no more labels of this number */
			/* forward references are now undefined */
			fp->flags &= ~DEFINED;
			fp->lineno = NOT_YET;
			fp->value = 0;
		} else {
			fp->lineno = nl->lineno;
			fp->value = nl->value;
			fp->nlab = nl;
		}
		do_list_pc();
	}
}

/* At the beginning of pass 2, reset all of the numeric labels.
   Backward references become undefined, forward references are defined
   by the first instance of the label. */
void
reset_numeric_labels()
{
	register symbol p;
	register struct numlab *nl;
	register int h;

	for( h = 0; h < NHASH; ++h )
		for( p = symbol_hash[h]; p != NULL; p = p->next )
			if( (p->flags & NUMERIC_LABEL) != 0 )
				if( (p->flags & DEFINED) != 0 ){
					/* a backward reference */
					p->flags &= ~DEFINED;
				} else {
					/* a forward reference */
					p->flags |= DEFINED;
					nl = p->nlab;
					p->value = nl->value;
					p->lineno = nl->lineno;
				}
}
