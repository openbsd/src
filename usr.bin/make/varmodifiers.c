/*	$OpenPackages$ */
/*	$OpenBSD: varmodifiers.c,v 1.24 2007/09/17 09:44:20 espie Exp $	*/
/*	$NetBSD: var.c,v 1.18 1997/03/18 19:24:46 christos Exp $	*/

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

/* VarModifiers_Apply is mostly a constituent function of Var_Parse, it
 * is also called directly by Var_SubstVar.  */


#include <ctype.h>
#include <sys/types.h>
#ifndef MAKE_BOOTSTRAP
#include <regex.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "buf.h"
#include "var.h"
#include "varmodifiers.h"
#include "varname.h"
#include "targ.h"
#include "error.h"
#include "str.h"
#include "cmd_exec.h"
#include "memory.h"
#include "gnode.h"


/* Var*Pattern flags */
#define VAR_SUB_GLOBAL	0x01	/* Apply substitution globally */
#define VAR_SUB_ONE	0x02	/* Apply substitution to one word */
#define VAR_SUB_MATCHED 0x04	/* There was a match */
#define VAR_MATCH_START 0x08	/* Match at start of word */
#define VAR_MATCH_END	0x10	/* Match at end of word */

/* Modifiers flags */
#define VAR_EQUAL	0x20
#define VAR_MAY_EQUAL	0x40
#define VAR_ADD_EQUAL	0x80
#define VAR_BANG_EQUAL	0x100

typedef struct {
	char	  *lbuffer; /* left string to free */
	char	  *lhs;     /* String to match */
	size_t	  leftLen;  /* Length of string */
	char	  *rhs;     /* Replacement string (w/ &'s removed) */
	size_t	  rightLen; /* Length of replacement */
	int 	  flags;
} VarPattern;

struct LoopStuff {
	struct LoopVar	*var;
	char	*expand;
	bool	err;
};

static bool VarHead(struct Name *, bool, Buffer, void *);
static bool VarTail(struct Name *, bool, Buffer, void *);
static bool VarSuffix(struct Name *, bool, Buffer, void *);
static bool VarRoot(struct Name *, bool, Buffer, void *);
static bool VarMatch(struct Name *, bool, Buffer, void *);
static bool VarSYSVMatch(struct Name *, bool, Buffer, void *);
static bool VarNoMatch(struct Name *, bool, Buffer, void *);
static bool VarUniq(struct Name *, bool, Buffer, void *);
static bool VarLoop(struct Name *, bool, Buffer, void *);


#ifndef MAKE_BOOTSTRAP
static void VarREError(int, regex_t *, const char *);
static bool VarRESubstitute(struct Name *, bool, Buffer, void *);
static char *do_regex(const char *, const struct Name *, void *);

typedef struct {
	regex_t	  re;
	int 	  nsub;
	regmatch_t	 *matches;
	char	 *replace;
	int 	  flags;
} VarREPattern;
#endif

static bool VarSubstitute(struct Name *, bool, Buffer, void *);
static char *VarGetPattern(SymTable *, int, const char **, int, int,
    size_t *, VarPattern *);
static char *VarQuote(const char *, const struct Name *, void *);
static char *VarModify(char *, bool (*)(struct Name *, bool, Buffer, void *), void *);

static void *check_empty(const char **, SymTable *, bool, int);
static char *do_upper(const char *, const struct Name *, void *);
static char *do_lower(const char *, const struct Name *, void *);
static void *check_shcmd(const char **, SymTable *, bool, int);
static char *do_shcmd(const char *, const struct Name *, void *);
static char *do_sort(const char *, const struct Name *, void *);
static char *finish_loop(const char *, const struct Name *, void *);
static int NameCompare(const void *, const void *);
static char *do_label(const char *, const struct Name *, void *);
static char *do_path(const char *, const struct Name *, void *);
static char *do_def(const char *, const struct Name *, void *);
static char *do_undef(const char *, const struct Name *, void *);
static char *do_assign(const char *, const struct Name *, void *);
static char *do_exec(const char *, const struct Name *, void *);

static void *assign_get_value(const char **, SymTable *, bool, int);
static void *get_cmd(const char **, SymTable *, bool, int);
static void *get_value(const char **, SymTable *, bool, int);
static void *get_stringarg(const char **, SymTable *, bool, int);
static void free_stringarg(void *);
static void *get_patternarg(const char **, SymTable *, bool, int);
static void *get_spatternarg(const char **, SymTable *, bool, int);
static void *common_get_patternarg(const char **, SymTable *, bool, int, bool);
static void free_patternarg(void *);
static void free_looparg(void *);
static void *get_sysvpattern(const char **, SymTable *, bool, int);
static void *get_loop(const char **, SymTable *, bool, int);
static char *LoopGrab(const char **);

static struct Name dummy;
static struct Name *dummy_arg = &dummy;

static struct modifier {
	    bool atstart;
	    void * (*getarg)(const char **, SymTable *, bool, int);
	    char * (*apply)(const char *, const struct Name *, void *);
	    bool (*word_apply)(struct Name *, bool, Buffer, void *);
	    void   (*freearg)(void *);
} *choose_mod[256],
	match_mod = {false, get_stringarg, NULL, VarMatch, free_stringarg},
	nomatch_mod = {false, get_stringarg, NULL, VarNoMatch, free_stringarg},
	subst_mod = {false, get_spatternarg, NULL, VarSubstitute, free_patternarg},
#ifndef MAKE_BOOTSTRAP
	resubst_mod = {false, get_patternarg, do_regex, NULL, free_patternarg},
