/*	$OpenPackages$ */
/*	$OpenBSD: var.c,v 1.62 2007/07/08 17:44:20 espie Exp $	*/
/*	$NetBSD: var.c,v 1.18 1997/03/18 19:24:46 christos Exp $	*/

/*
 * Copyright (c) 1999,2000,2007 Marc Espie.
 *
 * Extensive code modifications for the OpenBSD project.
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
/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "defines.h"
#include "buf.h"
#include "stats.h"
#include "ohash.h"
#include "pathnames.h"
#include "varmodifiers.h"
#include "var.h"
#include "varname.h"
#include "error.h"
#include "str.h"
#include "var_int.h"
#include "memory.h"
#include "symtable.h"
#include "gnode.h"

/* extended indices for System V stuff */
#define FTARGET_INDEX	7
#define DTARGET_INDEX	8
#define FPREFIX_INDEX	9
#define DPREFIX_INDEX	10
#define FARCHIVE_INDEX	11
#define DARCHIVE_INDEX	12
#define FMEMBER_INDEX	13
#define DMEMBER_INDEX	14

#define EXTENDED2SIMPLE(i)	(((i)-LOCAL_SIZE)/2)
#define IS_EXTENDED_F(i)	((i)%2 == 1)

/*
 * This is a harmless return value for Var_Parse that can be used by Var_Subst
 * to determine if there was an error in parsing -- easier than returning
 * a flag, as things outside this module don't give a hoot.
 */
char	var_Error[] = "";

/*
 * Similar to var_Error, but returned when the 'err' flag for Var_Parse is
 * set false. Why not just use a constant? Well, gcc likes to condense
 * identical string instances...
 */
static char	varNoError[] = "";
bool 		oldVars;	/* variable substitution style */
static bool 	checkEnvFirst;	/* true if environment should be searched for
			         * variables before the global context */

void
Var_setCheckEnvFirst(bool yes)
{
	checkEnvFirst = yes;
}

/*
 * Variable values are obtained from four different contexts:
 *	1) the process environment. The process environment itself
 *	   may not be changed, but these variables may be modified,
 *	   unless make is invoked with -e, in which case those variables
 *	   are unmodifiable and supersede the global context.
 *	2) the global context. Variables set in the Makefile are located in
 *	    the global context. It is the penultimate context searched when
 *	    substituting.
 *	3) the command-line context. All variables set on the command line
 *	   are placed in this context. They are UNALTERABLE once placed here.
 *	4) the local context. Each target has associated with it a context
 *	   list. On this list are located the structures describing such
 *	   local variables as $(@) and $(*)
 * The four contexts are searched in the reverse order from which they are
 * listed.
 */

static struct ohash global_variables;

static char *varnames[] = {
    TARGET,
    PREFIX,
    ARCHIVE,
    MEMBER,
    OODATE,
    ALLSRC,
    IMPSRC,
    FTARGET,
    DTARGET,
    FPREFIX,
    DPREFIX,
    FARCHIVE,
    DARCHIVE,
    FMEMBER,
    DMEMBER
    };


typedef struct Var_ {
    BUFFER	  val;		/* its value */
    unsigned int  flags;	/* miscellaneous status flags */
#define VAR_IN_USE	1	/* Variable's value currently being used. */
				/* Used to avoid recursion */
#define VAR_DUMMY	2	/* Placeholder: already looked up */
#define VAR_FROM_CMD	4	/* From the command line */
#define VAR_FROM_ENV	8	/* Read from environment */
#define VAR_SEEN_ENV	16	/* Already seen environment */
#define VAR_SHELL	32	/* magic, see posix */
#define POISONS (POISON_NORMAL | POISON_EMPTY | POISON_NOT_DEFINED)
    char	  name[1];	/* the variable's name */
}  Var;


static struct ohash_info var_info = {
	offsetof(Var, name),
    NULL, hash_alloc, hash_free, element_alloc };
static int quick_lookup(const char *, const char **, uint32_t *);
#define VarValue(v)	Buf_Retrieve(&((v)->val))
static Var *varfind(const char *, const char *, SymTable *, int, uint32_t);
static Var *find_global_var(const char *, const char *, uint32_t);
static void VarDelete(Var *);
static void VarPrintVar(Var *);

