/*	$OpenBSD: varmodifiers.c,v 1.3 2000/07/17 23:54:26 espie Exp $	*/
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

/* VarModifiers_Apply is mostly a constituent function of Var_Parse.  */
 
#include    <ctype.h>
#ifndef MAKE_BOOTSTRAP
#include    <sys/types.h>
#include    <regex.h>
#endif
#include "make.h"
#include "buf.h"
#include "varmodifiers.h"

/* Var*Pattern flags */
#define VAR_SUB_GLOBAL	0x01	/* Apply substitution globally */
#define VAR_SUB_ONE	0x02	/* Apply substitution to one word */
#define VAR_SUB_MATCHED	0x04	/* There was a match */
#define VAR_MATCH_START	0x08	/* Match at start of word */
#define VAR_MATCH_END	0x10	/* Match at end of word */

typedef struct {
    char    	  *lhs;	    /* String to match */
    size_t    	  leftLen;  /* Length of string */
    char    	  *rhs;	    /* Replacement string (w/ &'s removed) */
    size_t    	  rightLen; /* Length of replacement */
    int	    	  flags;
} VarPattern;

#ifndef MAKE_BOOTSTRAP
typedef struct {
    regex_t	  re;
    int		  nsub;
    regmatch_t	 *matches;
    char	 *replace;
    int		  flags;
} VarREPattern;
#endif

static Boolean VarHead __P((const char *, Boolean, Buffer, void *));
static Boolean VarTail __P((const char *, Boolean, Buffer, void *));
static Boolean VarSuffix __P((const char *, Boolean, Buffer, void *));
static Boolean VarRoot __P((const char *, Boolean, Buffer, void *));
static Boolean VarMatch __P((const char *, Boolean, Buffer, void *));
#ifdef SYSVVARSUB
static Boolean VarSYSVMatch __P((const char *, Boolean, Buffer, void *));
#endif
static Boolean VarNoMatch __P((const char *, Boolean, Buffer, void *));
#ifndef MAKE_BOOTSTRAP
static void VarREError __P((int, regex_t *, const char *));
static Boolean VarRESubstitute __P((const char *, Boolean, Buffer, void *));
#endif
static Boolean VarSubstitute __P((const char *, Boolean, Buffer, void *));
static char *VarGetPattern __P((SymTable *, int, char **, int, int *, size_t *,
				VarPattern *));
static char *VarQuote __P((const char *));
static char *VarModify __P((const char *, Boolean (*)(const char *, Boolean, Buffer, void *), void *));
static Boolean VarUppercase __P((const char *, Boolean, Buffer, void *));
static Boolean VarLowercase __P((const char *, Boolean, Buffer, void *));

