/*	$OpenBSD: var.c,v 1.46 2000/07/18 20:17:20 espie Exp $	*/
/*	$NetBSD: var.c,v 1.18 1997/03/18 19:24:46 christos Exp $	*/

/*
 * Copyright (c) 1999 Marc Espie.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)var.c	8.3 (Berkeley) 3/19/94";
#else
static char rcsid[] = "$OpenBSD: var.c,v 1.46 2000/07/18 20:17:20 espie Exp $";
#endif
#endif /* not lint */

/*-
 * var.c --
 *	Variable-handling functions
 *
 * Interface:
 *	Var_Set	  	    Set the value of a variable in the given
 *	    	  	    context. The variable is created if it doesn't
 *	    	  	    yet exist. The value and variable name need not
 *	    	  	    be preserved.
 *
 *	Var_Append	    Append more characters to an existing variable
 *	    	  	    in the given context. The variable needn't
 *	    	  	    exist already -- it will be created if it doesn't.
 *	    	  	    A space is placed between the old value and the
 *	    	  	    new one.
 *
 *	Var_Exists	    See if a variable exists.
 *
 *	Var_Value 	    Return the value of a variable in a context or
 *	    	  	    NULL if the variable is undefined.
 *
 *	Var_Subst 	    Substitute named variable, or all variables if
 *			    NULL in a string using
 *	    	  	    the given context as the top-most one. If the
 *	    	  	    third argument is non-zero, Parse_Error is
 *	    	  	    called if any variables are undefined.
 *
 *	Var_Parse 	    Parse a variable expansion from a string and
 *	    	  	    return the result and the number of characters
 *	    	  	    consumed.
 *
 *	Var_Delete	    Delete a variable in a context.
 *
 *	Var_Init  	    Initialize this module.
 *
 * Debugging:
 *	Var_Dump  	    Print out all variables defined in the given
 *	    	  	    context.
 *
 * XXX: There's a lot of duplication in these functions.
 */

#include    <ctype.h>
#ifndef MAKE_BOOTSTRAP
#include    <sys/types.h>
#include    <regex.h>
#endif
#include    <stdlib.h>
#include    <stddef.h>
#include    "make.h"
#include    "buf.h"
#include    "ohash.h"
#include    "hashconsts.h"
#include    "varmodifiers.h"

static SymTable *CTXT_GLOBAL, *CTXT_CMD, *CTXT_ENV;

static char *varnames[] = {
    TARGET,
    OODATE,
    ALLSRC,
    IMPSRC,
    PREFIX,
    ARCHIVE,
    MEMBER };

/*
 * This is a harmless return value for Var_Parse that can be used by Var_Subst
 * to determine if there was an error in parsing -- easier than returning
 * a flag, as things outside this module don't give a hoot.
 */
char 	var_Error[] = "";

/*
 * Similar to var_Error, but returned when the 'err' flag for Var_Parse is
 * set false. Why not just use a constant? Well, gcc likes to condense
 * identical string instances...
 */
static char	varNoError[] = "";

/*
 * Internally, variables are contained in four different contexts.
 *	1) the environment. They may not be changed. If an environment
 *	    variable is appended-to, the result is placed in the global
 *	    context.
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
GSymT		*VAR_GLOBAL;	/* variables from the makefile */
GSymT		*VAR_CMD;	/* variables defined on the command-line */
static GSymT	*VAR_ENV;	/* variables read from env */

#define FIND_MINE	0x1   /* look in CTXT_CMD and CTXT_GLOBAL */
#define FIND_ENV  	0x2   /* look in the environment */

typedef struct Var_ {
    BUFFER	  val;	    	/* its value */
    int	    	  flags;    	/* miscellaneous status flags */
#define VAR_IN_USE	1   	    /* Variable's value currently being used.
				     * Used to avoid recursion */
#define VAR_JUNK  	4   	    /* Variable is a junk variable that
				     * should be destroyed when done with
				     * it. Used by Var_Parse for undefined,
				     * modified variables */
    char          name[1];	/* the variable's name */
}  Var;