static Var *obtain_global_var(const char *, const char *, uint32_t);
static void fill_from_env(Var *);
static Var *create_var(const char *, const char *);
static void varq_set_append(int, const char *, GNode *, bool);
static void var_init_string(Var *, const char *);
static void var_set_string(Var *, const char *);
static void var_append_string(Var *, const char *);
static void var_set_append(const char *, const char *, const char *, int, bool);
static void set_magic_shell_variable(void);
static void poison_check(Var *);
static const char *find_0(const char *);
static const char *find_rparen(const char *);
static const char *find_ket(const char *);
typedef const char * (*find_t)(const char *);
static find_t find_pos(int);

/* retrieve the hashed values  for well-known variables.  */
#include    "varhashconsts.h"

void
SymTable_Init(SymTable *ctxt)
{
    static SymTable sym_template;	
    memcpy(ctxt, &sym_template, sizeof(*ctxt));
}

#ifdef CLEANUP
void
SymTable_Destroy(SymTable *ctxt)
{
    int i;

    for (i = 0; i < LOCAL_SIZE; i++)
	if (ctxt->locals[i] != NULL)
	    VarDelete(ctxt->locals[i]);
}
#endif

static int
quick_lookup(const char *name, const char **enamePtr, uint32_t *pk)
{
    size_t len;

    *pk = ohash_interval(name, enamePtr);
    len = *enamePtr - name;
	/* substitute short version for long local name */
    switch (*pk % MAGICSLOTS1) { 	    /* MAGICSLOTS should be the    */
    case K_LONGALLSRC % MAGICSLOTS1:	    /* smallest constant yielding  */
					    /* distinct case values	   */
	if (*pk == K_LONGALLSRC && len == strlen(LONGALLSRC) && 
	    strncmp(name, LONGALLSRC, len) == 0)
	    return ALLSRC_INDEX;
	break;
    case K_LONGARCHIVE % MAGICSLOTS1:
	if (*pk == K_LONGARCHIVE && len == strlen(LONGARCHIVE) &&
	    strncmp(name, LONGARCHIVE, len) == 0)
	    return ARCHIVE_INDEX;
	break;
    case K_LONGIMPSRC % MAGICSLOTS1:
	if (*pk == K_LONGIMPSRC && len == strlen(LONGIMPSRC) &&
	    strncmp(name, LONGIMPSRC, len) == 0)
	    return IMPSRC_INDEX;
	break;
    case K_LONGMEMBER % MAGICSLOTS1:
	if (*pk == K_LONGMEMBER && len == strlen(LONGMEMBER) &&
	    strncmp(name, LONGMEMBER, len) == 0)
	    return MEMBER_INDEX;
	break;
    case K_LONGOODATE % MAGICSLOTS1:
	if (*pk == K_LONGOODATE && len == strlen(LONGOODATE) &&
	    strncmp(name, LONGOODATE, len) == 0)
	    return OODATE_INDEX;
	break;
    case K_LONGPREFIX % MAGICSLOTS1:
	if (*pk == K_LONGPREFIX && len == strlen(LONGPREFIX) &&
	    strncmp(name, LONGPREFIX, len) == 0)
	    return PREFIX_INDEX;
	break;
    case K_LONGTARGET % MAGICSLOTS1:
	if (*pk == K_LONGTARGET && len == strlen(LONGTARGET) &&
	    strncmp(name, LONGTARGET, len) == 0)
	    return TARGET_INDEX;
	break;
    case K_TARGET % MAGICSLOTS1:
	if (name[0] == TARGET[0] && len == 1)
	    return TARGET_INDEX;
	break;
    case K_OODATE % MAGICSLOTS1:
	if (name[0] == OODATE[0] && len == 1)
	    return OODATE_INDEX;
	break;
    case K_ALLSRC % MAGICSLOTS1:
	if (name[0] == ALLSRC[0] && len == 1)
	    return ALLSRC_INDEX;
	break;
    case K_IMPSRC % MAGICSLOTS1:
	if (name[0] == IMPSRC[0] && len == 1)
	    return IMPSRC_INDEX;
	break;
    case K_PREFIX % MAGICSLOTS1:
	if (name[0] == PREFIX[0] && len == 1)
	    return PREFIX_INDEX;
	break;
    case K_ARCHIVE % MAGICSLOTS1:
	if (name[0] == ARCHIVE[0] && len == 1)
	    return ARCHIVE_INDEX;
	break;
    case K_MEMBER % MAGICSLOTS1:
	if (name[0] == MEMBER[0] && len == 1)
	    return MEMBER_INDEX;
	break;
    case K_FTARGET % MAGICSLOTS1:
    	if (name[0] == FTARGET[0] && name[1] == FTARGET[1] && len == 2)
	    return FTARGET_INDEX;
	break;
    case K_DTARGET % MAGICSLOTS1:
    	if (name[0] == DTARGET[0] && name[1] == DTARGET[1] && len == 2)
	    return DTARGET_INDEX;
	break;
    case K_FPREFIX % MAGICSLOTS1:
    	if (name[0] == FPREFIX[0] && name[1] == FPREFIX[1] && len == 2)
	    return FPREFIX_INDEX;
	break;
    case K_DPREFIX % MAGICSLOTS1:
    	if (name[0] == DPREFIX[0] && name[1] == DPREFIX[1] && len == 2)
	    return DPREFIX_INDEX;
	break;
    case K_FARCHIVE % MAGICSLOTS1:
    	if (name[0] == FARCHIVE[0] && name[1] == FARCHIVE[1] && len == 2)
	    return FARCHIVE_INDEX;
	break;
    case K_DARCHIVE % MAGICSLOTS1:
    	if (name[0] == DARCHIVE[0] && name[1] == DARCHIVE[1] && len == 2)
	    return DARCHIVE_INDEX;
	break;
    case K_FMEMBER % MAGICSLOTS1:
    	if (name[0] == FMEMBER[0] && name[1] == FMEMBER[1] && len == 2)
	    return FMEMBER_INDEX;
	break;
    case K_DMEMBER % MAGICSLOTS1:
    	if (name[0] == DMEMBER[0] && name[1] == DMEMBER[1] && len == 2)
	    return DMEMBER_INDEX;
	break;
    default:
	break;
    }
    return -1;
}