#endif
	quote_mod = {false, check_empty, VarQuote, NULL , NULL},
	tail_mod = {false, check_empty, NULL, VarTail, NULL},
	head_mod = {false, check_empty, NULL, VarHead, NULL},
	suffix_mod = {false, check_empty, NULL, VarSuffix, NULL},
	root_mod = {false, check_empty, NULL, VarRoot, NULL},
	upper_mod = {false, check_empty, do_upper, NULL, NULL},
	lower_mod = {false, check_empty, do_lower, NULL, NULL},
	shcmd_mod = {false, check_shcmd, do_shcmd, NULL, NULL},
	sysv_mod = {false, get_sysvpattern, NULL, VarSYSVMatch, free_patternarg},
	uniq_mod = {false, check_empty, NULL, VarUniq, NULL},
	sort_mod = {false, check_empty, do_sort, NULL, NULL},
	loop_mod = {false, get_loop, finish_loop, VarLoop, free_looparg},
	undef_mod = {true, get_value, do_undef, NULL, NULL},
	def_mod = {true, get_value, do_def, NULL, NULL},
	label_mod = {true, check_empty, do_label, NULL, NULL},
	path_mod = {true, check_empty, do_path, NULL, NULL},
	assign_mod = {true, assign_get_value, do_assign, NULL, free_patternarg},
	exec_mod = {true, get_cmd, do_exec, NULL, free_patternarg}
;

void
VarModifiers_Init()
{
	choose_mod['M'] = &match_mod;
	choose_mod['N'] = &nomatch_mod;
	choose_mod['S'] = &subst_mod;
#ifndef MAKE_BOOTSTRAP
	choose_mod['C'] = &resubst_mod;
#endif
	choose_mod['Q'] = &quote_mod;
	choose_mod['T'] = &tail_mod;
	choose_mod['H'] = &head_mod;
	choose_mod['E'] = &suffix_mod;
	choose_mod['R'] = &root_mod;
	if (FEATURES(FEATURE_UPPERLOWER)) {
		choose_mod['U'] = &upper_mod;
		choose_mod['L'] = &lower_mod;
	}
	if (FEATURES(FEATURE_SUNSHCMD))
		choose_mod['s'] = &shcmd_mod;
	if (FEATURES(FEATURE_UNIQ))
		choose_mod['u'] = &uniq_mod;
	if (FEATURES(FEATURE_SORT))
		choose_mod['O'] = &sort_mod;
	if (FEATURES(FEATURE_ODE)) {
		choose_mod['@'] = &loop_mod;
		choose_mod['D'] = &def_mod;
		choose_mod['U'] = &undef_mod;
		choose_mod['L'] = &label_mod;
		choose_mod['P'] = &path_mod;
	}
	if (FEATURES(FEATURE_ASSIGN))
		choose_mod[':'] = &assign_mod;
	if (FEATURES(FEATURE_EXECMOD))
		choose_mod['!'] = &exec_mod;
}

/* All modifiers handle addSpace (need to add a space before placing the
 * next word into the buffer) and propagate it when necessary.
 */

/*-
 *-----------------------------------------------------------------------
 * VarHead --
 *	Remove the tail of the given word and add the result to the given
 *	buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarHead(struct Name *word, bool addSpace, Buffer buf, void *dummy UNUSED)
{
	const char	*slash;

	slash = Str_rchri(word->s, word->e, '/');
	if (slash != NULL) {
		if (addSpace)
			Buf_AddSpace(buf);
		Buf_Addi(buf, word->s, slash);
	} else {
		/* If no directory part, give . (q.v. the POSIX standard).  */
		if (addSpace)
			Buf_AddString(buf, " .");
		else
			Buf_AddChar(buf, '.');
	}
	return true;
}

/*-
 *-----------------------------------------------------------------------
 * VarTail --
 *	Remove the head of the given word add the result to the given
 *	buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarTail(struct Name *word, bool addSpace, Buffer buf, void *dummy UNUSED)
{
	const char	*slash;

	if (addSpace)
		Buf_AddSpace(buf);
	slash = Str_rchri(word->s, word->e, '/');
	if (slash != NULL)
		Buf_Addi(buf, slash+1, word->e);
	else
		Buf_Addi(buf, word->s, word->e);
	return true;
}

/*-
 *-----------------------------------------------------------------------
 * VarSuffix --
 *	Add the suffix of the given word to the given buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarSuffix(struct Name *word, bool addSpace, Buffer buf, void *dummy UNUSED)
{
	const char	*dot;

	dot = Str_rchri(word->s, word->e, '.');
	if (dot != NULL) {
		if (addSpace)
			Buf_AddSpace(buf);
		Buf_Addi(buf, dot+1, word->e);
		addSpace = true;
	}
	return addSpace;
}

/*-
 *-----------------------------------------------------------------------
 * VarRoot --
 *	Remove the suffix of the given word and add the result to the
 *	buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarRoot(struct Name *word, bool addSpace, Buffer buf, void *dummy UNUSED)
{
	const char	*dot;

	if (addSpace)
		Buf_AddSpace(buf);
	dot = Str_rchri(word->s, word->e, '.');
	if (dot != NULL)
		Buf_Addi(buf, word->s, dot);
	else
		Buf_Addi(buf, word->s, word->e);
	return true;
}

/*-
 *-----------------------------------------------------------------------
 * VarMatch --
 *	Add the word to the buffer if it matches the given pattern.
 *-----------------------------------------------------------------------
 */
