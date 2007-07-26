%{
/*
 * Copyright (c) 1996, 1998-2004, 2007
 *	Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
# include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#include <ctype.h>
#include "sudo.h"
#include "parse.h"
#include <sudo.tab.h>

#ifndef lint
__unused static const char rcsid[] = "$Sudo: parse.lex,v 1.132.2.3 2007/06/23 21:36:48 millert Exp $";
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
WORD			([^#>@!=:,\(\) \t\n\\]|\\[^\n])+
ENVAR			([^#!=, \t\n\\]|\\[^\n])([^#=, \t\n\\]|\\[^\n])*
DEFVAR			[a-z_]+

/* XXX - convert GOTRUNAS to exclusive state (GOTDEFS cannot be) */
%s	GOTRUNAS
%s	GOTDEFS
%x	GOTCMND
%x	STARTDEFS
%x	INDEFS

%%
<GOTDEFS>[[:blank:]]+	BEGIN STARTDEFS;

<STARTDEFS>{DEFVAR}	{
			    BEGIN INDEFS;
			    LEXTRACE("DEFVAR ");
			    fill(yytext, yyleng);
			    return(DEFVAR);
			}

<INDEFS>{
    ,			{
			    BEGIN STARTDEFS;
			    LEXTRACE(", ");
			    return(',');
			}			/* return ',' */

    =			{
			    LEXTRACE("= ");
			    return('=');
			}			/* return '=' */

    \+=			{
			    LEXTRACE("+= ");
			    return('+');
			}			/* return '+' */

    -=			{
			    LEXTRACE("-= ");
			    return('-');
			}			/* return '-' */

    \"([^\"]|\\\")+\"	{
			    LEXTRACE("WORD(1) ");
			    fill(yytext + 1, yyleng - 2);
			    return(WORD);
			}

    {ENVAR}		{
			    LEXTRACE("WORD(2) ");
			    fill(yytext, yyleng);
			    return(WORD);
			}
}

<GOTCMND>{
    \\[\*\?\[\]\!]	{
			    /* quoted fnmatch glob char, pass verbatim */
			    LEXTRACE("QUOTEDCHAR ");
			    fill_args(yytext, 2, sawspace);
			    sawspace = FALSE;
			}

    \\[:\\,= \t#]	{
			    /* quoted sudoers special char, strip backslash */
			    LEXTRACE("QUOTEDCHAR ");
			    fill_args(yytext + 1, 1, sawspace);
			    sawspace = FALSE;
			}

    [#:\,=\n]		{
			    BEGIN INITIAL;
			    unput(*yytext);
			    return(COMMAND);
			}			/* end of command line args */

    [^\\:, \t\n]+ 	{
			    LEXTRACE("ARG ");
			    fill_args(yytext, yyleng, sawspace);
			    sawspace = FALSE;
			}			/* a command line arg */
}

<INITIAL>^Defaults[:@>]? {
			    BEGIN GOTDEFS;
			    switch (yytext[8]) {
				case ':':
				    LEXTRACE("DEFAULTS_USER ");
				    return(DEFAULTS_USER);
				case '>':
				    LEXTRACE("DEFAULTS_RUNAS ");
				    return(DEFAULTS_RUNAS);
				case '@':
				    LEXTRACE("DEFAULTS_HOST ");
				    return(DEFAULTS_HOST);
				default:
				    LEXTRACE("DEFAULTS ");
				    return(DEFAULTS);
			    }
			}

<INITIAL>^(Host|Cmnd|User|Runas)_Alias	{
			    fill(yytext, yyleng);
			    switch (*yytext) {
				case 'H':
				    LEXTRACE("HOSTALIAS ");
				    return(HOSTALIAS);
				case 'C':
				    LEXTRACE("CMNDALIAS ");
				    return(CMNDALIAS);
				case 'U':
				    LEXTRACE("USERALIAS ");
				    return(USERALIAS);
				case 'R':
				    LEXTRACE("RUNASALIAS ");
				    BEGIN GOTRUNAS;
				    return(RUNASALIAS);
			    }
			}

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

NOEXEC[[:blank:]]*:	{
			    	LEXTRACE("NOEXEC ");
			    	return(NOEXEC);
			}

EXEC[[:blank:]]*:	{
			    	LEXTRACE("EXEC ");
			    	return(EXEC);
			}