static void
varq_set_append(int idx, const char *val, GNode *gn, bool append)
{
    Var *v = gn->context.locals[idx];

    if (v == NULL) {
	v = create_var(varnames[idx], NULL);
#ifdef STATS_VAR_LOOKUP
	STAT_VAR_CREATION++;
#endif
	if (val != NULL)
	    var_init_string(v, val);
	else
	    Buf_Init(&(v->val), 1);
	v->flags = 0;
	gn->context.locals[idx] = v;
    } else {
    	if (append)
		Buf_AddSpace(&(v->val));
	else
		Buf_Reset(&(v->val));
	Buf_AddString(&(v->val), val);
    }
    if (DEBUG(VAR))
	printf("%s:%s = %s\n", gn->name, varnames[idx], VarValue(v));
}

void
Varq_Set(int idx, const char *val, GNode *gn)
{
    varq_set_append(idx, val, gn, false);
}

void
Varq_Append(int idx, const char *val, GNode *gn)
{
    varq_set_append(idx, val, gn, true);
}

char *
Varq_Value(int idx, GNode *gn)
{
    Var *v = gn->context.locals[idx];

    if (v == NULL)
    	return NULL;
    else
	return VarValue(v);
}

static Var *
create_var(const char *name, const char *ename)
{
    return ohash_create_entry(&var_info, name, &ename);
}

/* Set the initial value a var should have */
static void
var_init_string(Var *v, const char *val)
{
    size_t len;

    len = strlen(val);
    Buf_Init(&(v->val), len+1);
    Buf_AddChars(&(v->val), len, val);
}

static void
var_set_string(Var *v, const char *val)
{
    if ((v->flags & VAR_DUMMY) == 0) {
	Buf_Reset(&(v->val));
	Buf_AddString(&(v->val), val);
    } else {
    	var_init_string(v, val);
	v->flags &= ~VAR_DUMMY;
    }
}