static bool
VarMatch(struct Name *word, bool addSpace, Buffer buf,
    void *pattern) /* Pattern the word must match */
{
	const char *pat = (const char *)pattern;

	if (Str_Matchi(word->s, word->e, pat, strchr(pat, '\0'))) {
		if (addSpace)
			Buf_AddSpace(buf);
		Buf_Addi(buf, word->s, word->e);
		return true;
	} else
		return addSpace;
}

/*-
 *-----------------------------------------------------------------------
 * VarNoMatch --
 *	Add the word to the buffer if it doesn't match the given pattern.
 *-----------------------------------------------------------------------
 */
static bool
VarNoMatch(struct Name *word, bool addSpace, Buffer buf,
    void *pattern) /* Pattern the word must not match */
{
	const char *pat = (const char *)pattern;

	if (!Str_Matchi(word->s, word->e, pat, strchr(pat, '\0'))) {
		if (addSpace)
			Buf_AddSpace(buf);
		Buf_Addi(buf, word->s, word->e);
		return true;
	} else
		return addSpace;
}

static bool
VarUniq(struct Name *word, bool addSpace, Buffer buf, void *lastp)
{
	struct Name *last = (struct Name *)lastp;

	/* does not match */
	if (last->s == NULL || last->e - last->s != word->e - word->s ||
	    strncmp(word->s, last->s, word->e - word->s) != 0) {
		if (addSpace)
			Buf_AddSpace(buf);
		Buf_Addi(buf, word->s, word->e);
		addSpace = true;
	}
	last->s = word->s;
	last->e = word->e;
	return addSpace;
}

static bool
VarLoop(struct Name *word, bool addSpace, Buffer buf, void *vp)
{
	struct LoopStuff *v = (struct LoopStuff *)vp;

	if (addSpace)
		Buf_AddSpace(buf);
	Var_SubstVar(buf, v->expand, v->var, word->s);
	return true;
}

static char *
finish_loop(const char *s, const struct Name *n UNUSED , void *p)
{
	struct LoopStuff *l = (struct LoopStuff *)p;

	return Var_Subst(s, NULL,  l->err);
}

static int
NameCompare(const void *ap, const void *bp)
{
	struct Name *a, *b;
	size_t n, m;
	int c;

	a = (struct Name *)ap;
	b = (struct Name *)bp;
	n = a->e - a->s;
	m = b->e - b->s;
	if (n < m) {
		c = strncmp(a->s, b->s, n);
		if (c != 0)
		    return c;
		else
		    return -1;
    	} else if (m < n) {
		c = strncmp(a->s, b->s, m);
		if (c != 0)
		    return c;
		else
		    return 1;
    	} else
	    return strncmp(a->s, b->s, n);
}

static char *
do_sort(const char *s, const struct Name *dummy UNUSED, void *arg UNUSED)
{
	struct Name *t;
	unsigned long n, i, j;
	const char *start, *end;

	n = 1024;	/* start at 1024 words */
	t = (struct Name *)emalloc(sizeof(struct Name) * n);
	start = s;
	end = start;

	for (i = 0;; i++) {
		if (i == n) {
			n *= 2;
			t = (struct Name *)erealloc(t, sizeof(struct Name) * n);
		}
		start = iterate_words(&end);
		if (start == NULL)
			break;
		t[i].s = start;
		t[i].e = end;
	}
	if (i > 0) {
		BUFFER buf;

		Buf_Init(&buf, end - s);
		qsort(t, i, sizeof(struct Name), NameCompare);
		Buf_Addi(&buf, t[0].s, t[0].e);
		for (j = 1; j < i; j++) {
			Buf_AddSpace(&buf);
			Buf_Addi(&buf, t[j].s, t[j].e);
		}
		free(t);
		return Buf_Retrieve(&buf);
	} else {
		free(t);
		return "";
	}
}

static char *
do_label(const char *s UNUSED, const struct Name *n, void *arg UNUSED)
{
	return Str_dupi(n->s, n->e);
}

static char *
do_path(const char *s UNUSED, const struct Name *n, void *arg UNUSED)
{
	GNode *gn;

	gn = Targ_FindNodei(n->s, n->e, TARG_NOCREATE);
	if (gn == NULL)
		return Str_dupi(n->s, n->e);
	else
		return strdup(gn->path);
}

static char *
do_def(const char *s, const struct Name *n UNUSED, void *arg)
{
	VarPattern *v = (VarPattern *)arg;
	if (s == NULL) {
		free_patternarg(v);
		return NULL;
	} else
		return v->lbuffer;
}

static char *
do_undef(const char *s, const struct Name *n UNUSED, void *arg)
{
	VarPattern *v = (VarPattern *)arg;
	if (s != NULL) {
		free_patternarg(v);
		return NULL;
	} else
		return v->lbuffer;
}

static char *
do_assign(const char *s, const struct Name *n, void *arg)
{
	VarPattern *v = (VarPattern *)arg;
	char *msg;
	char *result;

	switch (v->flags) {
	case VAR_EQUAL:
		Var_Seti(n->s, n->e, v->lbuffer);
		break;
	case VAR_MAY_EQUAL:
		if (s == NULL)
			Var_Seti(n->s, n->e, v->lbuffer);
		break;
	case VAR_ADD_EQUAL:
		if (s == NULL)
			Var_Seti(n->s, n->e, v->lbuffer);
		else
			Var_Appendi(n->s, n->e, v->lbuffer);
		break;
	case VAR_BANG_EQUAL:
		result = Cmd_Exec(v->lbuffer, &msg);
		if (result != NULL) {
			Var_Seti(n->s, n->e, result);
			free(result);
		} else
			Error(msg, v->lbuffer);
		break;

	}
	return NULL;
}

