%{
/*	$OpenBSD: parse.lex,v 1.10 1999/03/29 20:29:05 millert Exp $	*/

/*
 *  CU sudo version 1.5.9
 *  Copyright (c) 1996, 1998, 1999 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 *******************************************************************
 *
 * parse.lex -- lexigraphical analyzer for sudo.
 *
 * Chris Jepeway <jepeway@cs.utk.edu>
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
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
#include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "sudo.h"
#include "sudo.tab.h"

#ifndef lint
static const char rcsid[] = "$Sudo: parse.lex,v 1.88 1999/03/29 04:05:10 millert Exp $";
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

OCTET			[[:digit:]]{1,3}
DOTTEDQUAD		{OCTET}(\.{OCTET}){3}
WORD			[[:alnum:]_-]+

%e	4000
%p	6000
%k	3500

%s	GOTCMND
%s	GOTRUNAS

%%
[ \t]+			{			/* throw away space/tabs */
			    sawspace = TRUE;	/* but remember for fill_args */
			}

\\[ \t]*\n		{ 
			    sawspace = TRUE;	/* remember for fill_args */
			    ++sudolineno;
			    LEXTRACE("\n\t");
			}			/* throw away EOL after \ */

<GOTCMND>\\[:\,=\\ \t] {
			    LEXTRACE("QUOTEDCHAR ");
			    fill_args(yytext + 1, 1, sawspace);
			    sawspace = FALSE;
			}

<GOTCMND>[:\,=\n]	{
			    BEGIN INITIAL;
			    unput(*yytext);
			    return(COMMAND);
			}			/* end of command line args */

\n			{ 
			    ++sudolineno; 
			    LEXTRACE("\n");
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

\,			{
			    LEXTRACE(", ");
			    return(',');
			}			/* return ',' */

\!			{
			    return('!');		/* return '!' */
			}

=			{
			    LEXTRACE("= ");
			    return('=');
			}			/* return '=' */

:			{
			    LEXTRACE(": ");
			    return(':');
			}			/* return ':' */

\.			{
			    return('.');
			}

NOPASSWD[[:blank:]]*:	{ 
				/* cmnd does not require passwd for this user */
			    	LEXTRACE("NOPASSWD ");
			    	return(NOPASSWD);
			}

\+{WORD}		{
			    /* netgroup */
			    fill(yytext, yyleng);
			    return(NETGROUP);
			}

\%{WORD}		{
			    /* UN*X group */
			    fill(yytext, yyleng);
			    return(USERGROUP);
			}

{DOTTEDQUAD}(\/{DOTTEDQUAD})? {
			    fill(yytext, yyleng);
			    LEXTRACE("NTWKADDR ");
			    return(NTWKADDR);
			}

[[:alpha:]][[:alnum:]_-]*(\.{WORD})+ {
			    fill(yytext, yyleng);
			    LEXTRACE("FQHOST ");
			    return(FQHOST);
			}

<INITIAL>\(		{
				BEGIN GOTRUNAS;
				LEXTRACE("RUNAS ");
				return (RUNAS);
			}

<GOTRUNAS>[[:upper:]][[:upper:][:digit:]_]* {
			    /* Runas_Alias that user can run command as or ALL */
			    fill(yytext, yyleng);
			    if (strcmp(yytext, "ALL") == 0) {
				LEXTRACE("ALL ");
				return(ALL);
			    } else {
				LEXTRACE("ALIAS ");
				return(ALIAS);
			    }
			}

<GOTRUNAS>#?{WORD}	{
			    /* username/uid that user can run command as */
			    fill(yytext, yyleng);
			    LEXTRACE("NAME ");
			    return(NAME);
			}

<GOTRUNAS>\)		BEGIN INITIAL;


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

[[:upper:]][[:upper:][:digit:]_]*	{
			    fill(yytext, yyleng);
			    if (strcmp(yytext, "ALL") == 0) {
				LEXTRACE("ALL ");
				return(ALL);
			    }
			    LEXTRACE("ALIAS ");
			    return(ALIAS);
			}

[[:alnum:]][[:alnum:]_-]*	{
			    fill(yytext, yyleng);
			    if (strcmp(yytext, "Host_Alias") == 0) {
				LEXTRACE("HOSTALIAS ");
				return(HOSTALIAS);
			    }
			    if (strcmp(yytext, "Cmnd_Alias") == 0) {
				LEXTRACE("CMNDALIAS ");
				return(CMNDALIAS);
			    }
			    if (strcmp(yytext, "User_Alias") == 0) {
				LEXTRACE("USERALIAS ");
				return(USERALIAS);
			    }
			    if (strcmp(yytext, "Runas_Alias") == 0) {
				LEXTRACE("RUNASALIAS ");
				return(RUNASALIAS);
			    }

			    /* NAME is what RFC1034 calls a label */
			    LEXTRACE("NAME ");
			    return(NAME);
			}

.			{
			    return(ERROR);
			}	/* parse error */

%%
static void fill(s, len)
    char *s;
    int len;
{
    yylval.string = (char *) malloc(len + 1);
    if (yylval.string == NULL)
	yyerror("unable to allocate memory");

    /* copy the string and NULL-terminate it */
    (void) strncpy(yylval.string, s, len);
    yylval.string[len] = '\0';
}


static void fill_cmnd(s, len)
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


static void fill_args(s, len, addspace)
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
		(void) free(yylval.command.args);
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


int yywrap()
{
#ifdef YY_NEW_FILE
    YY_NEW_FILE;
#endif /* YY_NEW_FILE */

    /* don't reset the aliases if called by testsudoers */
    if (clearaliases)
	reset_aliases();

    return(TRUE);
}