static void
var_append_string(Var *v, const char *val)
{
    if ((v->flags & VAR_DUMMY) == 0) {
    	Buf_AddSpace(&(v->val));
	Buf_AddString(&(v->val), val);
    } else {
    	var_init_string(v, val);
	v->flags &= ~VAR_DUMMY;
    }
}

static void
fill_from_env(Var *v)
{
    char	*env;

    env = getenv(v->name);
    if (env == NULL)
	v->flags |= VAR_SEEN_ENV;
    else {
    	var_set_string(v, env);
	v->flags |= VAR_FROM_ENV | VAR_SEEN_ENV;
    }

#ifdef STATS_VAR_LOOKUP
    STAT_VAR_FROM_ENV++;
#endif
}

static Var *
obtain_global_var(const char *name, const char *ename, uint32_t k)
{
	unsigned int slot;
	Var *v;

	slot = ohash_lookup_interval(&global_variables, name, ename, k);
	v = ohash_find(&global_variables, slot);
	if (v == NULL) {
		v = create_var(name, ename);
		v->flags = VAR_DUMMY;
		ohash_insert(&global_variables, slot, v);
	}
	return v;
}

static void
poison_check(Var *v)
{
	if (v->flags & POISON_NORMAL) {
		Parse_Error(PARSE_FATAL, 
		    "Poisoned variable %s has been referenced\n", v->name);
		return;
	}
	if (v->flags & VAR_DUMMY) {
		Parse_Error(PARSE_FATAL,
		    "Poisoned variable %s is not defined\n", v->name);
		return;
	}
	if (v->flags & POISON_EMPTY)
		if (strcmp(VarValue(v), "") == 0)
			Parse_Error(PARSE_FATAL, 
			    "Poisoned variable %s is empty\n", v->name);
}

static Var *
find_global_var(const char *name, const char *ename, uint32_t k)
{
    Var 		*v;

    v = obtain_global_var(name, ename, k);

    if ((v->flags & VAR_SEEN_ENV) == 0 &&
    	(checkEnvFirst  && (v->flags & VAR_FROM_CMD) == 0 || 
	    (v->flags & VAR_DUMMY) != 0))
		fill_from_env(v);

    return v;
}

static Var *
varfind(const char *name, const char *ename, SymTable *ctxt, 
    int idx, uint32_t k)
{
    /* Handle local variables first */
    if (idx != -1) {
    	if (ctxt != NULL) {
		if (idx < LOCAL_SIZE)
		    return ctxt->locals[idx];
		else
		    return ctxt->locals[EXTENDED2SIMPLE(idx)];
	} else
		return NULL;
    } else {
    	return find_global_var(name, ename, k);
    }
}

/*-
 *-----------------------------------------------------------------------
 * VarDelete  --
 *	Delete a variable and all the space associated with it.
 *-----------------------------------------------------------------------
 */
static void
VarDelete(Var *v)
{
    if ((v->flags & VAR_DUMMY) == 0)
	Buf_Destroy(&(v->val));
    free(v);
}



void
Var_Delete(const char *name)
{
	Var 	*v;
	uint32_t 	k;
	unsigned int slot;
	const char 	*ename = NULL;
	int		idx;


	if (DEBUG(VAR))
		printf("delete %s\n", name);

	idx = quick_lookup(name, &ename, &k);
	if (idx != -1)
		Parse_Error(PARSE_FATAL, "Trying to delete dynamic variable");
	slot = ohash_lookup_interval(&global_variables, name, ename, k);
	v = ohash_find(&global_variables, slot);
	
	if (v == NULL)
		return;
	if (checkEnvFirst && (v->flags & VAR_FROM_ENV))
		return;

	if (v->flags & VAR_FROM_CMD)
		return;

	ohash_remove(&global_variables, slot);
	VarDelete(v);
}