static char *
do_exec(const char *s UNUSED, const struct Name *n UNUSED, void *arg)
{
	VarPattern *v = (VarPattern *)arg;
	char *msg;
	char *result;

	result = Cmd_Exec(v->lbuffer, &msg);
	if (result == NULL)
		Error(msg, v->lbuffer);
	return result;
}

/*-
 *-----------------------------------------------------------------------
 * VarSYSVMatch --
 *	Add the word to the buffer if it matches the given pattern.
 *	Used to implement the System V % modifiers.
 *-----------------------------------------------------------------------
 */
static bool
VarSYSVMatch(struct Name *word, bool addSpace, Buffer buf, void *patp)
{
	size_t	len;
	const char	*ptr;
	VarPattern	*pat = (VarPattern *)patp;

	if (*word->s != '\0') {
		if (addSpace)
			Buf_AddSpace(buf);
		if ((ptr = Str_SYSVMatch(word->s, pat->lhs, &len)) != NULL)
			Str_SYSVSubst(buf, pat->rhs, ptr, len);
		else
			Buf_Addi(buf, word->s, word->e);
		return true;
	} else
		return addSpace;
}

void *
get_sysvpattern(const char **p, SymTable *ctxt UNUSED, bool err UNUSED,
    int endc)
{
	VarPattern		*pattern;
	const char		*cp, *cp2;
	int cnt = 0;
	char startc = endc == ')' ? '(' : '{';

	for (cp = *p;; cp++) {
		if (*cp == '=' && cnt == 0)
			break;
		if (*cp == '\0')
			return NULL;
		if (*cp == startc)
			cnt++;
		else if (*cp == endc) {
			cnt--;
			if (cnt < 0)
				return NULL;
		}
	}
	for (cp2 = cp+1;; cp2++) {
		if ((*cp2 == ':' || *cp2 == endc) && cnt == 0)
			break;
		if (*cp2 == '\0')
			return NULL;
		if (*cp2 == startc)
			cnt++;
		else if (*cp2 == endc) {
			cnt--;
			if (cnt < 0)
				return NULL;
		}
	}

	pattern = (VarPattern *)emalloc(sizeof(VarPattern));
	pattern->lbuffer = pattern->lhs = Str_dupi(*p, cp);
	pattern->leftLen = cp - *p;
	pattern->rhs = Str_dupi(cp+1, cp2);
	pattern->rightLen = cp2 - (cp+1);
	pattern->flags = 0;
	*p = cp2;
	return pattern;
}


/*-
 *-----------------------------------------------------------------------
 * VarSubstitute --
 *	Perform a string-substitution on the given word, Adding the
 *	result to the given buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarSubstitute(struct Name *word, bool addSpace, Buffer buf,
    void *patternp) /* Pattern for substitution */
{
    size_t	wordLen;    /* Length of word */
    const char	*cp;	    /* General pointer */
    VarPattern	*pattern = (VarPattern *)patternp;

    wordLen = word->e - word->s;
    if ((pattern->flags & (VAR_SUB_ONE|VAR_SUB_MATCHED)) !=
	(VAR_SUB_ONE|VAR_SUB_MATCHED)) {
	/* Still substituting -- break it down into simple anchored cases
	 * and if none of them fits, perform the general substitution case.  */
	if ((pattern->flags & VAR_MATCH_START) &&
	    (strncmp(word->s, pattern->lhs, pattern->leftLen) == 0)) {
		/* Anchored at start and beginning of word matches pattern.  */
		if ((pattern->flags & VAR_MATCH_END) &&
		    (wordLen == pattern->leftLen)) {
			/* Also anchored at end and matches to the end (word
			 * is same length as pattern) add space and rhs only
			 * if rhs is non-null.	*/
			if (pattern->rightLen != 0) {
			    if (addSpace)
				Buf_AddSpace(buf);
			    addSpace = true;
			    Buf_AddChars(buf, pattern->rightLen,
					 pattern->rhs);
			}
			pattern->flags |= VAR_SUB_MATCHED;
		} else if (pattern->flags & VAR_MATCH_END) {
		    /* Doesn't match to end -- copy word wholesale.  */
		    goto nosub;
		} else {
		    /* Matches at start but need to copy in
		     * trailing characters.  */
		    if ((pattern->rightLen + wordLen - pattern->leftLen) != 0){
			if (addSpace)
			    Buf_AddSpace(buf);
			addSpace = true;
		    }
		    Buf_AddChars(buf, pattern->rightLen, pattern->rhs);
		    Buf_AddChars(buf, wordLen - pattern->leftLen,
				 word->s + pattern->leftLen);
		    pattern->flags |= VAR_SUB_MATCHED;
		}
	} else if (pattern->flags & VAR_MATCH_START) {
	    /* Had to match at start of word and didn't -- copy whole word.  */
	    goto nosub;
	} else if (pattern->flags & VAR_MATCH_END) {
	    /* Anchored at end, Find only place match could occur (leftLen
	     * characters from the end of the word) and see if it does. Note
	     * that because the $ will be left at the end of the lhs, we have
	     * to use strncmp.	*/
	    cp = word->s + (wordLen - pattern->leftLen);
	    if (cp >= word->s &&
		strncmp(cp, pattern->lhs, pattern->leftLen) == 0) {
		/* Match found. If we will place characters in the buffer,
		 * add a space before hand as indicated by addSpace, then
		 * stuff in the initial, unmatched part of the word followed
		 * by the right-hand-side.  */
		if (((cp - word->s) + pattern->rightLen) != 0) {
		    if (addSpace)
			Buf_AddSpace(buf);
		    addSpace = true;
		}
		Buf_Addi(buf, word->s, cp);
		Buf_AddChars(buf, pattern->rightLen, pattern->rhs);
		pattern->flags |= VAR_SUB_MATCHED;
	    } else {
		/* Had to match at end and didn't. Copy entire word.  */
		goto nosub;
	    }
	} else {
	    /* Pattern is unanchored: search for the pattern in the word using
	     * strstr, copying unmatched portions and the
	     * right-hand-side for each match found, handling non-global
	     * substitutions correctly, etc. When the loop is done, any
	     * remaining part of the word (word and wordLen are adjusted
	     * accordingly through the loop) is copied straight into the
	     * buffer.
	     * addSpace is set to false as soon as a space is added to the
	     * buffer.	*/
	    bool done;
	    size_t origSize;

	    done = false;
	    origSize = Buf_Size(buf);
	    while (!done) {
		cp = strstr(word->s, pattern->lhs);
		if (cp != NULL) {
		    if (addSpace && (cp - word->s) + pattern->rightLen != 0){
			Buf_AddSpace(buf);
			addSpace = false;
		    }
		    Buf_Addi(buf, word->s, cp);
		    Buf_AddChars(buf, pattern->rightLen, pattern->rhs);
		    wordLen -= (cp - word->s) + pattern->leftLen;
		    word->s = cp + pattern->leftLen;
		    if (wordLen == 0 || (pattern->flags & VAR_SUB_GLOBAL) == 0)
			done = true;
		    pattern->flags |= VAR_SUB_MATCHED;
		} else
		    done = true;
	    }
	    if (wordLen != 0) {
		if (addSpace)
		    Buf_AddSpace(buf);
		Buf_AddChars(buf, wordLen, word->s);
	    }
	    /* If added characters to the buffer, need to add a space
	     * before we add any more. If we didn't add any, just return
	     * the previous value of addSpace.	*/
	    return Buf_Size(buf) != origSize || addSpace;
	}
	return addSpace;
    }
 nosub:
    if (addSpace)
	Buf_AddSpace(buf);
    Buf_AddChars(buf, wordLen, word->s);
    return true;
}