static struct hash_info var_info = { 
	offsetof(Var, name), 
    NULL, hash_alloc, hash_free, element_alloc };
static int quick_lookup __P((const char *, const char **, u_int32_t *));
#define VarValue(v)	Buf_Retrieve(&((v)->val))
static Var *varfind __P((const char *, const char *, SymTable *, int, int, u_int32_t));
static Var *VarFind_interval __P((const char *, const char *, SymTable *, int));
#define VarFind(n, ctxt, flags)	VarFind_interval(n, NULL, ctxt, flags)
static Var *VarAdd __P((const char *, const char *, GSymT *));
static void VarDelete __P((void *));
static void VarPrintVar __P((void *));
static const char *context_name __P((GSymT *));
static Var *new_var __P((const char *, const char *));
static Var *getvar __P((GSymT *, const char *, const char *, u_int32_t));
static Var *var_name_with_dollar __P((char *, char **, SymTable *, Boolean, char));

void
SymTable_Init(ctxt)
    SymTable	*ctxt;
{
    static SymTable sym_template;
    memcpy(ctxt, &sym_template, sizeof(*ctxt));
}

void
SymTable_Destroy(ctxt)
    SymTable	*ctxt;
{
    int i;

    for (i = 0; i < LOCAL_SIZE; i++)
	if (ctxt->locals[i] != NULL)
	    VarDelete(ctxt->locals[i]);
}

static int
quick_lookup(name, end, pk)
    const char *name;
    const char **end;
    u_int32_t *pk;
{
    size_t len;

    *pk = hash_interval(name, end);
    len = *end - name;
	/* substitute short version for long local name */
    switch (*pk % MAGICSLOTS) {		    /* MAGICSLOTS should be the    */
    case K_LONGALLSRC % MAGICSLOTS:	    /* smallest constant yielding  */
					    /* distinct case values        */
	if (*pk == K_LONGALLSRC && strncmp(name, LONGALLSRC, len) == 0 &&
	    len == strlen(LONGALLSRC))
	    return ALLSRC_INDEX;
	break;
    case K_LONGARCHIVE % MAGICSLOTS:
	if (*pk == K_LONGARCHIVE && strncmp(name, LONGARCHIVE, len) == 0 &&
	    len == strlen(LONGARCHIVE))
	    return ARCHIVE_INDEX;
	break;
    case K_LONGIMPSRC % MAGICSLOTS:
	if (*pk == K_LONGIMPSRC && strncmp(name, LONGIMPSRC, len) == 0 &&
	    len == strlen(LONGIMPSRC))
	    return IMPSRC_INDEX;
	break;
    case K_LONGMEMBER % MAGICSLOTS:
	if (*pk == K_LONGMEMBER && strncmp(name, LONGMEMBER, len) == 0 &&
	    len == strlen(LONGMEMBER))
	    return MEMBER_INDEX;
	break;
    case K_LONGOODATE % MAGICSLOTS:
	if (*pk == K_LONGOODATE && strncmp(name, LONGOODATE, len) == 0 &&
	    len == strlen(LONGOODATE))
	    return OODATE_INDEX;
	break;
    case K_LONGPREFIX % MAGICSLOTS:
	if (*pk == K_LONGPREFIX && strncmp(name, LONGPREFIX, len) == 0 &&
	    len == strlen(LONGPREFIX))
	    return PREFIX_INDEX;
	break;
    case K_LONGTARGET % MAGICSLOTS:
	if (*pk == K_LONGTARGET && strncmp(name, LONGTARGET, len) == 0 &&
	    len == strlen(LONGTARGET))
	    return TARGET_INDEX;
	break;
    case K_TARGET % MAGICSLOTS:
	if (name[0] == TARGET[0] && len == 1)
	    return TARGET_INDEX;
	break;
    case K_OODATE % MAGICSLOTS:
	if (name[0] == OODATE[0] && len == 1)
	    return OODATE_INDEX;
	break;
    case K_ALLSRC % MAGICSLOTS:
	if (name[0] == ALLSRC[0] && len == 1)
	    return ALLSRC_INDEX;
	break;
    case K_IMPSRC % MAGICSLOTS:
	if (name[0] == IMPSRC[0] && len == 1)
	    return IMPSRC_INDEX;
	break;
    case K_PREFIX % MAGICSLOTS:
	if (name[0] == PREFIX[0] && len == 1)
	    return PREFIX_INDEX;
	break;
    case K_ARCHIVE % MAGICSLOTS:
	if (name[0] == ARCHIVE[0] && len == 1)
	    return ARCHIVE_INDEX;
	break;
    case K_MEMBER % MAGICSLOTS:
	if (name[0] == MEMBER[0] && len == 1)
	    return MEMBER_INDEX;
	break;
    default:
	break;
    }
    return -1;
}