static void
var_set_append(const char *name, const char *ename, const char *val, int ctxt,
    bool append)
{
	Var   *v;
	uint32_t	k;
	int		idx;

	idx = quick_lookup(name, &ename, &k);
	if (idx != -1) {
		Parse_Error(PARSE_FATAL, "Trying to %s dynamic variable $%s",
		    append ? "append to" : "set", varnames[idx]);
		return;
	}

	v = find_global_var(name, ename, k);
	if (v->flags & POISON_NORMAL)
		Parse_Error(PARSE_FATAL, "Trying to %s poisoned variable %s\n",
		    append ? "append to" : "set", v->name);
	/* so can we write to it ? */
	if (ctxt == VAR_CMD) { 	/* always for command line */
		(append ? var_append_string : var_set_string)(v, val);
		v->flags |= VAR_FROM_CMD;
		if ((v->flags & VAR_SHELL) == 0) {
			/* Any variables given on the command line are 
			 * automatically exported to the environment,
			 * except for SHELL (as per POSIX standard).  
			 */
			esetenv(v->name, val);
	    	}
		if (DEBUG(VAR))
			printf("command:%s = %s\n", v->name, VarValue(v));
	} else if ((v->flags & VAR_FROM_CMD) == 0 &&
	     (!checkEnvFirst || (v->flags & VAR_FROM_ENV) == 0)) {
		(append ? var_append_string : var_set_string)(v, val);
		if (DEBUG(VAR))
			printf("global:%s = %s\n", v->name, VarValue(v));
	} else if (DEBUG(VAR))
			printf("overriden:%s = %s\n", v->name, VarValue(v));
}

void
Var_Seti(const char *name, const char *ename, const char *val, int ctxt)
{
	var_set_append(name, ename, val, ctxt, false);
}

void
Var_Appendi(const char *name, const char *ename, const char *val, int ctxt)
{
	var_set_append(name, ename, val, ctxt, true);
}

void
Var_MarkPoisoned(const char *name, const char *ename, unsigned int type)
{
	Var   *v;
	uint32_t	k;
	int		idx;
	idx = quick_lookup(name, &ename, &k);

	if (idx != -1) {
		Parse_Error(PARSE_FATAL, 
		    "Trying to poison dynamic variable $%s",
		    varnames[idx]);
		return;
	}

	v = find_global_var(name, ename, k);
	v->flags |= type;
	if (v->flags & POISON_NORMAL) {
		if (v->flags & VAR_DUMMY)
			return;
		if (v->flags & VAR_FROM_ENV)
			return;
		Parse_Error(PARSE_FATAL,
		    "Poisoned variable %s is already set\n", v->name);
	}
}

char *
Var_Valuei(const char *name, const char *ename)
{
	Var 	   *v;
	uint32_t		k;
	int 		idx;

	idx = quick_lookup(name, &ename, &k);
	if (idx == -1) {
		v = find_global_var(name, ename, k);
		if (v->flags & POISONS)
		    poison_check(v);
		if ((v->flags & VAR_DUMMY) == 0)
			return VarValue(v);
	}

	return NULL;
}

bool
Var_Definedi(const char *name, const char *ename)
{
	Var 	   	*v;
	uint32_t	k;
	int 		idx;

	idx = quick_lookup(name, &ename, &k);
	if (idx == -1) {
		v = find_global_var(name, ename, k);
		if (v->flags & POISON_NORMAL)
		    poison_check(v);
		if ((v->flags & VAR_DUMMY) == 0)
			return true;
	}

	return false;
}

static const char *
find_0(const char *p)
{
	while (*p != '$' && *p != '\0' && *p != ':')
		p++;
	return p;
}

static const char *
find_rparen(const char *p)
{
	while (*p != '$' && *p != '\0' && *p != ')' && *p != ':')
		p++;
	return p;
}

static const char *
find_ket(const char *p)
{
	while (*p != '$' && *p != '\0' && *p != '}' && *p != ':')
		p++;
	return p;
}

static find_t
find_pos(int c)
{
	switch(c) {
	case '\0':
		return find_0;
	case ')':
		return find_rparen;
	case '}':
		return find_ket;
	default:
		return 0;
	}
}