#ifndef MAKE_BOOTSTRAP
/*-
 *-----------------------------------------------------------------------
 * VarREError --
 *	Print the error caused by a regcomp or regexec call.
 *-----------------------------------------------------------------------
 */
static void
VarREError(int err, regex_t *pat, const char *str)
{
	char	*errbuf;
	int 	errlen;

	errlen = regerror(err, pat, 0, 0);
	errbuf = emalloc(errlen);
	regerror(err, pat, errbuf, errlen);
	Error("%s: %s", str, errbuf);
	free(errbuf);
}

/*-
 *-----------------------------------------------------------------------
 * VarRESubstitute --
 *	Perform a regex substitution on the given word, placing the
 *	result in the passed buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarRESubstitute(struct Name *word, bool addSpace, Buffer buf, void *patternp)
{
	VarREPattern	*pat;
	int 		xrv;
	const char		*wp;
	char		*rp;
	int 		added;

#define MAYBE_ADD_SPACE()		\
	if (addSpace && !added) 	\
		Buf_AddSpace(buf);	\
	added = 1

	added = 0;
	wp = word->s;
	pat = patternp;

	if ((pat->flags & (VAR_SUB_ONE|VAR_SUB_MATCHED)) ==
	    (VAR_SUB_ONE|VAR_SUB_MATCHED))
		xrv = REG_NOMATCH;
	else {
	tryagain:
		xrv = regexec(&pat->re, wp, pat->nsub, pat->matches, 0);
	}

	switch (xrv) {
	case 0:
		pat->flags |= VAR_SUB_MATCHED;
		if (pat->matches[0].rm_so > 0) {
			MAYBE_ADD_SPACE();
			Buf_AddChars(buf, pat->matches[0].rm_so, wp);
		}

		for (rp = pat->replace; *rp; rp++) {
			if (*rp == '\\' && (rp[1] == '&' || rp[1] == '\\')) {
				MAYBE_ADD_SPACE();
				Buf_AddChar(buf,rp[1]);
				rp++;
			}
			else if (*rp == '&' ||
			    (*rp == '\\' && isdigit(rp[1]))) {
				int n;
				const char *subbuf;
				int sublen;
				char errstr[3];

				if (*rp == '&') {
					n = 0;
					errstr[0] = '&';
					errstr[1] = '\0';
				} else {
					n = rp[1] - '0';
					errstr[0] = '\\';
					errstr[1] = rp[1];
					errstr[2] = '\0';
					rp++;
				}

				if (n > pat->nsub) {
					Error("No subexpression %s",
					    &errstr[0]);
					subbuf = "";
					sublen = 0;
				} else if (pat->matches[n].rm_so == -1 &&
				    pat->matches[n].rm_eo == -1) {
					Error("No match for subexpression %s",
					    &errstr[0]);
					subbuf = "";
					sublen = 0;
				} else {
					subbuf = wp + pat->matches[n].rm_so;
					sublen = pat->matches[n].rm_eo -
					    pat->matches[n].rm_so;
				}

				if (sublen > 0) {
					MAYBE_ADD_SPACE();
					Buf_AddChars(buf, sublen, subbuf);
				}
			} else {
				MAYBE_ADD_SPACE();
				Buf_AddChar(buf, *rp);
			}
		}
		wp += pat->matches[0].rm_eo;
		if (pat->flags & VAR_SUB_GLOBAL)
			goto tryagain;
		if (*wp) {
			MAYBE_ADD_SPACE();
			Buf_AddString(buf, wp);
		}
		break;
	default:
		VarREError(xrv, &pat->re, "Unexpected regex error");
	       /* fall through */
	case REG_NOMATCH:
		if (*wp) {
			MAYBE_ADD_SPACE();
			Buf_AddString(buf, wp);
		}
		break;
	}
	return addSpace||added;
}
#endif

