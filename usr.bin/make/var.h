#ifndef VAR_H
#define VAR_H
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

/* Global contexts handling. */
/* value = Var_Valuei(name, end);
 *	Returns value of global variable name/end, or NULL if inexistent. */
extern char *Var_Valuei(const char *, const char *);
#define Var_Value(n)	Var_Valuei(n, NULL)
/* Only check if variable is defined */
extern bool Var_Definedi(const char *, const char *);

/* Var_Seti(name, end, val, ctxt);
 *	Sets value val of variable name/end in context ctxt.  Copies val. */
extern void Var_Seti(const char *, const char *, const char *,
	int);
#define Var_Set(n, v, ctxt)	Var_Seti(n, NULL, v, ctxt)
/* Var_Appendi(name, end, val, cxt);
 *	Appends value val to variable name/end in context ctxt, defining it
 *	if it does not already exist, and inserting one space otherwise. */
extern void Var_Appendi(const char *, const char *,
	const char *, int);
#define Var_Append(n, v, ctxt)	Var_Appendi(n, NULL, v, ctxt)
	
/* Var_Delete(name);
 *	Deletes a variable from the global context.  */
extern void Var_Delete(const char *);

/* Local context handling */
#define TARGET_INDEX	0
#define PREFIX_INDEX	1
#define ARCHIVE_INDEX	2
#define MEMBER_INDEX	3
#define OODATE_INDEX	4
#define ALLSRC_INDEX	5
#define IMPSRC_INDEX	6
extern char *Varq_Value(int,  GNode *);
extern void Varq_Set(int, const char *, GNode *);
extern void Varq_Append(int, const char *, GNode *);

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

/* length = Var_ParseSkip(varspec, ctxt, &ok);
 *	Parses a variable specification and returns the specification
 *	length. Fills ok if the varspec is correct, that pointer can be
 *	NULL if this information is not needed.  */
extern size_t Var_ParseSkip(const char *, SymTable *, bool *);

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

/* Var_SubstVar(buf, str, varname, val);
 *	Substitutes variable varname with value val in string str, adding
 *	the result to buffer buf.  undefs are never error. */
extern void Var_SubstVar(Buffer, const char *, const char *, const char *);

/* Note that substituting to a buffer in Var_Subst is not useful. On the
 * other hand, handling intervals in Var_Subst and Var_Parse would be
 * useful, but this is hard. */

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

#define POISON_INVALID 		0
#define POISON_DEFINED 		1
#define POISON_NORMAL		64
#define POISON_EMPTY		128
#define POISON_NOT_DEFINED	256

extern void Var_MarkPoisoned(const char *, const char *, unsigned int);

#endif