size_t
Var_ParseSkip(const char *str, SymTable *ctxt, bool *result)
{
    const char	*tstr;		/* Pointer into str */
    Var 	*v;		/* Variable in invocation */
    char	endc;		/* Ending character when variable in parens
				 * or braces */
    const char	*start;
    size_t	length;
    struct Name name;

    v = NULL;
    start = str;
    str++;

    if (*str != '(' && *str != '{') {
	name.tofree = false;
	tstr = str + 1;
	length = 2;
	endc = '\0';
    } else {
	endc = *str == '(' ? ')' : '}';
	str++;

	/* Find eventual modifiers in the variable */
	tstr = VarName_Get(str, &name, ctxt, false, find_pos(endc));
	VarName_Free(&name);
	length = tstr - start;
	if (*tstr != 0)
	    length++;
    }

    if (result != NULL)
	*result = true;
    if (*tstr == ':' && endc != '\0')
	 if (VarModifiers_Apply(NULL, NULL, ctxt, true, NULL, tstr, endc,
	    &length) == var_Error)
		if (result != NULL)
		    *result = false;
    return length;
}

/* As of now, Var_ParseBuffer is just a wrapper around Var_Parse. For
 * speed, it may be better to revisit the implementation to do things
 * directly. */
bool
Var_ParseBuffer(Buffer buf, const char *str, SymTable *ctxt, bool err, 
    size_t *lengthPtr)
{
    char	*result;
    bool	freeIt;

    result = Var_Parse(str, ctxt, err, lengthPtr, &freeIt);
    if (result == var_Error)
	return false;

    Buf_AddString(buf, result);
    if (freeIt)
	free(result);
    return true;
}

char *
Var_Parse(const char *str, 	/* The string to parse */
    SymTable *ctxt, 		/* The context for the variable */
    bool err, 			/* true if undefined variables are an error */
    size_t *lengthPtr, 		/* OUT: The length of the specification */
    bool *freePtr)		/* OUT: true if caller should free result */
{
    const char	*tstr;		/* Pointer into str */
    Var 	*v;		/* Variable in invocation */
    char	endc;		/* Ending character when variable in parens
				 * or braces */
    struct Name	name;
    const char	*start;
    char	*val;		/* Variable value  */
    uint32_t	k;
    int 	idx;

    *freePtr = false;
    start = str++;

    val = NULL;
    v = NULL;
    idx = -1;

    if (*str != '(' && *str != '{') {
    	name.s = str;
	name.e = str+1;
	name.tofree = false;
	tstr = str + 1;
	*lengthPtr = 2;
	endc = '\0';
    } else {
	endc = *str == '(' ? ')' : '}';
	str++;

	/* Find eventual modifiers in the variable */
	tstr = VarName_Get(str, &name, ctxt, false, find_pos(endc));
	*lengthPtr = tstr - start;
	if (*tstr != '\0')
		(*lengthPtr)++;
    }

    idx = quick_lookup(name.s, &name.e, &k);
    v = varfind(name.s, name.e, ctxt, idx, k);
    if (v->flags & POISONS)
    	poison_check(v);
    if (v != NULL && (v->flags & VAR_DUMMY) == 0) {
	if (v->flags & VAR_IN_USE)
	    Fatal("Variable %s is recursive.", v->name);
	    /*NOTREACHED*/
	else
	    v->flags |= VAR_IN_USE;

	/* Before doing any modification, we have to make sure the value
	 * has been fully expanded. If it looks like recursion might be
	 * necessary (there's a dollar sign somewhere in the variable's value)
	 * we just call Var_Subst to do any other substitutions that are
	 * necessary. Note that the value returned by Var_Subst will have
	 * been dynamically-allocated, so it will need freeing when we
	 * return.  */
	val = VarValue(v);
	if (idx == -1) {
	    if (strchr(val, '$') != NULL) {
		val = Var_Subst(val, ctxt, err);
		*freePtr = true;
	    }
	} else if (idx >= LOCAL_SIZE) {
	    if (IS_EXTENDED_F(idx))
		val = Var_GetTail(val);
	    else
		val = Var_GetHead(val);
	    *freePtr = true;
	}
	v->flags &= ~VAR_IN_USE;
    }
    if (*tstr == ':' && endc != '\0')
	val = VarModifiers_Apply(val, &name, ctxt, err, freePtr, tstr, endc,
	    lengthPtr);
    if (val == NULL) {
	val = err ? var_Error : varNoError;
	/* Dynamic source */
	if (idx != -1) {
	    /* can't be expanded for now: copy the var spec instead. */
	    if (ctxt == NULL) {
		*freePtr = true;
		val = Str_dupi(start, start+ *lengthPtr);
	    } else {
	    /* somehow, this should have been expanded already. */
		GNode *n;

		n = (GNode *)(((char *)ctxt) - offsetof(GNode, context));
		if (idx >= LOCAL_SIZE)
			idx = EXTENDED2SIMPLE(idx);
		switch(idx) {
		case IMPSRC_INDEX:
		    Fatal("Using $< in a non-suffix rule context is a GNUmake idiom (line %lu of %s)",
			n->lineno, n->fname);
		default:
		    Error("Using undefined dynamic variable $%s (line %lu of %s)", 
			varnames[idx], n->lineno, n->fname);
		    break;
		}
	    }
	}
    }
    VarName_Free(&name);
    return val;
}