SETENV[[:blank:]]*:	{
			    	LEXTRACE("SETENV ");
			    	return(SETENV);
			}

NOSETENV[[:blank:]]*:	{
			    	LEXTRACE("NOSETENV ");
			    	return(NOSETENV);
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

[[:upper:]][[:upper:][:digit:]_]* {
			    if (strcmp(yytext, "ALL") == 0) {
				LEXTRACE("ALL ");
				return(ALL);
			    } else {
				fill(yytext, yyleng);
				LEXTRACE("ALIAS ");
				return(ALIAS);
			    }
			}

<GOTRUNAS>(#[0-9-]+|{WORD}) {
			    /* username/uid that user can run command as */
			    fill(yytext, yyleng);
			    LEXTRACE("WORD(3) ");
			    return(WORD);
			}

<GOTRUNAS>\)		{
			    BEGIN INITIAL;
			}

sudoedit		{
			    BEGIN GOTCMND;
			    LEXTRACE("COMMAND ");
			    fill_cmnd(yytext, yyleng);
			}			/* sudo -e */

\/(\\[\,:= \t#]|[^\,:=\\ \t\n#])+	{
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

<INITIAL,GOTDEFS>{WORD} {
			    /* a word */
			    fill(yytext, yyleng);
			    LEXTRACE("WORD(4) ");
			    return(WORD);
			}

,			{
			    LEXTRACE(", ");
			    return(',');
			}			/* return ',' */

=			{
			    LEXTRACE("= ");
			    return('=');
			}			/* return '=' */

:			{
			    LEXTRACE(": ");
			    return(':');
			}			/* return ':' */

<*>!+			{
			    if (yyleng % 2 == 1)
				return('!');	/* return '!' */
			}

<*>\n			{
			    BEGIN INITIAL;
			    ++sudolineno;
			    LEXTRACE("\n");
			    return(COMMENT);
			}			/* return newline */

<*>[[:blank:]]+		{			/* throw away space/tabs */
			    sawspace = TRUE;	/* but remember for fill_args */
			}

<*>\\[[:blank:]]*\n	{
			    sawspace = TRUE;	/* remember for fill_args */
			    ++sudolineno;
			    LEXTRACE("\n\t");
			}			/* throw away EOL after \ */

<INITIAL,STARTDEFS,INDEFS>#.*\n	{
			    BEGIN INITIAL;
			    ++sudolineno;
			    LEXTRACE("\n");
			    return(COMMENT);
			}			/* return comments */

<*>.			{
			    LEXTRACE("ERROR ");
			    return(ERROR);
			}	/* parse error */

<*><<EOF>>		{
			    if (YY_START != INITIAL) {
			    	BEGIN INITIAL;
				LEXTRACE("ERROR ");
				return(ERROR);
			    }
			    yyterminate();
			}

%%
static void
fill(s, len)
    char *s;
    int len;
{
    int i, j;

    yylval.string = (char *) malloc(len + 1);
    if (yylval.string == NULL) {
	yyerror("unable to allocate memory");
	return;
    }

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

    yylval.command.cmnd = (char *) malloc(++len);
    if (yylval.command.cmnd == NULL) {
	yyerror("unable to allocate memory");
	return;
    }

    /* copy the string and NULL-terminate it (escapes handled by fnmatch) */
    (void) strlcpy(yylval.command.cmnd, s, len);

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

    if (yylval.command.args == NULL) {
	addspace = 0;
	new_len = len;
    } else
	new_len = arg_len + len + addspace;

    if (new_len >= arg_size) {
	/* Allocate more space than we need for subsequent args */
	while (new_len >= (arg_size += COMMANDARGINC))
	    ;

	p = yylval.command.args ?
	    (char *) realloc(yylval.command.args, arg_size) :
	    (char *) malloc(arg_size);
	if (p == NULL) {
	    efree(yylval.command.args);
	    yyerror("unable to allocate memory");
	    return;
	} else
	    yylval.command.args = p;
    }

    /* Efficiently append the arg (with a leading space if needed). */
    p = yylval.command.args + arg_len;
    if (addspace)
	*p++ = ' ';
    if (strlcpy(p, s, arg_size - (p - yylval.command.args)) != len)
	yyerror("fill_args: buffer overflow");	/* paranoia */
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