void 
Varq_Set(idx, val, gn)
    int 	idx;
    char 	*val;
    GNode 	*gn;
{
    /* We only look for a variable in the given context since anything set
     * here will override anything in a lower context, so there's not much
     * point in searching them all just to save a bit of memory...  */
    Var *v = gn->context.locals[idx];

    if (v == NULL) {
    	v = new_var(varnames[idx], val);
	v->flags = 0;
	gn->context.locals[idx] = v;
    } else {
	Buf_Reset(&(v->val));
	Buf_AddString(&(v->val), val);

    }
    if (DEBUG(VAR))
	printf("%s:%s = %s\n", gn->name, varnames[idx], val);
}

void 
Varq_Append(idx, val, gn)
    int		idx;
    char	*val;
    GNode	*gn;
{
    Var *v = gn->context.locals[idx];

    if (v == NULL) {
    	v = new_var(varnames[idx], val);
	v->flags = 0;
	gn->context.locals[idx] = v;
    } else {
	Buf_AddSpace(&(v->val));
	Buf_AddString(&(v->val), val);
    }
    if (DEBUG(VAR))
	printf("%s:%s = %s\n", gn->name, varnames[idx], VarValue(v));
}

char *
Varq_Value(idx, gn)
    int		idx;
    GNode	*gn;
{
    Var *v = gn->context.locals[idx];

    if (v != NULL)
    	return VarValue(v);
    else
    	return NULL;
}

Boolean
Varq_Exists(idx, gn)
    int		idx;
    GNode	*gn;
{
    return gn->context.locals[idx] != NULL;
}


static const char *
context_name(ctxt)
    GSymT *ctxt;
{
    if (ctxt == VAR_GLOBAL)
    	return "Global";
    else if (ctxt == VAR_CMD)
    	return "Command";
    else
    	return "Environment";
}
    
/* Create a variable, to pass to VarAdd.  */
static Var *
new_var(name, val)
    const char *name;
    const char *val;
{
    Var *v;
    const char *end = NULL;

    v = hash_create_entry(&var_info, name, &end);

    if (val != NULL) {
    	size_t len = strlen(val);
	Buf_Init(&(v->val), len+1);
	Buf_AddChars(&(v->val), len, val);
    } else
    	Buf_Init(&(v->val), 1);

    return v;
}

static Var *
getvar(ctxt, name, end, k)
    GSymT	*ctxt;
    const char 	*name;
    const char	*end;
    u_int32_t	k;
{
    return hash_find(ctxt, hash_lookup_interval(ctxt, name, end, k));
}

/*-
 *-----------------------------------------------------------------------
 * VarFind_interval --
 *	Find the given variable in the given context and any other contexts
 *	indicated.  if end is NULL, name is a string, otherwise, only
 *      the interval name - end  is concerned.
 *
 * Results:
 *	A pointer to the structure describing the desired variable or
 *	NULL if the variable does not exist.
 *
 * Side Effects:
 *	Caches env variables in the VAR_ENV context.
 *-----------------------------------------------------------------------
 */
static Var *
VarFind_interval(name, end, ctxt, flags)
    const char          *name;	/* name to find */
    const char		*end;	/* end of name */
    SymTable          	*ctxt;	/* context in which to find it */
    int             	flags;	/* FIND_MINE set means to look in the
				 * CTXT_GLOBAL and CTXT_CMD contexts also.
				 * FIND_ENV set means to look in the
				 * environment */
{
    int 		idx;
    u_int32_t		k;

    idx = quick_lookup(name, &end, &k);
    return varfind(name, end, ctxt, flags, idx, k);
}