char *
Var_Subst(const char *str, 	/* the string in which to substitute */
    SymTable *ctxt, 		/* the context wherein to find variables */
    bool undefErr)		/* true if undefineds are an error */
{
    BUFFER	  buf;		/* Buffer for forming things */
    static bool errorReported;  /* Set true if an error has already
				 * been reported to prevent a plethora
				 * of messages when recursing */

    Buf_Init(&buf, MAKE_BSIZE);
    errorReported = false;

    for (;;) {
	char		*val;	/* Value to substitute for a variable */
	size_t		length; /* Length of the variable invocation */
	bool 	doFree; 	/* Set true if val should be freed */
	const char *cp;

	/* copy uninteresting stuff */
	for (cp = str; *str != '\0' && *str != '$'; str++)
	    ;
	Buf_Addi(&buf, cp, str);
	if (*str == '\0')
	    break;
	if (str[1] == '$') {
	    /* A dollar sign may be escaped with another dollar sign.  */
	    Buf_AddChar(&buf, '$');
	    str += 2;
	    continue;
	}
	val = Var_Parse(str, ctxt, undefErr, &length, &doFree);
	/* When we come down here, val should either point to the
	 * value of this variable, suitably modified, or be NULL.
	 * Length should be the total length of the potential
	 * variable invocation (from $ to end character...) */
	if (val == var_Error || val == varNoError) {
	    /* If performing old-time variable substitution, skip over
	     * the variable and continue with the substitution. Otherwise,
	     * store the dollar sign and advance str so we continue with
	     * the string...  */
	    if (oldVars)
		str += length;
	    else if (undefErr) {
		/* If variable is undefined, complain and skip the
		 * variable. The complaint will stop us from doing anything
		 * when the file is parsed.  */
		if (!errorReported)
		    Parse_Error(PARSE_FATAL,
				 "Undefined variable \"%.*s\"",length,str);
		str += length;
		errorReported = true;
	    } else {
		Buf_AddChar(&buf, *str);
		str++;
	    }
	} else {
	    /* We've now got a variable structure to store in. But first,
	     * advance the string pointer.  */
	    str += length;

	    /* Copy all the characters from the variable value straight
	     * into the new string.  */
	    Buf_AddString(&buf, val);
	    if (doFree)
		free(val);
	}
    }
    return  Buf_Retrieve(&buf);
}

