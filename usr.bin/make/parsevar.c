/*	$OpenPackages$ */
/*	$OpenBSD: parsevar.c,v 1.4 2007/07/08 17:53:15 espie Exp $	*/
/*	$NetBSD: parse.c,v 1.29 1997/03/10 21:20:04 christos Exp $	*/

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

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "var.h"
#include "varname.h"
#include "error.h"
#include "cmd_exec.h"
#include "parsevar.h"

static const char *find_op1(const char *);
static const char *find_op2(const char *);
static bool parse_variable_assignment(const char *, int);

static const char *
find_op1(const char *p)
{
    for(;; p++) {
    	if (isspace(*p) || *p == '$' || *p == '\0')
	    break;
	if (p[strspn(p, "?:!+")] == '=')
	    break;
	if (p[0] == ':' && p[1] == 's' && p[2] == 'h')
	    break;
    }
    return p;
}

static const char *
find_op2(const char *p)
{
    for(;; p++) {
    	if (isspace(*p) || *p == '$' || *p == '\0')
	    break;
	if (p[strspn(p, "?:!+")] == '=')
	    break;
    }
    return p;
}

static bool
parse_variable_assignment(const char *line, 
    int ctxt) 			/* Context in which to do the assignment */
{
    const char	*arg;
    char	*res1 = NULL, *res2 = NULL;
#define VAR_NORMAL	0
#define VAR_SUBST	1
#define VAR_APPEND	2
#define VAR_SHELL	4
#define VAR_OPT		8
    int		    type;	/* Type of assignment */
    struct Name	    name;

    arg = VarName_Get(line, &name, NULL, true,
	FEATURES(FEATURE_SUNSHCMD) ? find_op1 : find_op2);

    while (isspace(*arg))
    	arg++;

    type = VAR_NORMAL;

    while (*arg != '=' && !isspace(*arg)) {
	/* Check operator type.  */
	switch (*arg++) {
	case '+':
	    if (type & (VAR_OPT|VAR_APPEND)) {
	    	VarName_Free(&name);
		return false;
	    }
	    type |= VAR_APPEND;
	    break;

	case '?':
	    if (type & (VAR_OPT|VAR_APPEND)) {
	    	VarName_Free(&name);
		return false;
	    }
	    type |= VAR_OPT;
	    break;

	case ':':
	    if (FEATURES(FEATURE_SUNSHCMD) && strncmp(arg, "sh", 2) == 0) {
		type = VAR_SHELL;
		arg += 2;
		while (*arg != '=' && *arg != '\0')
			arg++;
	    } else {
	    	if (type & VAR_SUBST) {
		    VarName_Free(&name);
		    return false;
		}
		type |= VAR_SUBST;
	    }
	    break;

	case '!':
	    if (type & VAR_SHELL) {
	    	VarName_Free(&name);
		return false;
	    }
	    type |= VAR_SHELL;
	    break;

	default:
	    VarName_Free(&name);
	    return false;
	}
    }

    /* Check validity of operator */
    if (*arg++ != '=') {
	VarName_Free(&name);
	return false;
    }

    while (isspace(*arg))
    	arg++;
    /* If the variable already has a value, we don't do anything.  */
    if ((type & VAR_OPT) && Var_Definedi(name.s, name.e)) {
	VarName_Free(&name);
	return true;
    }
    if (type & VAR_SHELL) {
	char *err;

	if (strchr(arg, '$') != NULL) {
	    char *sub;
	    /* There's a dollar sign in the command, so perform variable
	     * expansion on the whole thing. */
	    sub = Var_Subst(arg, NULL, true);
	    res1 = Cmd_Exec(sub, &err);
	    free(sub);
	} else
	    res1 = Cmd_Exec(arg, &err);

	if (err)
	    Parse_Error(PARSE_WARNING, err, arg);
	arg = res1;
    }
    if (type & VAR_SUBST) {
	/*
	 * Allow variables in the old value to be undefined, but leave their
	 * invocation alone -- this is done by forcing errorIsOkay to be false.
	 * XXX: This can cause recursive variables, but that's not hard to do,
	 * and this allows someone to do something like
	 *
	 *  CFLAGS = $(.INCLUDES)
	 *  CFLAGS := -I.. $(CFLAGS)
	 *
	 * And not get an error.
	 */
	bool   saved = errorIsOkay;

	errorIsOkay = false;
	/* ensure the variable is set to something to avoid `variable
	 * is recursive' errors.  */
	if (Var_Valuei(name.s, name.e) == NULL)
	    Var_Seti(name.s, name.e, "", ctxt);

	res2 = Var_Subst(arg, NULL, false);
	errorIsOkay = saved;

	arg = res2;
    }

    if (type & VAR_APPEND)
	Var_Appendi(name.s, name.e, arg, ctxt);
    else
	Var_Seti(name.s, name.e, arg, ctxt);

    VarName_Free(&name);
    free(res2);
    free(res1);
    return true;
}

bool
Parse_DoVar(const char *line)
{
	return parse_variable_assignment(line, VAR_GLOBAL);
}

bool
Parse_CmdlineVar(const char *line)
{
	bool result;
	bool saved = errorIsOkay;

	errorIsOkay = false;
	result = parse_variable_assignment(line, VAR_CMD);
	errorIsOkay = errorIsOkay;
	return result;
}

