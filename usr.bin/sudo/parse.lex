%{
/*
 * Copyright (c) 1996, 1998, 1999 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * This code is derived from software contributed by Chris Jepeway
 * <jepeway@cs.utk.edu>
 *
 * This code is derived from software contributed by Chris Jepeway
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
#include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include "sudo.h"
#include "parse.h"
#include "sudo.tab.h"

#ifndef lint
static const char rcsid[] = "$Sudo: parse.lex,v 1.111 2000/03/23 04:38:20 millert Exp $";
#endif /* lint */

#undef yywrap		/* guard against a yywrap macro */

extern YYSTYPE yylval;
extern int clearaliases;
int sudolineno = 1;
static int sawspace = 0;
static int arg_len = 0;
static int arg_size = 0;

static void fill		__P((char *, int));
static void fill_cmnd		__P((char *, int));
static void fill_args		__P((char *, int, int));
extern void reset_aliases	__P((void));
extern void yyerror		__P((char *));

/* realloc() to size + COMMANDARGINC to make room for command args */
#define COMMANDARGINC	64

#ifdef TRACELEXER
#define LEXTRACE(msg)	fputs(msg, stderr)
#else
#define LEXTRACE(msg)
#endif
%}

OCTET			(1?[0-9]{1,2})|(2[0-4][0-9])|(25[0-5])
DOTTEDQUAD		{OCTET}(\.{OCTET}){3}
HOSTNAME		[[:alnum:]_-]+
WORD			([^@!=:,\(\) \t\n\\]|\\[^\n])+

%s	GOTCMND
%s	GOTRUNAS
%s	GOTDEFS

%%
[ \t]+			{			/* throw away space/tabs */
			    sawspace = TRUE;	/* but remember for fill_args */
			}

\\[ \t]*\n		{
			    sawspace = TRUE;	/* remember for fill_args */
			    ++sudolineno;
			    LEXTRACE("\n\t");
			}			/* throw away EOL after \ */

<GOTCMND>\\[:\,=\\ \t]	{
			    LEXTRACE("QUOTEDCHAR ");
			    fill_args(yytext + 1, 1, sawspace);
			    sawspace = FALSE;
			}

<GOTDEFS>\"([^\"]|\\\")+\"	{
			    LEXTRACE("WORD(1) ");
			    fill(yytext + 1, yyleng - 2);
			    return(WORD);
			}

<GOTDEFS>(#.*)?\n	{
			    BEGIN INITIAL;
			    ++sudolineno;
			    LEXTRACE("\n");
			    return(COMMENT);
			}

<GOTCMND>[:\,=\n]	{
			    BEGIN INITIAL;
			    unput(*yytext);
			    return(COMMAND);
			}			/* end of command line args */

\n			{
			    ++sudolineno;
			    LEXTRACE("\n");
			    BEGIN INITIAL;
			    return(COMMENT);
			}			/* return newline */

<INITIAL>#.*\n		{
			    ++sudolineno;
			    LEXTRACE("\n");
			    return(COMMENT);
			}			/* return comments */

<GOTCMND>[^\\:, \t\n]+ {
			    LEXTRACE("ARG ");
			    fill_args(yytext, yyleng, sawspace);
			    sawspace = FALSE;
			  }			/* a command line arg */

,			{
			    LEXTRACE(", ");
			    return(',');
			}			/* return ',' */

!+			{
			    if (yyleng % 2 == 1)
				return('!');	/* return '!' */
			}

=			{
			    LEXTRACE("= ");
			    return('=');
			}			/* return '=' */

:			{
			    LEXTRACE(": ");
			    return(':');
			}			/* return ':' */

NOPASSWD[[:blank:]]*:	{
				/* cmnd does not require passwd for this user */
			    	LEXTRACE("NOPASSWD ");
			    	return(NOPASSWD);
			}

PASSWD[[:blank:]]*:	{
				/* cmnd requires passwd for this user */
			    	LEXTRACE("PASSWD ");
			    	return(PASSWD);
			}

\+{WORD}		{
			    /* netgroup */
			    fill(yytext, yyleng);
			    LEXTRACE("NETGROUP ");
			    return(NETGROUP);
			}

\%{WORD}		{
			    /* UN*X group */
			    fill(yytext, yyleng);
			    LEXTRACE("GROUP ");
			    return(USERGROUP);
			}

{DOTTEDQUAD}(\/{DOTTEDQUAD})? {
			    fill(yytext, yyleng);
			    LEXTRACE("NTWKADDR ");
			    return(NTWKADDR);
			}

{DOTTEDQUAD}\/([12][0-9]*|3[0-2]*) {
			    fill(yytext, yyleng);
			    LEXTRACE("NTWKADDR ");
			    return(NTWKADDR);
			}

<INITIAL>\(		{
				BEGIN GOTRUNAS;
				LEXTRACE("RUNAS ");
				return (RUNAS);
			}

<GOTRUNAS>[[:upper:]][[:upper:][:digit:]_]* {
			    /* Runas_Alias user can run command as or ALL */
			    if (strcmp(yytext, "ALL") == 0) {
				LEXTRACE("ALL ");
				return(ALL);
			    } else {
				fill(yytext, yyleng);
				LEXTRACE("ALIAS ");
				return(ALIAS);
			    }
			}

<GOTRUNAS>#?{WORD}	{
			    /* username/uid that user can run command as */
			    fill(yytext, yyleng);
			    LEXTRACE("WORD(2) ");
			    return(WORD);
			}