/*-
 *-----------------------------------------------------------------------
 * VarModify --
 *	Modify each of the words of the passed string using the given
 *	function. Used to implement all modifiers.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *-----------------------------------------------------------------------
 */
static char *
VarModify(char *str, 		/* String whose words should be trimmed */
				/* Function to use to modify them */
    bool (*modProc)(struct Name *, bool, Buffer, void *),
    void *datum)		/* Datum to pass it */
{
	BUFFER	  buf;		/* Buffer for the new string */
	bool	  addSpace;	/* true if need to add a space to the
				     * buffer before adding the trimmed
				     * word */
	struct Name	  word;

	Buf_Init(&buf, 0);
	addSpace = false;

	word.e = str;

	while ((word.s = iterate_words(&word.e)) != NULL) {
		char termc;

		termc = *word.e;
		*((char *)(word.e)) = '\0';
		addSpace = (*modProc)(&word, addSpace, &buf, datum);
		*((char *)(word.e)) = termc;
	}
	return Buf_Retrieve(&buf);
}

/*-
 *-----------------------------------------------------------------------
 * VarGetPattern --
 *	Pass through the tstr looking for 1) escaped delimiters,
 *	'$'s and backslashes (place the escaped character in
 *	uninterpreted) and 2) unescaped $'s that aren't before
 *	the delimiter (expand the variable substitution).
 *	Return the expanded string or NULL if the delimiter was missing
 *	If pattern is specified, handle escaped ampersands, and replace
 *	unescaped ampersands with the lhs of the pattern.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *	If length is specified, return the string length of the buffer
 *-----------------------------------------------------------------------
 */
static char *
VarGetPattern(SymTable *ctxt, int err, const char **tstr, int delim1,
    int delim2, size_t *length, VarPattern *pattern)
{
	const char	*cp;
	char	*result;
	BUFFER	buf;
	size_t	junk;

	Buf_Init(&buf, 0);
	if (length == NULL)
		length = &junk;

#define IS_A_MATCH(cp, delim1, delim2) \
	(cp[0] == '\\' && (cp[1] == delim1 || cp[1] == delim2 || \
	 cp[1] == '\\' || cp[1] == '$' || (pattern && cp[1] == '&')))

	/*
	 * Skim through until the matching delimiter is found;
	 * pick up variable substitutions on the way. Also allow
	 * backslashes to quote the delimiter, $, and \, but don't
	 * touch other backslashes.
	 */
	for (cp = *tstr; *cp != '\0' && *cp != delim1 && *cp != delim2; cp++) {
		if (IS_A_MATCH(cp, delim1, delim2)) {
			Buf_AddChar(&buf, cp[1]);
			cp++;
		} else if (*cp == '$') {
			/* Allowed at end of pattern */
			if (cp[1] == delim1 || cp[1] == delim2)
				Buf_AddChar(&buf, *cp);
			else {
				size_t len;

				/* If unescaped dollar sign not before the
				 * delimiter, assume it's a variable
				 * substitution and recurse.  */
				(void)Var_ParseBuffer(&buf, cp, ctxt, err,
				    &len);
				cp += len - 1;
			}
		} else if (pattern && *cp == '&')
			Buf_AddChars(&buf, pattern->leftLen, pattern->lhs);
		else
			Buf_AddChar(&buf, *cp);
	}

	*length = Buf_Size(&buf);
	result = Buf_Retrieve(&buf);

	if (*cp != delim1 && *cp != delim2) {
		*tstr = cp;
		*length = 0;
		free(result);
		return NULL;
	}
	else {
		*tstr = ++cp;
		return result;
	}
}

/*-
 *-----------------------------------------------------------------------
 * VarQuote --
 *	Quote shell meta-characters in the string
 *
 * Results:
 *	The quoted string
 *-----------------------------------------------------------------------
 */
static char *
VarQuote(const char *str, const struct Name *n UNUSED, void *dummy UNUSED)
{

	BUFFER	  buf;
	/* This should cover most shells :-( */
	static char meta[] = "\n \t'`\";&<>()|*?{}[]\\$!#^~";

	Buf_Init(&buf, MAKE_BSIZE);
	for (; *str; str++) {
		if (strchr(meta, *str) != NULL)
			Buf_AddChar(&buf, '\\');
		Buf_AddChar(&buf, *str);
	}
	return Buf_Retrieve(&buf);
}

static void *
check_empty(const char **p, SymTable *ctxt UNUSED, bool b UNUSED, int endc)
{
	dummy_arg->s = NULL;
	if ((*p)[1] == endc || (*p)[1] == ':') {
		(*p)++;
		return dummy_arg;
	} else
		return NULL;
}

static void *
check_shcmd(const char **p, SymTable *ctxt UNUSED, bool b UNUSED, int endc)
{
	if ((*p)[1] == 'h' && ((*p)[2] == endc || (*p)[2] == ':')) {
		(*p)+=2;
		return dummy_arg;
	} else
		return NULL;
}