void
Var_SubstVar(Buffer buf, 	/* To store result */
    const char *str, 		/* The string in which to substitute */
    const char *var, 		/* Named variable */
    const char *val)		/* Its value */
{

    assert(*var != '\0');

    for (;;) {
	const char *start;
	/* Copy uninteresting stuff */
	for (start = str; *str != '\0' && *str != '$'; str++)
	    ;
	Buf_Addi(buf, start, str);

	start = str;
	if (*str++ == '\0')
	    break;
	str++;
	/* and escaped dollars */
	if (start[1] == '$') {
	    Buf_Addi(buf, start, start+2);
	    continue;
	}
	/* Simple variable, if it's not us, copy.  */
	if (start[1] != '(' && start[1] != '{') {
	    if (start[1] != *var || var[1] != '\0') {
		Buf_AddChars(buf, 2, start);
		continue;
	    }
	} else {
	    const char *p;
	    char endc;

	    if (start[1] == '(')
		endc = ')';
	    else
		endc = '}';

	    /* Find the end of the variable specification.  */
	    p = str;
	    while (*p != '\0' && *p != ':' && *p != endc && *p != '$')
		p++;
	    /* A variable inside the variable.	We don't know how to
	     * expand the external variable at this point, so we try
	     * again with the nested variable.	*/
	    if (*p == '$') {
		Buf_Addi(buf, start, p);
		str = p;
		continue;
	    }

	    if (strncmp(var, str, p - str) != 0 ||
		var[p - str] != '\0') {
		/* Not the variable we want to expand.	*/
		Buf_Addi(buf, start, p);
		str = p;
		continue;
	    }
	    if (*p == ':') {
		size_t	length; 	/* Length of the variable invocation */
		bool doFree; 	/* Set true if val should be freed */
		char	*newval;	/* Value substituted for a variable */
		struct Name name;

		length = p - str + 1;
		doFree = false;
		name.s = var;
		name.e = var + (p-str);

		/* val won't be freed since doFree == false, but
		 * VarModifiers_Apply doesn't know that, hence the cast. */
		newval = VarModifiers_Apply((char *)val, &name, NULL, false,
		    &doFree, p, endc, &length);
		Buf_AddString(buf, newval);
		if (doFree)
		    free(newval);
		str += length;
		continue;
	    } else
		str = p+1;
	}
	Buf_AddString(buf, val);
    }
}

static void
set_magic_shell_variable()
{
    const char *name = "SHELL";
    const char *ename = NULL;
    uint32_t k;
    Var *v;
    k = ohash_interval(name, &ename);
    v = create_var(name, ename);
    ohash_insert(&global_variables, 
    	ohash_lookup_interval(&global_variables, name, ename, k), v);
	/* the environment shall not affect it */
    v->flags = VAR_SHELL | VAR_SEEN_ENV;
    var_init_string(v, _PATH_BSHELL);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Init --
 *	Initialize the module
 *
 * Side Effects:
 *	The CTXT_CMD and CTXT_GLOBAL contexts are initialized
 *-----------------------------------------------------------------------
 */
void
Var_Init(void)
{
    ohash_init(&global_variables, 10, &var_info);
    set_magic_shell_variable();


    oldVars = true;
    Var_setCheckEnvFirst(false);

    VarModifiers_Init();
}


#ifdef CLEANUP
void
Var_End(void)
{
    Var *v;
    unsigned int i;

    for (v = ohash_first(&global_variables, &i); v != NULL;
	v = ohash_next(&global_variables, &i))
	    VarDelete(v);
}
#endif

static const char *interpret(int);

static const char *
interpret(int f)
{
    if (f & VAR_DUMMY)
	return "(D)";
    return "";
}


/****************** PRINT DEBUGGING INFO *****************/
static void
VarPrintVar(Var *v)
{
    printf("%-16s%s = %s\n", v->name, interpret(v->flags),
	(v->flags & VAR_DUMMY) == 0 ? VarValue(v) : "(none)");
}

void
Var_Dump(void)
{
    Var *v;
    unsigned int i;

    printf("#*** Global Variables:\n");

    for (v = ohash_first(&global_variables, &i); v != NULL;
	v = ohash_next(&global_variables, &i))
	VarPrintVar(v);

}

static const char *quotable = " \t\n\\'\"";

/* In POSIX mode, variable assignments passed on the command line are
 * propagated to sub makes through MAKEFLAGS.
 */
void
Var_AddCmdline(const char *name)
{
    Var *v;
    unsigned int i;
    BUFFER buf;
    char *s;

    Buf_Init(&buf, MAKE_BSIZE);

    for (v = ohash_first(&global_variables, &i); v != NULL;
	v = ohash_next(&global_variables, &i)) {
		if (!(v->flags & VAR_FROM_CMD)) {
			continue;
		}
		/* We assume variable names don't need quoting */
		Buf_AddString(&buf, v->name);
		Buf_AddChar(&buf, '=');
		for (s = VarValue(v); *s != '\0'; s++) {
			if (strchr(quotable, *s))
				Buf_AddChar(&buf, '\\');
			Buf_AddChar(&buf, *s);
		}
		Buf_AddSpace(&buf);
    }
    Var_Append(name, Buf_Retrieve(&buf), VAR_GLOBAL);
    Buf_Destroy(&buf);
}