<GOTRUNAS>\)		{
			    BEGIN INITIAL;
			}

[[:upper:]][[:upper:][:digit:]_]*	{
			    if (strcmp(yytext, "ALL") == 0) {
				LEXTRACE("ALL ");
				return(ALL);
			    } else {
				fill(yytext, yyleng);
				LEXTRACE("ALIAS ");
				return(ALIAS);
			    }
			}

<GOTDEFS>{WORD}		{
			    LEXTRACE("WORD(3) ");
			    fill(yytext, yyleng);
			    return(WORD);
			}

<INITIAL>^Defaults[:@]?	{
			    BEGIN GOTDEFS;
			    if (yyleng == 9) {
				switch (yytext[8]) {
				    case ':' :
					LEXTRACE("DEFAULTS_USER ");
					return(DEFAULTS_USER);
				    case '@' :
					LEXTRACE("DEFAULTS_HOST ");
					return(DEFAULTS_HOST);
				}
			    } else {
				LEXTRACE("DEFAULTS ");
				return(DEFAULTS);
			    }
			}

<INITIAL>^(Host|Cmnd|User|Runas)_Alias	{
			    fill(yytext, yyleng);
			    if (*yytext == 'H') {
				LEXTRACE("HOSTALIAS ");
				return(HOSTALIAS);
			    }
			    if (*yytext == 'C') {
				LEXTRACE("CMNDALIAS ");
				return(CMNDALIAS);
			    }
			    if (*yytext == 'U') {
				LEXTRACE("USERALIAS ");
				return(USERALIAS);
			    }
			    if (*yytext == 'R') {
				LEXTRACE("RUNASALIAS ");
				BEGIN GOTRUNAS;
				return(RUNASALIAS);
			    }
			}

\/[^\,:=\\ \t\n#]+	{
			    /* directories can't have args... */
			    if (yytext[yyleng - 1] == '/') {
				LEXTRACE("COMMAND ");
				fill_cmnd(yytext, yyleng);
				return(COMMAND);
			    } else {
				BEGIN GOTCMND;
				LEXTRACE("COMMAND ");
				fill_cmnd(yytext, yyleng);
			    }
			}			/* a pathname */

<INITIAL>{WORD}		{
			    /* a word */
			    fill(yytext, yyleng);
			    LEXTRACE("WORD(4) ");
			    return(WORD);
			}

.			{
			    LEXTRACE("ERROR ");
			    return(ERROR);
			}	/* parse error */

%%
static void
fill(s, len)
    char *s;
    int len;
{
    int i, j;

    yylval.string = (char *) malloc(len + 1);
    if (yylval.string == NULL)
	yyerror("unable to allocate memory");

    /* Copy the string and collapse any escaped characters. */
    for (i = 0, j = 0; i < len; i++, j++) {
	if (s[i] == '\\' && i != len - 1)
	    yylval.string[j] = s[++i];
	else
	    yylval.string[j] = s[i];
    }
    yylval.string[j] = '\0';
}

static void
fill_cmnd(s, len)
    char *s;
    int len;
{
    arg_len = arg_size = 0;

    yylval.command.cmnd = (char *) malloc(len + 1);
    if (yylval.command.cmnd == NULL)
	yyerror("unable to allocate memory");

    /* copy the string and NULL-terminate it */
    (void) strncpy(yylval.command.cmnd, s, len);
    yylval.command.cmnd[len] = '\0';

    yylval.command.args = NULL;
}

static void
fill_args(s, len, addspace)
    char *s;
    int len;
    int addspace;
{
    int new_len;
    char *p;

    /*
     * If first arg, malloc() some room, else if we don't
     * have enough space realloc() some more.
     */
    if (yylval.command.args == NULL) {
	addspace = 0;
	new_len = len;

	while (new_len >= (arg_size += COMMANDARGINC))
	    ;

	yylval.command.args = (char *) malloc(arg_size);
	if (yylval.command.args == NULL)
	    yyerror("unable to allocate memory");
    } else {
	new_len = arg_len + len + addspace;

	if (new_len >= arg_size) {
	    /* Allocate more space than we need for subsequent args */
	    while (new_len >= (arg_size += COMMANDARGINC))
		;

	    if ((p = (char *) realloc(yylval.command.args, arg_size)) == NULL) {
		free(yylval.command.args);
		yyerror("unable to allocate memory");
	    } else
		yylval.command.args = p;
	}
    }

    /* Efficiently append the arg (with a leading space if needed). */
    p = yylval.command.args + arg_len;
    if (addspace)
	*p++ = ' ';
    (void) strcpy(p, s);
    arg_len = new_len;
}

int
yywrap()
{

    /* Free space used by the aliases unless called by testsudoers. */
    if (clearaliases)
	reset_aliases();

    return(TRUE);
}