static char *
do_shcmd(const char *s, const struct Name *n UNUSED, void *arg UNUSED)
{
	char *err;
	char *t;

	t = Cmd_Exec(s, &err);
	if (err)
		Error(err, s);
	return t;
}

static void *
get_stringarg(const char **p, SymTable *ctxt UNUSED, bool b UNUSED, int endc)
{
	const char *cp;
	char *s;

	for (cp = *p + 1; *cp != ':' && *cp != endc; cp++) {
		if (*cp == '\\') {
			if (cp[1] == ':' || cp[1] == endc || cp[1] == '\\')
				cp++;
		} else if (*cp == '\0')
			return NULL;
	}
	s = escape_dupi(*p+1, cp, ":)}");
	*p = cp;
	return s;
}

static void
free_stringarg(void *arg)
{
	free(arg);
}

static char *
do_upper(const char *s, const struct Name *n UNUSED, void *arg UNUSED)
{
	size_t len, i;
	char *t;

	len = strlen(s);
	t = emalloc(len+1);
	for (i = 0; i < len; i++)
		t[i] = toupper(s[i]);
	t[len] = '\0';
	return t;
}

static char *
do_lower(const char *s, const struct Name *n UNUSED, void *arg UNUSED)
{
	size_t	len, i;
	char	*t;

	len = strlen(s);
	t = emalloc(len+1);
	for (i = 0; i < len; i++)
		t[i] = tolower(s[i]);
	t[len] = '\0';
	return t;
}

static void *
get_patternarg(const char **p, SymTable *ctxt, bool err, int endc)
{
	return common_get_patternarg(p, ctxt, err, endc, false);
}

/* Extract anchors */
static void *
get_spatternarg(const char **p, SymTable *ctxt, bool err, int endc)
{
	VarPattern *pattern;

	pattern = common_get_patternarg(p, ctxt, err, endc, true);
	if (pattern != NULL && pattern->leftLen > 0) {
		if (pattern->lhs[pattern->leftLen-1] == '$') {
			    pattern->leftLen--;
			    pattern->flags |= VAR_MATCH_END;
		}
		if (pattern->lhs[0] == '^') {
			    pattern->lhs++;
			    pattern->leftLen--;
			    pattern->flags |= VAR_MATCH_START;
		}
	}
	return pattern;
}

static void
free_looparg(void *arg)
{
	struct LoopStuff *l = (struct LoopStuff *)arg;

	Var_DeleteLoopVar(l->var);
	free(l->expand);
}

static char *
LoopGrab(const char **s)
{
	const char *p, *start;

	start = *s;
	for (p = start; *p != '@'; p++) {
		if (*p == '\\')
			p++;
		if (*p == 0)
			return NULL;
	}
	*s = p+1;
	return escape_dupi(start, p, "@\\");
}

static void *
get_loop(const char **p, SymTable *ctxt UNUSED, bool err, int endc)
{
	static struct LoopStuff loop;
	const char *s;
	const char *var;

	s = *p +1;

	loop.var = NULL;
	loop.expand = NULL;
	loop.err = err;
	var = LoopGrab(&s);
	if (var != NULL) {
		loop.expand = LoopGrab(&s);
		if (*s == endc || *s == ':') {
			*p = s;
			loop.var = Var_NewLoopVar(var, NULL);
			return &loop;
		}
	}
	free_looparg(&loop);
	return NULL;
}

static void *
common_get_patternarg(const char **p, SymTable *ctxt, bool err, int endc,
    bool dosubst)
{
	VarPattern *pattern;
	char delim;
	const char *s;

	pattern = (VarPattern *)emalloc(sizeof(VarPattern));
	pattern->flags = 0;
	s = *p;

	delim = s[1];
	if (delim == '\0')
		return NULL;
	s += 2;

	pattern->rhs = NULL;
	pattern->lhs = VarGetPattern(ctxt, err, &s, delim, delim,
	    &pattern->leftLen, NULL);
	pattern->lbuffer = pattern->lhs;
	if (pattern->lhs != NULL) {
		pattern->rhs = VarGetPattern(ctxt, err, &s, delim, delim,
		    &pattern->rightLen, dosubst ? pattern: NULL);
		if (pattern->rhs != NULL) {
			/* Check for global substitution. If 'g' after the
			 * final delimiter, substitution is global and is
			 * marked that way.  */
			for (;; s++) {
				switch (*s) {
				case 'g':
					pattern->flags |= VAR_SUB_GLOBAL;
					continue;
				case '1':
					pattern->flags |= VAR_SUB_ONE;
					continue;
				}
				break;
			}
			if (*s == endc || *s == ':') {
				*p = s;
				return pattern;
			}
		}
	}
	free_patternarg(pattern);
	return NULL;
}

static void *
assign_get_value(const char **p, SymTable *ctxt, bool err, int endc)
{
	const char *s;
	int flags;
	VarPattern *arg;

	s = *p + 1;
	if (s[0] == '=')
		flags = VAR_EQUAL;
	else if (s[0] == '?' && s[1] == '=')
		flags = VAR_MAY_EQUAL;
	else if (s[0] == '+' && s[1] == '=')
		flags = VAR_ADD_EQUAL;
	else if (s[0] == '!' && s[1] == '=')
		flags = VAR_BANG_EQUAL;
	else
		return NULL;

	arg = get_value(&s, ctxt, err, endc);
	if (arg != NULL) {
		*p = s;
		arg->flags = flags;
	}
	return arg;
}

