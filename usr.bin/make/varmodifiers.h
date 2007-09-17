#ifndef VARMODIFIERS_H
#define VARMODIFIERS_H

/* $OpenPackages$ */
/* $OpenBSD: varmodifiers.h,v 1.10 2007/09/17 09:28:36 espie Exp $ */

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Extensive code changes for the OpenBSD project.
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

/* VarModifiers_Init();
 *	Set up varmodifiers internal table according to selected features.
 *	This can be called several times without harm. */
extern void VarModifiers_Init(void);


/* result = VarModifiers_Apply(val, name, ctxt, undef_is_bad,
 *   &should_free, &modstart, paren);
 *	Applies variable modifiers starting at modstart (including :),
 *	using parenthesis paren, to value val.
 *	Variables in spec are taken from context ctxt.
 *	If undef_is_bad, error occurs if undefined variables are mentioned.
 *	modstart is advanced past the end of the spec.
 *	name holds the name of the corresponding variable, as some ODE
 *	modifiers need it.
 *
 *	If both val and name are NULL, VarModifiers_Apply just parses the
 *	modifiers specification, as it can't apply it to anything. */
extern char *VarModifiers_Apply(char *, const struct Name *, SymTable *,
	bool, bool *, const char **, int);

/* Direct interface to specific modifiers used under special circumstances. */
/* tails = Var_GetTail(string);
 *	Returns the tail of list of words in string (needed for SysV locals). */
extern char *Var_GetTail(char *);
/* heads = Var_GetHead(string);
 *	Returns the head of list of words in string. */
/* XXX this does not replace foo with ., as (sun) System V make does.
 * Should it ? */
extern char *Var_GetHead(char *);
#endif
