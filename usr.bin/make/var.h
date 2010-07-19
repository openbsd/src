#ifndef VAR_H
#define VAR_H
/* $OpenBSD: var.h,v 1.14 2010/07/19 19:46:44 espie Exp $ */
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

extern void Var_Init(void);
#ifdef CLEANUP
extern void Var_End(void);
#else
#define Var_End()
#endif

extern void Var_setCheckEnvFirst(bool);

/* Global variable handling. */
/* value = Var_Valuei(name, end);
 *	Returns value of global variable name/end, or NULL if inexistent. */
extern char *Var_Valuei(const char *, const char *);
#define Var_Value(n)	Var_Valuei(n, NULL)

/* isDefined = Var_Definedi(name, end);
 *	Checks whether global variable name/end is defined. */
extern bool Var_Definedi(const char *, const char *);

/* Var_Seti_with_ctxt(name, end, val, ctxt);
 *	Sets value val of variable name/end.  Copies val.
 *	ctxt can be VAR_CMD (command line) or VAR_GLOBAL (normal variable). */
extern void Var_Seti_with_ctxt(const char *, const char *, const char *,
	int);
#define Var_Set(n, v)	Var_Seti_with_ctxt(n, NULL, v, VAR_GLOBAL)
#define Var_Seti(n, e, v) Var_Seti_with_ctxt(n, e, v, VAR_GLOBAL)
/* Var_Appendi_with_ctxt(name, end, val, cxt);
 *	Appends value val to variable name/end in context ctxt, defining it
 *	if it does not already exist, and inserting one space otherwise. */
extern void Var_Appendi_with_ctxt(const char *, const char *,
	const char *, int);
#define Var_Append(n, v)	Var_Appendi_with_ctxt(n, NULL, v, VAR_GLOBAL)
#define Var_Appendi(n, e, v) 	Var_Appendi_with_ctxt(n, e, v, VAR_GLOBAL)

/* Var_Deletei(name, end);
 *	Deletes a global variable. */
extern void Var_Deletei(const char *, const char *);

/* Dynamic variable indices */
#define TARGET_INDEX	0
#define PREFIX_INDEX	1
#define ARCHIVE_INDEX	2
#define MEMBER_INDEX	3
#define OODATE_INDEX	4
#define ALLSRC_INDEX	5
#define IMPSRC_INDEX	6

#define Var(idx, gn)	((gn)->context.locals[idx])


/* SymTable_Init(t);
 *	Inits the local symtable in a GNode. */
extern void SymTable_Init(SymTable *);
/* SymTable_destroy(t);
 *	Destroys the local symtable in a GNode. */
extern void SymTable_Destroy(SymTable *);

/* Several ways to parse a variable specification. */
/* value = Var_Parse(varspec, ctxt, undef_is_bad, &length, &freeit);
 *	Parses a variable specification varspec and evaluates it in context
 *	ctxt.  Returns the resulting value, freeit indicates whether it's
 *	a copy that should be freed when no longer needed.  If it's not a
 *	copy, it's only valid until the next time variables are set.
 *	The length of the spec is returned in length, e.g., varspec begins
 *	at the $ and ends at the closing } or ).  Returns special value
 *	var_Error if a problem occurred. */
extern char *Var_Parse(const char *, SymTable *, bool, size_t *,
	bool *);
/* Note that var_Error is an instance of the empty string "", so that
 * callers who don't care don't need to. */
extern char	var_Error[];

/* ok = Var_ParseSkip(&varspec, ctxt, &ok);
 *	Parses a variable specification and returns true if the varspec
 *	is correct. Advances pointer past specification.  */
extern bool Var_ParseSkip(const char **, SymTable *);

/* ok = Var_ParseBuffer(buf, varspec, ctxt, undef_is_bad, &length);
 *	Similar to Var_Parse, except the value is directly appended to
 *	buffer buf. */
extern bool Var_ParseBuffer(Buffer, const char *, SymTable *,
	bool, size_t *);


/* The substitution itself */
/* subst = Var_Subst(str, ctxt, undef_is_bad);
 *	Substitutes all variable values in string str under context ctxt.
 *	Emit a PARSE_FATAL error if undef_is_bad and an undef variable is
 *	encountered. The result is always a copy that should be free. */
extern char *Var_Subst(const char *, SymTable *, bool);
/* subst = Var_Substi(str, estr, ctxt, undef_if_bad);
 */
extern char *Var_Substi(const char *, const char *, SymTable *, bool);


/* For loop handling.
 *	// Create handle for variable name.
 *	handle = Var_NewLoopVar(name, end);
 *	// set up buffer
 *	for (...)
 *		// Substitute val for variable in str, and accumulate in buffer
 *		Var_SubstVar(buffer, str, handle, val);
 *	// Free handle
 *	Var_DeleteLoopVar(handle);
 */
struct LoopVar;	/* opaque handle */
struct LoopVar *Var_NewLoopVar(const char *, const char *);
void Var_DeleteLoopVar(struct LoopVar *);
extern void Var_SubstVar(Buffer, const char *, struct LoopVar *, const char *);
char *Var_LoopVarName(struct LoopVar *);


/* Var_Dump();
 *	Print out all global variables. */
extern void Var_Dump(void);

/* Var_AddCmdline(name);
 *	Add all variable values from VAR_CMD to variable name.
 *	Used to propagate variable values to submakes through MAKEFLAGS.  */
extern void Var_AddCmdline(const char *);

/* stuff common to var.c and varparse.c */
extern bool	errorIsOkay;

#define		VAR_GLOBAL	0
	/* Variables defined in a global context, e.g in the Makefile itself */
#define		VAR_CMD		1
	/* Variables defined on the command line */

#define POISON_INVALID		0
#define POISON_DEFINED		1
#define POISON_NORMAL		64
#define POISON_EMPTY		128
#define POISON_NOT_DEFINED	256

extern void Var_MarkPoisoned(const char *, const char *, unsigned int);

#endif