static void *
get_value(const char **p, SymTable *ctxt, bool err, int endc)
{
	VarPattern *pattern;
	const char *s;

	pattern = (VarPattern *)emalloc(sizeof(VarPattern));
	s = *p + 1;
	pattern->rhs = NULL;
	pattern->lbuffer = VarGetPattern(ctxt, err, &s, ':', endc,
	    &pattern->leftLen, NULL);
	if (s[-1] == endc || s[-1] == ':') {
		*p = s-1;
		return pattern;
	}
	free_patternarg(pattern);
	return NULL;
}

static void *
get_cmd(const char **p, SymTable *ctxt, bool err, int endc UNUSED)
{
	VarPattern *pattern;
	const char *s;

	pattern = (VarPattern *)emalloc(sizeof(VarPattern));
	s = *p + 1;
	pattern->rhs = NULL;
	pattern->lbuffer = VarGetPattern(ctxt, err, &s, '!', '!',
	    &pattern->leftLen, NULL);
	if (s[-1] == '!') {
		*p = s-1;
		return pattern;
	}
	free_patternarg(pattern);
	return NULL;
}

static void
free_patternarg(void *p)
{
	VarPattern *vp = (VarPattern *)p;

	free(vp->lbuffer);
	free(vp->rhs);
	free(vp);
}

#ifndef MAKE_BOOTSTRAP
static char *
do_regex(const char *s, const struct Name *n UNUSED, void *arg)
{
	VarREPattern p2;
	VarPattern *p = (VarPattern *)arg;
	int error;
	char *result;

	error = regcomp(&p2.re, p->lhs, REG_EXTENDED);
	if (error) {
		VarREError(error, &p2.re, "RE substitution error");
		return var_Error;
	}
	p2.nsub = p2.re.re_nsub + 1;
	p2.replace = p->rhs;
	p2.flags = p->flags;
	if (p2.nsub < 1)
		p2.nsub = 1;
	if (p2.nsub > 10)
		p2.nsub = 10;
	p2.matches = emalloc(p2.nsub * sizeof(regmatch_t));
	result = VarModify((char *)s, VarRESubstitute, &p2);
	regfree(&p2.re);
	free(p2.matches);
	return result;
}
#endif

char *
VarModifiers_Apply(char *str, const struct Name *name, SymTable *ctxt,
    bool err, bool *freePtr, const char **pscan, int paren)
{
	const char *tstr;
	bool atstart;    /* Some ODE modifiers only make sense at start */
	char endc = paren == '(' ? ')' : '}';
	const char *start = *pscan;

	tstr = start;
	/*
	 * Now we need to apply any modifiers the user wants applied.
	 * These are:
	 *		  :M<pattern>	words which match the given <pattern>.
	 *				<pattern> is of the standard file
	 *				wildcarding form.
	 *		  :S<d><pat1><d><pat2><d>[g]
	 *				Substitute <pat2> for <pat1> in the
	 *				value
	 *		  :C<d><pat1><d><pat2><d>[g]
	 *				Substitute <pat2> for regex <pat1> in
	 *				the value
	 *		  :H		Substitute the head of each word
	 *		  :T		Substitute the tail of each word
	 *		  :E		Substitute the extension (minus '.') of
	 *				each word
	 *		  :R		Substitute the root of each word
	 *				(pathname minus the suffix).
	 *		  :lhs=rhs	Like :S, but the rhs goes to the end of
	 *				the invocation.
	 */

	atstart = true;
	while (*tstr != endc && *tstr != '\0') {
		struct modifier *mod;
		void *arg;
		char *newStr;

		tstr++;
		if (DEBUG(VAR))
			printf("Applying :%c to \"%s\"\n", *tstr, str);

		mod = choose_mod[*tstr];
		arg = NULL;

		if (mod != NULL && (!mod->atstart || atstart))
			arg = mod->getarg(&tstr, ctxt, err, endc);
		if (FEATURES(FEATURE_SYSVVARSUB) && arg == NULL) {
			mod = &sysv_mod;
			arg = mod->getarg(&tstr, ctxt, err, endc);
		}
		atstart = false;
		if (arg != NULL) {
			if (str != NULL || (mod->atstart && name != NULL)) {
				if (mod->word_apply != NULL) {
					newStr = VarModify(str,
					    mod->word_apply, arg);
					if (mod->apply != NULL) {
						char *newStr2;

						newStr2 = mod->apply(newStr,
						    name, arg);
						free(newStr);
						newStr = newStr2;
					}
				} else
					newStr = mod->apply(str, name, arg);
				if (*freePtr)
					free(str);
				str = newStr;
				if (str != var_Error)
					*freePtr = true;
				else
					*freePtr = false;
			}
			if (mod->freearg != NULL)
				mod->freearg(arg);
		} else {
			Error("Bad modifier: %s\n", tstr);
			/* Try skipping to end of var... */
			for (tstr++; *tstr != endc && *tstr != '\0';)
				tstr++;
			if (str != NULL && *freePtr)
				free(str);
			str = var_Error;
			*freePtr = false;
			break;
		}
		if (DEBUG(VAR))
			printf("Result is \"%s\"\n", str);
	}
	if (*tstr == '\0')
		Error("Unclosed variable specification");
	else
		tstr++;

	*pscan = tstr;
	return str;
}

char *
Var_GetHead(char *s)
{
	return VarModify(s, VarHead, NULL);
}

char *
Var_GetTail(char *s)
{
	return VarModify(s, VarTail, NULL);
}