static Var *
varfind(name, end, ctxt, flags, idx, k)
    const char		*name;
    const char		*end;
    SymTable		*ctxt;
    int			flags;
    int			idx;
    u_int32_t		k;
{
    Var			*v;

    /*
     * First look for the variable in the given context. If it's not there,
     * look for it in VAR_CMD, VAR_GLOBAL and the environment, in that order,
     * depending on the FIND_* flags in 'flags'
     */
    if (ctxt == NULL)
    	v = NULL;
    else if (ctxt == CTXT_GLOBAL || ctxt == CTXT_CMD || ctxt == CTXT_ENV)
	v = getvar((GSymT *)ctxt, name, end, k);
    else {
    	if (idx == -1)
	    v = NULL;
	else
	    v = ctxt->locals[idx];
    }
    if (v != NULL)
    	return v;
	    
    if ((flags & FIND_MINE) && ctxt != CTXT_CMD)
	v = getvar(VAR_CMD, name, end, k);
    if (v != NULL)
    	return v;

    if (!checkEnvFirst && (flags & FIND_MINE) && ctxt != CTXT_GLOBAL)
	v = getvar(VAR_GLOBAL, name, end, k);
    if (v != NULL)
    	return v;

    if ((flags & FIND_ENV)) {
	char *env;
	char *n;

    	v = getvar(VAR_ENV, name, end, k);
	if (v != NULL)
	    return v;

	/* getenv requires a null-terminated name */
	n = interval_dup(name, end);
	env = getenv(n);
	free(n);
	if (env != NULL)
	    return VarAdd(name, env, VAR_ENV);
    }

    if (checkEnvFirst && (flags & FIND_MINE) && ctxt != CTXT_GLOBAL) 
	v = getvar(VAR_GLOBAL, name, end, k);
    return v;
}

/*-
 *-----------------------------------------------------------------------
 * VarAdd  --
 *	Add a new variable of name name and value val to the given context
 *
 * Results:
 *	The added variable
 *
 * Side Effects:
 *	The new variable is placed at the front of the given context
 *	The name and val arguments are duplicated so they may
 *	safely be freed.
 *-----------------------------------------------------------------------
 */
static Var *
VarAdd(name, val, ctxt)
    const char	*name;	/* name of variable to add */
    const char	*val;	/* value to set it to */
    GSymT	*ctxt;	/* context in which to set it */
{
    Var   	*v;
    const char 	*end = NULL;
    int 	idx;
    u_int32_t 	k;

    v = new_var(name, val);

    v->flags = 0;

    idx = quick_lookup(name, &end, &k);

    if (idx != -1) {
    	Parse_Error(PARSE_FATAL, "Trying to set dynamic variable %s",
	    v->name);
    } else
	hash_insert(ctxt, hash_lookup_interval(ctxt, name, end, k), v);
    return v;
}


/*-
 *-----------------------------------------------------------------------
 * VarDelete  --
 *	Delete a variable and all the space associated with it.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static void
VarDelete(vp)
    void *vp;
{
    Var *v = (Var *) vp;
    Buf_Destroy(&(v->val));
    free(v);
}



/*-
 *-----------------------------------------------------------------------
 * Var_Delete --
 *	Remove a variable from a context.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The Var structure is removed and freed.
 *
 *-----------------------------------------------------------------------
 */