/*-
 *-----------------------------------------------------------------------
 * VarUppercase --
 *	Place the Upper cased word in the given buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The word is added to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarUppercase(word, addSpace, buf, dummy)
    const char    *word;    	/* Word to Upper Case */
    Boolean 	  addSpace; 	/* True if need to add a space to the buffer
				 * before sticking in the head */
    Buffer  	  buf;	    	/* Buffer in which to store it */
    void *dummy;
{
    size_t len = strlen(word);

    if (addSpace)
	Buf_AddSpace(buf);
    while (len--)
    	Buf_AddChar(buf, toupper(*word++));
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * VarLowercase --
 *	Place the Lower cased word in the given buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The word is added to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarLowercase(word, addSpace, buf, dummy)
    const char    *word;    	/* Word to Lower Case */
    Boolean 	  addSpace; 	/* True if need to add a space to the buffer
				 * before sticking in the head */
    Buffer  	  buf;	    	/* Buffer in which to store it */
    void *dummy;
{
    size_t len = strlen(word);

    if (addSpace)
	Buf_AddSpace(buf);
    while (len--)
    	Buf_AddChar(buf, tolower(*word++));
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * VarHead --
 *	Remove the tail of the given word and place the result in the given
 *	buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarHead(word, addSpace, buf, dummy)
    const char    *word;    	/* Word to trim */
    Boolean 	  addSpace; 	/* True if need to add a space to the buffer
				 * before sticking in the head */
    Buffer  	  buf;	    	/* Buffer in which to store it */
    void 	  *dummy;
{
    const char 	  *slash;

    slash = strrchr(word, '/');
    if (slash != NULL) {
	if (addSpace)
	    Buf_AddSpace(buf);
	Buf_AddInterval(buf, word, slash);
	return TRUE;
    } else {
	/* If no directory part, give . (q.v. the POSIX standard) */
	if (addSpace)
	    Buf_AddString(buf, " .");
	else
	    Buf_AddChar(buf, '.');
    }
    return(dummy ? TRUE : TRUE);
}

/*-
 *-----------------------------------------------------------------------
 * VarTail --
 *	Remove the head of the given word and place the result in the given
 *	buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarTail(word, addSpace, buf, dummy)
    const char    *word;    	/* Word to trim */
    Boolean 	  addSpace; 	/* TRUE if need to stick a space in the
				 * buffer before adding the tail */
    Buffer  	  buf;	    	/* Buffer in which to store it */
    void 	  *dummy;
{
    const char *slash;

    if (addSpace) 
	Buf_AddSpace(buf);
    slash = strrchr(word, '/');
    if (slash != NULL)
	Buf_AddString(buf, slash+1);
    else
	Buf_AddString(buf, word);
    return (dummy ? TRUE : TRUE);
}

/*-
 *-----------------------------------------------------------------------
 * VarSuffix --
 *	Place the suffix of the given word in the given buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The suffix from the word is placed in the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarSuffix(word, addSpace, buf, dummy)
    const char    *word;    	/* Word to trim */
    Boolean 	  addSpace; 	/* TRUE if need to add a space before placing
				 * the suffix in the buffer */
    Buffer  	  buf;	    	/* Buffer in which to store it */
    void 	  *dummy;
{
    const char *dot;

    dot = strrchr(word, '.');
    if (dot != NULL) {
	if (addSpace)
	    Buf_AddSpace(buf);
	Buf_AddString(buf, dot+1);
	addSpace = TRUE;
    }
    return (dummy ? addSpace : addSpace);
}

/*-
 *-----------------------------------------------------------------------
 * VarRoot --
 *	Remove the suffix of the given word and place the result in the
 *	buffer.
 *
 * Results:
 *	TRUE if characters were added to the buffer (a space needs to be
 *	added to the buffer before the next word).
 *
 * Side Effects:
 *	The trimmed word is added to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarRoot(word, addSpace, buf, dummy)
    const char 	  *word;    	/* Word to trim */
    Boolean 	  addSpace; 	/* TRUE if need to add a space to the buffer
				 * before placing the root in it */
    Buffer  	  buf;	    	/* Buffer in which to store it */
    void 	  *dummy;
{
    const char *dot;

    if (addSpace)
	Buf_AddSpace(buf);

    dot = strrchr(word, '.');
    if (dot != NULL)
	Buf_AddInterval(buf, word, dot);
    else
	Buf_AddString(buf, word);
    return (dummy ? TRUE : TRUE);
}

/*-
 *-----------------------------------------------------------------------
 * VarMatch --
 *	Place the word in the buffer if it matches the given pattern.
 *	Callback function for VarModify to implement the :M modifier.
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarMatch(word, addSpace, buf, pattern)
    const char    *word;    	/* Word to examine */
    Boolean 	  addSpace; 	/* TRUE if need to add a space to the
				 * buffer before adding the word, if it
				 * matches */
    Buffer  	  buf;	    	/* Buffer in which to store it */
    void 	  *pattern; 	/* Pattern the word must match */
{
    if (Str_Match(word, (char *) pattern)) {
	if (addSpace)
	    Buf_AddSpace(buf);
	addSpace = TRUE;
	Buf_AddString(buf, word);
    }
    return addSpace;
}

#ifdef SYSVVARSUB
/*-
 *-----------------------------------------------------------------------
 * VarSYSVMatch --
 *	Place the word in the buffer if it matches the given pattern.
 *	Callback function for VarModify to implement the System V %
 *	modifiers.
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarSYSVMatch(word, addSpace, buf, patp)
    const char    *word;    	/* Word to examine */
    Boolean 	  addSpace; 	/* TRUE if need to add a space to the
				 * buffer before adding the word, if it
				 * matches */
    Buffer  	  buf;	    	/* Buffer in which to store it */
    void 	  *patp; 	/* Pattern the word must match */
{
    size_t 	  len;
    const char    *ptr;
    VarPattern 	  *pat = (VarPattern *) patp;

    if (*word) {
	    if (addSpace)
		Buf_AddSpace(buf);

	    addSpace = TRUE;

	    if ((ptr = Str_SYSVMatch(word, pat->lhs, &len)) != NULL)
		Str_SYSVSubst(buf, pat->rhs, ptr, len);
	    else
		Buf_AddString(buf, word);
    }
    return addSpace;
}
#endif

/*-
 *-----------------------------------------------------------------------
 * VarNoMatch --
 *	Place the word in the buffer if it doesn't match the given pattern.
 *	Callback function for VarModify to implement the :N modifier.
 *
 * Results:
 *	TRUE if a space should be placed in the buffer before the next
 *	word.
 *
 * Side Effects:
 *	The word may be copied to the buffer.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarNoMatch(word, addSpace, buf, pattern)
    const char    *word;    	/* Word to examine */
    Boolean 	  addSpace; 	/* TRUE if need to add a space to the
				 * buffer before adding the word, if it
				 * matches */
    Buffer  	  buf;	    	/* Buffer in which to store it */
    void 	  *pattern; 	/* Pattern the word must match */
{
    if (!Str_Match(word, (char *) pattern)) {
	if (addSpace)
	    Buf_AddSpace(buf);
	addSpace = TRUE;
	Buf_AddString(buf, word);
    }
    return(addSpace);
}


/*-
 *-----------------------------------------------------------------------
 * VarSubstitute --
 *	Perform a string-substitution on the given word, placing the
 *	result in the passed buffer.
 *
 * Results:
 *	TRUE if a space is needed before more characters are added.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarSubstitute(word, addSpace, buf, patternp)
    const char 	  	*word;	    /* Word to modify */
    Boolean 	  	addSpace;   /* True if space should be added before
				     * other characters */
    Buffer  	  	buf;	    /* Buffer for result */
    void 		*patternp;  /* Pattern for substitution */
{
    size_t  		wordLen;    /* Length of word */
    const char 	*cp;	    	    /* General pointer */
    VarPattern	*pattern = (VarPattern *) patternp;

    wordLen = strlen(word);
    if ((pattern->flags & (VAR_SUB_ONE|VAR_SUB_MATCHED)) !=
	(VAR_SUB_ONE|VAR_SUB_MATCHED)) {
	/*
	 * Still substituting -- break it down into simple anchored cases
	 * and if none of them fits, perform the general substitution case.
	 */
	if ((pattern->flags & VAR_MATCH_START) &&
	    (strncmp(word, pattern->lhs, pattern->leftLen) == 0)) {
		/*
		 * Anchored at start and beginning of word matches pattern
		 */
		if ((pattern->flags & VAR_MATCH_END) &&
		    (wordLen == pattern->leftLen)) {
			/*
			 * Also anchored at end and matches to the end (word
			 * is same length as pattern) add space and rhs only
			 * if rhs is non-null.
			 */
			if (pattern->rightLen != 0) {
			    if (addSpace)
				Buf_AddSpace(buf);
			    addSpace = TRUE;
			    Buf_AddChars(buf, pattern->rightLen, pattern->rhs);
			}
			pattern->flags |= VAR_SUB_MATCHED;
		} else if (pattern->flags & VAR_MATCH_END) {
		    /*
		     * Doesn't match to end -- copy word wholesale
		     */
		    goto nosub;
		} else {
		    /*
		     * Matches at start but need to copy in trailing characters
		     */
		    if ((pattern->rightLen + wordLen - pattern->leftLen) != 0){
			if (addSpace)
			    Buf_AddSpace(buf);
			addSpace = TRUE;
		    }
		    Buf_AddChars(buf, pattern->rightLen, pattern->rhs);
		    Buf_AddChars(buf, wordLen - pattern->leftLen,
				 word + pattern->leftLen);
		    pattern->flags |= VAR_SUB_MATCHED;
		}
	} else if (pattern->flags & VAR_MATCH_START) {
	    /*
	     * Had to match at start of word and didn't -- copy whole word.
	     */
	    goto nosub;
	} else if (pattern->flags & VAR_MATCH_END) {
	    /*
	     * Anchored at end, Find only place match could occur (leftLen
	     * characters from the end of the word) and see if it does. Note
	     * that because the $ will be left at the end of the lhs, we have
	     * to use strncmp.
	     */
	    cp = word + (wordLen - pattern->leftLen);
	    if ((cp >= word) &&
		(strncmp(cp, pattern->lhs, pattern->leftLen) == 0)) {
		/*
		 * Match found. If we will place characters in the buffer,
		 * add a space before hand as indicated by addSpace, then
		 * stuff in the initial, unmatched part of the word followed
		 * by the right-hand-side.
		 */
		if (((cp - word) + pattern->rightLen) != 0) {
		    if (addSpace)
			Buf_AddSpace(buf);
		    addSpace = TRUE;
		}
		Buf_AddInterval(buf, word, cp);
		Buf_AddChars(buf, pattern->rightLen, pattern->rhs);
		pattern->flags |= VAR_SUB_MATCHED;
	    } else {
		/*
		 * Had to match at end and didn't. Copy entire word.
		 */
		goto nosub;
	    }
	} else {
	    /*
	     * Pattern is unanchored: search for the pattern in the word using
	     * String_FindSubstring, copying unmatched portions and the
	     * right-hand-side for each match found, handling non-global
	     * substitutions correctly, etc. When the loop is done, any
	     * remaining part of the word (word and wordLen are adjusted
	     * accordingly through the loop) is copied straight into the
	     * buffer.
	     * addSpace is set FALSE as soon as a space is added to the
	     * buffer.
	     */
	    register Boolean done;
	    size_t origSize;

	    done = FALSE;
	    origSize = Buf_Size(buf);
	    while (!done) {
		cp = strstr(word, pattern->lhs);
		if (cp != (char *)NULL) {
		    if (addSpace && (((cp - word) + pattern->rightLen) != 0)){
			Buf_AddSpace(buf);
			addSpace = FALSE;
		    }
		    Buf_AddInterval(buf, word, cp);
		    Buf_AddChars(buf, pattern->rightLen, pattern->rhs);
		    wordLen -= (cp - word) + pattern->leftLen;
		    word = cp + pattern->leftLen;
		    if (wordLen == 0 || (pattern->flags & VAR_SUB_GLOBAL) == 0){
			done = TRUE;
		    }
		    pattern->flags |= VAR_SUB_MATCHED;
		} else {
		    done = TRUE;
		}
	    }
	    if (wordLen != 0) {
		if (addSpace)
		    Buf_AddSpace(buf);
		Buf_AddChars(buf, wordLen, word);
	    }
	    /*
	     * If added characters to the buffer, need to add a space
	     * before we add any more. If we didn't add any, just return
	     * the previous value of addSpace.
	     */
	    return (Buf_Size(buf) != origSize || addSpace);
	}
	return (addSpace);
    }
 nosub:
    if (addSpace)
	Buf_AddSpace(buf);
    Buf_AddChars(buf, wordLen, word);
    return(TRUE);
}


#ifndef MAKE_BOOTSTRAP
/*-
 *-----------------------------------------------------------------------
 * VarREError --
 *	Print the error caused by a regcomp or regexec call.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	An error gets printed.
 *
 *-----------------------------------------------------------------------
 */
static void
VarREError(err, pat, str)
    int err;
    regex_t *pat;
    const char *str;
{
    char *errbuf;
    int errlen;

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
 *
 * Results:
 *	TRUE if a space is needed before more characters are added.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
static Boolean
VarRESubstitute(word, addSpace, buf, patternp)
    const char *word;
    Boolean addSpace;
    Buffer buf;
    void *patternp;
{
    VarREPattern *pat;
    int xrv;
    const char *wp;
    char *rp;
    int added;

#define MAYBE_ADD_SPACE()		\
	if (addSpace && !added)		\
	    Buf_AddSpace(buf);		\
	added = 1

    added = 0;
    wp = word;
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
	    if ((*rp == '\\') && ((rp[1] == '&') || (rp[1] == '\\'))) {
		MAYBE_ADD_SPACE();
		Buf_AddChar(buf, rp[1]);
		rp++;
	    }
	    else if ((*rp == '&') || ((*rp == '\\') && isdigit(rp[1]))) {
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
		    Error("No subexpression %s", &errstr[0]);
		    subbuf = "";
		    sublen = 0;
		} else if ((pat->matches[n].rm_so == -1) &&
			   (pat->matches[n].rm_eo == -1)) {
		    Error("No match for subexpression %s", &errstr[0]);
		    subbuf = "";
		    sublen = 0;
	        } else {
		    subbuf = wp + pat->matches[n].rm_so;
		    sublen = pat->matches[n].rm_eo - pat->matches[n].rm_so;
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
	    Buf_AddChars(buf, strlen(wp), wp);
	}
	break;
    default:
	VarREError(xrv, &pat->re, "Unexpected regex error");
       /* fall through */
    case REG_NOMATCH:
	if (*wp) {
	    MAYBE_ADD_SPACE();
	    Buf_AddChars(buf, strlen(wp), wp);
	}
	break;
    }
    return(addSpace||added);
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
 *
 *-----------------------------------------------------------------------
 */
static char *
VarModify (str, modProc, datum)
    const char    	  *str;	    /* String whose words should be trimmed */
				    /* Function to use to modify them */
    Boolean    	  (*modProc) __P((const char *, Boolean, Buffer, void *));
    void          *datum;    	    /* Datum to pass it */
{
    BUFFER  	  buf;	    	    /* Buffer for the new string */
    Boolean 	  addSpace; 	    /* TRUE if need to add a space to the
				     * buffer before adding the trimmed
				     * word */
    char 	  **av;		    /* word list */
    char 	  *as;		    /* word list memory */
    int ac, i;

    if (str == NULL)
    	return NULL;

    Buf_Init(&buf, 0);
    addSpace = FALSE;

    av = brk_string(str, &ac, FALSE, &as);

    for (i = 0; i < ac; i++)
	addSpace = (*modProc)(av[i], addSpace, &buf, datum);

    free(as);
    free(av);
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
 *	If flags is specified and the last character of the pattern is a
 *	$ set the VAR_MATCH_END bit of flags.
 *
 * Side Effects:
 *	None.
 *-----------------------------------------------------------------------
 */
static char *
VarGetPattern(ctxt, err, tstr, delim, flags, length, pattern)
    SymTable *ctxt;
    int err;
    char **tstr;
    int delim;
    int *flags;
    size_t *length;
    VarPattern *pattern;
{
    char *cp;
    BUFFER buf;
    size_t junk;

    Buf_Init(&buf, 0);
    if (length == NULL)
	length = &junk;

#define IS_A_MATCH(cp, delim) \
    ((cp[0] == '\\') && ((cp[1] == delim) ||  \
     (cp[1] == '\\') || (cp[1] == '$') || (pattern && (cp[1] == '&'))))

    /*
     * Skim through until the matching delimiter is found;
     * pick up variable substitutions on the way. Also allow
     * backslashes to quote the delimiter, $, and \, but don't
     * touch other backslashes.
     */
    for (cp = *tstr; *cp && (*cp != delim); cp++) {
	if (IS_A_MATCH(cp, delim)) {
	    Buf_AddChar(&buf, cp[1]);
	    cp++;
	} else if (*cp == '$') {
	    if (cp[1] == delim) {
		if (flags == NULL)
		    Buf_AddChar(&buf, *cp);
		else
		    /*
		     * Unescaped $ at end of pattern => anchor
		     * pattern at end.
		     */
		    *flags |= VAR_MATCH_END;
	    }
	    else {
		char   *cp2;
		size_t     len;
		Boolean freeIt;

		/*
		 * If unescaped dollar sign not before the
		 * delimiter, assume it's a variable
		 * substitution and recurse.
		 */
		cp2 = Var_Parse(cp, ctxt, err, &len, &freeIt);
		Buf_AddString(&buf, cp2);
		if (freeIt)
		    free(cp2);
		cp += len - 1;
	    }
	}
	else if (pattern && *cp == '&')
	    Buf_AddChars(&buf, pattern->leftLen, pattern->lhs);
	else
	    Buf_AddChar(&buf, *cp);
    }

    if (*cp != delim) {
	*tstr = cp;
	*length = 0;
	return NULL;
    }
    else {
	*tstr = ++cp;
	*length = Buf_Size(&buf);
	return Buf_Retrieve(&buf);
    }
}

char *
VarModifiers_Apply(str, ctxt, err, freePtr, start, endc, lengthPtr)
    char 	*str;
    SymTable	*ctxt;
    Boolean	err;
    Boolean	*freePtr;
    char	*start;
    char 	endc;
    size_t	*lengthPtr;
{
    char 	*tstr; 
    char	delim;
    char	*cp;

    tstr = start;

    /*
     * Now we need to apply any modifiers the user wants applied.
     * These are:
     *  	  :M<pattern>	words which match the given <pattern>.
     *  	  	    	<pattern> is of the standard file
     *  	  	    	wildcarding form.
     *  	  :S<d><pat1><d><pat2><d>[g]
     *  	  	    	Substitute <pat2> for <pat1> in the value
     *  	  :C<d><pat1><d><pat2><d>[g]
     *  	  	    	Substitute <pat2> for regex <pat1> in the value
     *  	  :H	    	Substitute the head of each word
     *  	  :T	    	Substitute the tail of each word
     *  	  :E	    	Substitute the extension (minus '.') of
     *  	  	    	each word
     *  	  :R	    	Substitute the root of each word
     *  	  	    	(pathname minus the suffix).
     *	    	  :lhs=rhs  	Like :S, but the rhs goes to the end of
     *	    	    	    	the invocation.
     */
    while (*tstr != endc) {
	char	*newStr;    /* New value to return */
	char	termc;	    /* Character which terminated scan */

	if (DEBUG(VAR))
	    printf("Applying :%c to \"%s\"\n", *tstr, str ? str : "");
	switch (*tstr) {
	    case 'N':
	    case 'M':
	    {
		for (cp = tstr + 1;
		     *cp != '\0' && *cp != ':' && *cp != endc;
		     cp++) {
		    if (*cp == '\\' && (cp[1] == ':' || cp[1] == endc)){
			cp++;
		    }
		}
		termc = *cp;
		*cp = '\0';
		if (*tstr == 'M')
		    newStr = VarModify(str, VarMatch, tstr+1);
		else
		    newStr = VarModify(str, VarNoMatch, tstr+1);
		break;
	    }
	    case 'S':
	    {
		VarPattern 	    pattern;

		pattern.flags = 0;
		delim = tstr[1];
		tstr += 2;

		/* If pattern begins with '^', it is anchored to the
		 * start of the word -- skip over it and flag pattern.  */
		if (*tstr == '^') {
		    pattern.flags |= VAR_MATCH_START;
		    tstr++;
		}

		cp = tstr;
		if ((pattern.lhs = VarGetPattern(ctxt, err, &cp, delim,
		    &pattern.flags, &pattern.leftLen, NULL)) == NULL)
		    goto cleanup;

		if ((pattern.rhs = VarGetPattern(ctxt, err, &cp, delim,
		    NULL, &pattern.rightLen, &pattern)) == NULL)
		    goto cleanup;

		/* Check for global substitution. If 'g' after the final
		 * delimiter, substitution is global and is marked that
		 * way.  */
		for (;; cp++) {
		    switch (*cp) {
		    case 'g':
			pattern.flags |= VAR_SUB_GLOBAL;
			continue;
		    case '1':
			pattern.flags |= VAR_SUB_ONE;
			continue;
		    }
		    break;
		}

		termc = *cp;
		newStr = VarModify(str, VarSubstitute, &pattern);

		/* Free the two strings.  */
		free(pattern.lhs);
		free(pattern.rhs);
		break;
	    }
#ifndef MAKE_BOOTSTRAP
	    case 'C':
	    {
		VarREPattern    pattern;
		char           *re;
		int             error;

		pattern.flags = 0;
		delim = tstr[1];
		tstr += 2;

		cp = tstr;

		if ((re = VarGetPattern(ctxt, err, &cp, delim, NULL,
		    NULL, NULL)) == NULL)
		    goto cleanup;

		if ((pattern.replace = VarGetPattern(ctxt, err, &cp,
		    delim, NULL, NULL, NULL)) == NULL) {
		    free(re);
		    goto cleanup;
		}

		for (;; cp++) {
		    switch (*cp) {
		    case 'g':
			pattern.flags |= VAR_SUB_GLOBAL;
			continue;
		    case '1':
			pattern.flags |= VAR_SUB_ONE;
			continue;
		    }
		    break;
		}

		termc = *cp;

		error = regcomp(&pattern.re, re, REG_EXTENDED);
		free(re);
		if (error) {
		    *lengthPtr = cp - start + 1;
		    VarREError(error, &pattern.re, "RE substitution error");
		    free(pattern.replace);
		    return var_Error;
		}

		pattern.nsub = pattern.re.re_nsub + 1;
		if (pattern.nsub < 1)
		    pattern.nsub = 1;
		if (pattern.nsub > 10)
		    pattern.nsub = 10;
		pattern.matches = emalloc(pattern.nsub *
					  sizeof(regmatch_t));
		newStr = VarModify(str, VarRESubstitute, &pattern);
		regfree(&pattern.re);
		free(pattern.replace);
		free(pattern.matches);
		break;
	    }
#endif
	    case 'Q':
		if (tstr[1] == endc || tstr[1] == ':') {
		    newStr = VarQuote(str);
		    cp = tstr + 1;
		    termc = *cp;
		    break;
		}
		/* FALLTHROUGH */
	    case 'T':
		if (tstr[1] == endc || tstr[1] == ':') {
		    newStr = VarModify(str, VarTail, NULL);
		    cp = tstr + 1;
		    termc = *cp;
		    break;
		}
		/* FALLTHROUGH */
	    case 'H':
		if (tstr[1] == endc || tstr[1] == ':') {
		    newStr = VarModify(str, VarHead, NULL);
		    cp = tstr + 1;
		    termc = *cp;
		    break;
		}
		/* FALLTHROUGH */
	    case 'E':
		if (tstr[1] == endc || tstr[1] == ':') {
		    newStr = VarModify(str, VarSuffix, NULL);
		    cp = tstr + 1;
		    termc = *cp;
		    break;
		}
		/* FALLTHROUGH */
	    case 'R':
		if (tstr[1] == endc || tstr[1] == ':') {
		    newStr = VarModify(str, VarRoot, NULL);
		    cp = tstr + 1;
		    termc = *cp;
		    break;
		}
		/* FALLTHROUGH */
	    case 'U':
		if (tstr[1] == endc || tstr[1] == ':') {
		    newStr = VarModify(str, VarUppercase, NULL);
		    cp = tstr + 1;
		    termc = *cp;
		    break;
		}
		/* FALLTHROUGH */
	    case 'L':
		if (tstr[1] == endc || tstr[1] == ':') {
		    newStr = VarModify(str, VarLowercase, NULL);
		    cp = tstr + 1;
		    termc = *cp;
		    break;
		}
		/* FALLTHROUGH */
#ifdef SUNSHCMD
	    case 's':
		if (tstr[1] == 'h' && (tstr[2] == endc || tstr[2] == ':')) {
		    char *err;
		    newStr = str ? Cmd_Exec(str, &err) : NULL;
		    if (err)
			Error(err, str);
		    cp = tstr + 2;
		    termc = *cp;
		    break;
		}
		/* FALLTHROUGH */
#endif
	    default:
	    {
#ifdef SYSVVARSUB
		/* This can either be a bogus modifier or a System-V
		 * substitution command.  */
		VarPattern      pattern;
		Boolean         eqFound;
		int           	cnt;	/* Used to count brace pairs when 
					 * variable in in parens or braces */
		char		startc;

		if (endc == ')') 
		    startc = '(';
		else
		    startc = '{';

		pattern.flags = 0;
		eqFound = FALSE;
		/* First we make a pass through the string trying
		 * to verify it is a SYSV-make-style translation:
		 * it must be: <string1>=<string2>) */
		cp = tstr;
		cnt = 1;
		while (*cp != '\0' && cnt) {
		    if (*cp == '=') {
			eqFound = TRUE;
			/* continue looking for endc */
		    }
		    else if (*cp == endc)
			cnt--;
		    else if (*cp == startc)
			cnt++;
		    if (cnt)
			cp++;
		}
		if (*cp == endc && eqFound) {

		    /* Now we break this sucker into the lhs and
		     * rhs. We must null terminate them of course.  */
		    for (cp = tstr; *cp != '='; cp++)
			continue;
		    pattern.lhs = tstr;
		    pattern.leftLen = cp - tstr;
		    *cp++ = '\0';

		    pattern.rhs = cp;
		    cnt = 1;
		    while (cnt) {
			if (*cp == endc)
			    cnt--;
			else if (*cp == startc)
			    cnt++;
			if (cnt)
			    cp++;
		    }
		    pattern.rightLen = cp - pattern.rhs;
		    *cp = '\0';

		    /* SYSV modifications happen through the whole
		     * string. Note the pattern is anchored at the end.  */
		    newStr = VarModify(str, VarSYSVMatch, &pattern);

		    /* Restore the nulled characters */
		    pattern.lhs[pattern.leftLen] = '=';
		    pattern.rhs[pattern.rightLen] = endc;
		    termc = endc;
		} else
#endif
		{
		    Error ("Unknown modifier '%c'\n", *tstr);
		    for (cp = tstr+1;
			 *cp != ':' && *cp != endc && *cp != '\0';)
			 cp++;
		    termc = *cp;
		    newStr = var_Error;
		}
	    }
	}
	if (DEBUG(VAR))
	    printf("Result is \"%s\"\n", newStr != NULL ? newStr : "");

	if (*freePtr)
	    free(str);
	str = newStr;
	if (str != var_Error && str != NULL)
	    *freePtr = TRUE;
	else
	    *freePtr = FALSE;
	if (termc == '\0')
	    Error("Unclosed variable specification");
	else if (termc == ':')
	    *cp++ = termc;
	else
	    *cp = termc;
	tstr = cp;
    }
    *lengthPtr += tstr - start+1;
    return str;

cleanup:
    *lengthPtr += cp - start +1;
    if (*freePtr)
	free(str);
    Error("Unclosed substitution for (%c missing)", delim);
    return var_Error;
}

/*-
 *-----------------------------------------------------------------------
 * VarQuote --
 *	Quote shell meta-characters in the string
 *
 * Results:
 *	The quoted string
 *
 *-----------------------------------------------------------------------
 */
static char *
VarQuote(str)
	const char *str;
{

    BUFFER  	  buf;
    /* This should cover most shells :-( */
    static char meta[] = "\n \t'`\";&<>()|*?{}[]\\$!#^~";

    if (str == NULL)
    	return NULL;

    Buf_Init(&buf, MAKE_BSIZE);
    for (; *str; str++) {
	if (strchr(meta, *str) != NULL)
	    Buf_AddChar(&buf, '\\');
	Buf_AddChar(&buf, *str);
    }
    return Buf_Retrieve(&buf);
}
/*-
 *-----------------------------------------------------------------------
 * Var_GetHead --
 *	Find the leading components of a (list of) filename(s).
 *	XXX: VarHead does not replace foo by ., as (sun) System V make
 *	does.
 *
 * Results:
 *	The leading components.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_GetHead(file)
    char    	*file;	    /* Filename to manipulate */
{
    return VarModify(file, VarHead, NULL);
}

/*-
 *-----------------------------------------------------------------------
 * Var_GetTail --
 *	Return the tail from each of a list of words. Used to set the
 *	System V local variables.
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_GetTail(file)
    char    	*file;	    /* Filename to modify */
{
    return VarModify(file, VarTail, NULL);
}

