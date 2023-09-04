/*	$OpenBSD: parsevar.c,v 1.17 2023/09/04 11:35:11 espie Exp $	*/
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
		if (ISSPACE(*p) || *p == '$' || *p == '\0')
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
		if (ISSPACE(*p) || *p == '$' || *p == '\0')
			break;
		if (p[strspn(p, "?:!+")] == '=')
			break;
	}
	return p;
}

static bool
parse_variable_assignment(const char *line, int ctxt)
{
	const char *arg;
	char *res1 = NULL, *res2 = NULL;
#define VAR_INVALID	-1
#define VAR_NORMAL	0
#define VAR_SUBST	1
#define VAR_APPEND	2
#define VAR_SHELL	4
#define VAR_OPT		8
#define VAR_LAZYSHELL	16
#define VAR_SUNSHELL	32
	int type;
	struct Name name;

	arg = VarName_Get(line, &name, NULL, true, find_op1);

	while (ISSPACE(*arg))
		arg++;

	type = VAR_NORMAL;

	/* double operators (except for :) are forbidden */
	/* OPT and APPEND don't match */
	/* APPEND and LAZYSHELL can't really work */
	while (*arg != '=') {
		/* Check operator type.  */
		switch (*arg++) {
		case '+':
			if (type & (VAR_OPT|VAR_LAZYSHELL|VAR_APPEND))
				type = VAR_INVALID;
			else
				type |= VAR_APPEND;
			break;

		case '?':
			if (type & (VAR_OPT|VAR_APPEND))
				type = VAR_INVALID;
			else
				type |= VAR_OPT;
			break;

		case ':':
			if (strncmp(arg, "sh", 2) == 0) {
				type = VAR_SUNSHELL;
				arg += 2;
				while (*arg != '=' && *arg != '\0')
					arg++;
			} else {
				if (type & VAR_SUBST)
					type = VAR_INVALID;
				else
					type |= VAR_SUBST;
			}
			break;

		case '!':
			if (type & VAR_SHELL) {
				if (type & (VAR_APPEND))
					type = VAR_INVALID;
				else
					type = VAR_LAZYSHELL;
			} else if (type & (VAR_LAZYSHELL|VAR_SUNSHELL))
				type = VAR_INVALID;
			else
				type |= VAR_SHELL;
			break;

		default:
			type = VAR_INVALID;
			break;
		}
		if (type == VAR_INVALID) {
			VarName_Free(&name);
			return false;
		}
	}

	arg++;
	while (ISSPACE(*arg))
		arg++;
	/* If the variable already has a value, we don't do anything.  */
	if ((type & VAR_OPT) && Var_Definedi(name.s, name.e)) {
		VarName_Free(&name);
		return true;
	}
	if (type & (VAR_SHELL|VAR_SUNSHELL)) {
		char *err;

		if (strchr(arg, '$') != NULL) {
			char *sub;
			/* There's a dollar sign in the command, so perform
			 * variable expansion on the whole thing. */
			sub = Var_Subst(arg, NULL, true);
			res1 = Cmd_Exec(sub, &err);
			free(sub);
		} else
			res1 = Cmd_Exec(arg, &err);

		if (err)
			Parse_Error(PARSE_WARNING, err, arg);
		arg = res1;
	}
	if (type & VAR_LAZYSHELL) {
		if (strchr(arg, '$') != NULL) {
			/* There's a dollar sign in the command, so perform
			 * variable expansion on the whole thing. */
			arg = Var_Subst(arg, NULL, true);
		}
	}
	if (type & VAR_SUBST) {
		/*
		 * Allow variables in the old value to be undefined, but leave
		 * their invocation alone -- this is done by forcing
		 * errorIsOkay to be false.
		 * XXX: This can cause recursive variables, but that's not
		 * hard to do, and this allows someone to do something like
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
		if (!Var_Definedi(name.s, name.e))
			Var_Seti_with_ctxt(name.s, name.e, "", ctxt);

		res2 = Var_Subst(arg, NULL, false);
		errorIsOkay = saved;

		arg = res2;
	}

	if (type & VAR_APPEND)
		Var_Appendi_with_ctxt(name.s, name.e, arg, ctxt);
	else
		Var_Seti_with_ctxt(name.s, name.e, arg, ctxt);
	if (type & VAR_LAZYSHELL)
		Var_Mark(name.s, name.e, VAR_EXEC_LATER);

	VarName_Free(&name);
	free(res2);
	free(res1);
	return true;
}

bool
Parse_As_Var_Assignment(const char *line)
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
	errorIsOkay = saved;
	return result;
}