void
Var_Delete(name, ctxt)
    char    	  *name;
    GSymT	  *ctxt;
{
    Var *v;
    u_int32_t k;
    const char *end = NULL;

    if (DEBUG(VAR))
	printf("%s:delete %s\n", context_name(ctxt), name);
    (void)quick_lookup(name, &end, &k);
    v = hash_remove(ctxt, hash_lookup_interval(ctxt, name, end, k));

    if (v != NULL)
	VarDelete(v);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Set --
 *	Set the variable name to the value val in the given context.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	If the variable doesn't yet exist, a new record is created for it.
 *	Else the old value is freed and the new one stuck in its place
 *
 * Notes:
 *	The variable is searched for only in its context before being
 *	created in that context. I.e. if the context is VAR_GLOBAL,
 *	only VAR_GLOBAL->context is searched. Likewise if it is VAR_CMD, only
 *	VAR_CMD->context is searched. This is done to avoid the literally
 *	thousands of unnecessary strcmp's that used to be done to
 *	set, say, $(@) or $(<).
 *-----------------------------------------------------------------------
 */
void
Var_Set(name, val, ctxt)
    char	*name;	/* name of variable to set */
    char	*val;	/* value to give to the variable */
    GSymT       *ctxt;	/* context in which to set it */
{
    register Var   *v;

    /*
     * We only look for a variable in the given context since anything set
     * here will override anything in a lower context, so there's not much
     * point in searching them all just to save a bit of memory...
     */
    v = VarFind(name, (SymTable *)ctxt, 0);
    if (v == NULL)
	(void)VarAdd(name, val, ctxt);
    else {
	Buf_Reset(&(v->val));
	Buf_AddString(&(v->val), val);

    }
    if (DEBUG(VAR))
	printf("%s:%s = %s\n", context_name(ctxt), name, val);
    /*
     * Any variables given on the command line are automatically exported
     * to the environment (as per POSIX standard).  
     * We put them into the env cache directly.
     * (Note that additions to VAR_CMD occur very early, so VAR_ENV is
     * actually empty at this point).
     */
    if (ctxt == VAR_CMD) {
	setenv(name, val, 1);
	(void)VarAdd(name, val, VAR_ENV);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Var_Append --
 *	The variable of the given name has the given value appended to it in
 *	the given context.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	If the variable doesn't exist, it is created. Else the strings
 *	are concatenated (with a space in between).
 *
 * Notes:
 *	Only if the variable is being sought in the global context is the
 *	environment searched.
 *	XXX: Knows its calling circumstances in that if called with ctxt
 *	an actual target, it will only search that context since only
 *	a local variable could be being appended to. This is actually
 *	a big win and must be tolerated.
 *-----------------------------------------------------------------------
 */
void
Var_Append(name, val, ctxt)
    char	*name;	/* Name of variable to modify */
    char	*val;	/* String to append to it */
    GSymT	*ctxt;	/* Context in which this should occur */
{
    register Var   *v;

    v = VarFind(name, (SymTable *)ctxt, (ctxt == VAR_GLOBAL) ? FIND_ENV : 0);

    if (v == NULL) {
	(void)VarAdd(name, val, ctxt);
    } else {
	Buf_AddSpace(&(v->val));
	Buf_AddString(&(v->val), val);


    }
    if (DEBUG(VAR))
	printf("%s:%s = %s\n", context_name(ctxt), name, VarValue(v));
}

/*-
 *-----------------------------------------------------------------------
 * Var_Exists --
 *	See if the given variable exists.
 *
 * Results:
 *	TRUE if it does, FALSE if it doesn't
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
Boolean
Var_Exists(name, ctxt)
    char	*name;    	/* Variable to find */
    GSymT	*ctxt;    	/* Context in which to start search */
{
    Var	    	  *v;

    v = VarFind(name, (SymTable *)ctxt, FIND_MINE|FIND_ENV);

    if (v == NULL)
	return FALSE;
    else
	return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * Var_Value --
 *	Return the value of the named variable in the given context
 *
 * Results:
 *	The value if the variable exists, NULL if it doesn't
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
char *
Var_Value(name, ctxt)
    char	*name;	/* name to find */
    GSymT	*ctxt;	/* context in which to search for it */
{
    Var            *v;

    v = VarFind(name, (SymTable *)ctxt, FIND_ENV | FIND_MINE);
    if (v != NULL) 
	return VarValue(v);
    else
	return NULL;
}

/*
 *-----------------------------------------------------------------------
 * v = var_name_with_dollar(str, &pos, ctxt, err, endc)
 *   handle variable name that contains a dollar
 *   str points to the first letter of the variable name,
 *   &pos to the current position in the name (first dollar at invocation,
 *   end of name specification at return).
 *   returns the corresponding variable.
 *-----------------------------------------------------------------------
 */
static Var *
var_name_with_dollar(str, pos, ctxt, err, endc)
    char *str;			/* First dollar in variable name */
    char **pos;			/* Current position in variable spec */
    SymTable   	*ctxt;    	/* The context for the variable */
    Boolean 	err;    	/* TRUE if undefined variables are an error */
    char	endc;		/* End character for spec */
{
    BUFFER buf;			/* Store the variable name */
    size_t sublen;		/* Deal with recursive expansions */
    Boolean subfree;		
    char *n; 			/* Sub name */
    Var *v;

    Buf_Init(&buf, MAKE_BSIZE);

    while (1) {
	Buf_AddInterval(&buf, str, *pos);
	n = Var_Parse(*pos, ctxt, err, &sublen, &subfree);
	if (n != NULL)
	    Buf_AddString(&buf, n);
	if (subfree)
	    free(n);
	*pos += sublen;
	str = *pos;
	for (; **pos != '$'; (*pos)++) {
	    if (**pos == '\0' || **pos == endc || **pos == ':') {
		v = VarFind(Buf_Retrieve(&buf), ctxt, FIND_ENV | FIND_MINE);
		Buf_Destroy(&buf);
		return v;
	    }
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * Var_Parse --
 *	Given the start of a variable invocation, extract the variable
 *	name and find its value, then modify it according to the
 *	specification.
 *
 * Results:
 *	The (possibly-modified) value of the variable or var_Error if the
 *	specification is invalid. The length of the specification is
 *	placed in *lengthPtr (for invalid specifications, this is just
 *	2...?).
 *	A Boolean in *freePtr telling whether the returned string should
 *	be freed by the caller.
 *-----------------------------------------------------------------------
 */
char *
Var_Parse(str, ctxt, err, lengthPtr, freePtr)
    char    	*str;	    	/* The string to parse */
    SymTable   	*ctxt;    	/* The context for the variable */
    Boolean 	err;    	/* TRUE if undefined variables are an error */
    size_t	*lengthPtr;	/* OUT: The length of the specification */
    Boolean 	*freePtr; 	/* OUT: TRUE if caller should free result */
{
    char  	*tstr;    	/* Pointer into str */
    Var	    	*v;	    	/* Variable in invocation */
    char   	endc;    	/* Ending character when variable in parens
				 * or braces */
    char    	*start;
    char	*val;		/* Variable value  */
    u_int32_t	k;
    int		idx;

    *freePtr = FALSE;
    start = str++;

    val = NULL;
    v = NULL;
    idx = 0;

    if (*str != '(' && *str != '{') {
    	tstr = str + 1;
	*lengthPtr = 2;
	endc = '\0';
    } else {
	endc = *str == '(' ? ')' : '}';
    	str++;

	/* Find eventual modifiers in the variable */
	for (tstr = str; *tstr != ':'; tstr++) {
	    if (*tstr == '$') {
	    	v = var_name_with_dollar(str, &tstr, ctxt, err, endc);
	    	if (*tstr == '\0' || *tstr == endc)
		    endc = '\0';
		break;
	    } else if (*tstr == '\0' || *tstr == endc) {
	    	endc = '\0';
		break;
	    }
	}
	*lengthPtr = tstr+1 - start;
    }

    if (v == NULL) {
	idx = quick_lookup(str, &tstr, &k);
	v = varfind(str, tstr, ctxt, FIND_ENV | FIND_MINE, idx, k);
    }
    if (v == NULL) {
    	/* Find out about D and F forms of local variables. */
    	if (idx == -1 && tstr == str+2 && (str[1] == 'D' || str[1] == 'F')) {
	    switch (*str) {
	    case '@':
	    	idx = TARGET_INDEX;
		break;
	    case '!':
	    	idx = ARCHIVE_INDEX;
		break;
	    case '*':
	    	idx = PREFIX_INDEX;
		break;
	    case '%':
	    	idx = MEMBER_INDEX;
		break;
	    default:
	    	break;
	    }
	    /* This is a DF form, check if we can expand it now.  */
	    if (idx != -1 && ctxt != NULL && ctxt != CTXT_GLOBAL) {
	    	v = varfind(str, str+1, ctxt, 0, idx, 0);
		/* No need for nested expansion or anything, as we're
		 * the only one who sets these things and we sure don't
		 * do nested invocations in them...  */
		if (v != NULL) {
		    val = VarValue(v);
		    if (str[1] == 'D')
			val = Var_GetHead(val);
		    else
			val = Var_GetTail(val);
		    *freePtr = TRUE;
		}
	    }
	}
    } else {
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
	if (strchr(val, '$') != NULL) {
	    val = Var_Subst(val, ctxt, err);
	    *freePtr = TRUE;
	}

	v->flags &= ~VAR_IN_USE;
    }
    if (endc != '\0')
	val = VarModifiers_Apply(val, ctxt, err, freePtr, tstr+1, endc, 
	    lengthPtr);
    if (val == NULL) {
    	/* Dynamic source that can't be expanded for now: copy the var
	 * specification instead.  */
    	if (idx != -1 && (ctxt == NULL || ctxt == CTXT_GLOBAL)) {
	    *freePtr = TRUE;
	    val = interval_dup(start, start+ *lengthPtr);
	} else 
	    val = err ? var_Error : varNoError;
    }

    return val;
}

/*-
 *-----------------------------------------------------------------------
 * Var_Subst  --
 *	Substitute for all variables in a string in a context
 *	If undefErr is TRUE, Parse_Error will be called when an undefined
 *	variable is encountered.
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None. The old string must be freed by the caller
 *-----------------------------------------------------------------------
 */
char *
Var_Subst(str, ctxt, undefErr)
    char 	  *str;	    	    /* the string in which to substitute */
    SymTable      *ctxt;	    /* the context wherein to find variables */
    Boolean 	  undefErr; 	    /* TRUE if undefineds are an error */
{
    BUFFER 	  buf;	    	    /* Buffer for forming things */
    static Boolean errorReported;   /* Set true if an error has already
				     * been reported to prevent a plethora
				     * of messages when recursing */

    Buf_Init(&buf, MAKE_BSIZE);
    errorReported = FALSE;

    for (;;) {
	char		*val;		/* Value to substitute for a variable */
	size_t		length;		/* Length of the variable invocation */
	Boolean		doFree;		/* Set true if val should be freed */
	const char *cp;

	/* copy uninteresting stuff */
	for (cp = str; *str != '\0' && *str != '$'; str++)
	    ;
	Buf_AddInterval(&buf, cp, str);
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
	    if (oldVars) {
		str += length;
	    } else if (undefErr) {
		/* If variable is undefined, complain and skip the
		 * variable. The complaint will stop us from doing anything
		 * when the file is parsed.  */
		if (!errorReported) {
		    Parse_Error(PARSE_FATAL,
				 "Undefined variable \"%.*s\"",length,str);
		}
		str += length;
		errorReported = TRUE;
	    } else {
		Buf_AddChar(&buf, *str);
		str += 1;
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

/*-
 *-----------------------------------------------------------------------
 * Var_SubstVar  --
 *	Substitute for one variable in the given string in the given context
 *	If undefErr is TRUE, Parse_Error will be called when an undefined
 *	variable is encountered.
 *
 * Side Effects:
 *	Append the result to the buffer
 *-----------------------------------------------------------------------
 */
void
Var_SubstVar(buf, str, var, ctxt)
    Buffer	buf;		/* Where to store the result */
    char	*str;	        /* The string in which to substitute */
    const char	*var;		/* Named variable */
    GSymT	*ctxt;		/* The context wherein to find variables */
{
    char	*val;		/* Value substituted for a variable */
    size_t	length;		/* Length of the variable invocation */
    Boolean	doFree;		/* Set true if val should be freed */

    for (;;) {
	const char *cp;
	/* copy uninteresting stuff */
	for (cp = str; *str != '\0' && *str != '$'; str++)
	    ;
	Buf_AddInterval(buf, cp, str);
	if (*str == '\0')
	    break;
	if (str[1] == '$') {
	    Buf_AddString(buf, "$$");
	    str += 2;
	    continue;
	}
	if (str[1] != '(' && str[1] != '{') {
	    if (str[1] != *var || var[1] != '\0') {
		Buf_AddChars(buf, 2, str);
		str += 2;
		continue;
	    }
	} else {
	    char *p;
	    char endc;

	    if (str[1] == '(')
		endc = ')';
	    else if (str[1] == '{')
		endc = '}';

	    /* Find the end of the variable specification.  */
	    p = str+2;
	    while (*p != '\0' && *p != ':' && *p != endc && *p != '$')
		p++;
	    /* A variable inside the variable.  We don't know how to
	     * expand the external variable at this point, so we try 
	     * again with the nested variable.  */
	    if (*p == '$') {
		Buf_AddInterval(buf, str, p);
		str = p;
		continue;
	    }

	    if (strncmp(var, str + 2, p - str - 2) != 0 ||
		var[p - str - 2] != '\0') {
		/* Not the variable we want to expand.  */
		Buf_AddInterval(buf, str, p);
		str = p;
		continue;
	    } 
	}
	/* okay, so we've found the variable we want to expand.  */
	val = Var_Parse(str, (SymTable *)ctxt, FALSE, &length, &doFree);
	/* We've now got a variable structure to store in. But first,
	 * advance the string pointer.  */
	str += length;

	/* Copy all the characters from the variable value straight
	 * into the new string.  */
	Buf_AddString(buf, val);
	if (doFree)
	    free(val);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Var_Init --
 *	Initialize the module
 *
 * Side Effects:
 *	The VAR_CMD and VAR_GLOBAL contexts are created
 *-----------------------------------------------------------------------
 */
void
Var_Init()
{
    static GSymT global_vars, cmd_vars, env_vars;

    VAR_GLOBAL = &global_vars;
    VAR_CMD = &cmd_vars;
    VAR_ENV = &env_vars;
    hash_init(VAR_GLOBAL, 10, &var_info);
    hash_init(VAR_CMD, 5, &var_info);
    hash_init(VAR_ENV, 5, &var_info);
    CTXT_GLOBAL = (SymTable *)VAR_GLOBAL;
    CTXT_CMD = (SymTable *)VAR_CMD;
    CTXT_ENV = (SymTable *)VAR_ENV;
}


void
Var_End()
{
#ifdef CLEANUP
    Var *v;
    unsigned int i;

    for (v = hash_first(VAR_GLOBAL, &i); v != NULL; 
	v = hash_next(VAR_GLOBAL, &i))
	    VarDelete(v);
    for (v = hash_first(VAR_CMD, &i); v != NULL; 
	v = hash_next(VAR_CMD, &i))
	    VarDelete(v);
    for (v = hash_first(VAR_ENV, &i); v != NULL; 
	v = hash_next(VAR_ENV, &i))
	    VarDelete(v);
#endif
}


/****************** PRINT DEBUGGING INFO *****************/
static void
VarPrintVar(vp)
    void *vp;
{
    Var    *v = (Var *)vp;

    printf("%-16s = %s\n", v->name, VarValue(v));
}

/*-
 *-----------------------------------------------------------------------
 * Var_Dump --
 *	print all variables in a context
 *-----------------------------------------------------------------------
 */
void
Var_Dump(ctxt)
   GSymT	*ctxt;
{
	Var *v;
	unsigned int i;

	for (v = hash_first(ctxt, &i); v != NULL; 
	    v = hash_next(ctxt, &i))
		VarPrintVar(v);
}


#ifdef POSIX

static const char *quotable = " \t\n\\'\"";

/* In POSIX mode, variable assignments passed on the command line are
 * propagated to sub makes through MAKEFLAGS.
 */
void
Var_AddCmdline(name)
	const char *name;
{
    Var *v;
    unsigned int i;
    BUFFER buf;
    char *s;

    Buf_Init(&buf, MAKE_BSIZE);

    for (v = hash_first(VAR_CMD, &i); v != NULL; 
    	v = hash_next(VAR_CMD, &i)) {
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
#endif
