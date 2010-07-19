/*	$OpenBSD: varname.h,v 1.4 2010/07/19 19:30:38 espie Exp $	*/
#ifndef VARNAME_H
#define VARNAME_H
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

/* varname -
 * 	Handles variable names in recursive situations, e.g.,
 *	to expand ${A${BC}}.
 */

/* Used to store temporary names, e.g., after $ expansion. */
struct Name {
	const char 	*s;		/* Start of name. */
	const char 	*e;		/* End of name. */
	bool		tofree;		/* Needs freeing after use ? */
};

/* endpos = VarName_Get(startpos, &name, ctxt, undef_is_bad, cont_function);
 *	Gets a variable name from startpos, storing the result into name.
 *	Recursive names are expanded from context ctxt. Boolean
 *	undef_is_bad governs whether undefined variables should trigger a
 *	parse error.  To parse its argument efficiently, VarName_Get
 *	uses cont_function(pos), which should return the position of the
 *	next $ in string, or the position where the variable spec ends (as
 *	differing modules have different requirements wrt variable spec
 *	endings). Returns the position where the variable spec finally
 *	ends. Name result might be a copy, or not. */
extern const char *VarName_Get(const char *, struct Name *, SymTable *,
    bool, const char *(*)(const char *));
/* VarName_Free(name);
 *	Frees a variable name filled by VarName_Get(). */
extern void VarName_Free(struct Name *);

#endif
