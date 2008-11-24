%{
/*
 * Copyright (c) 1996, 1998-2005, 2007-2008
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
#include <gram.h>

#ifndef lint
__unused static const char rcsid[] = "$Sudo: toke.l,v 1.27 2008/11/24 00:41:36 millert Exp $";
#endif /* lint */

extern YYSTYPE yylval;
int sudolineno = 1;
char *sudoers;
static int sawspace = 0;
static int arg_len = 0;
static int arg_size = 0;

static int append		__P((char *, int));
static int _fill		__P((char *, int, int));
static int fill_cmnd		__P((char *, int));
static int fill_args		__P((char *, int, int));
static int switch_buffer	__P((char *));
static int ipv6_valid		__P((const char *s));
static char *parse_include	__P((char *));
extern void yyerror		__P((const char *));

#define fill(a, b)		_fill(a, b, 0)

#define	push_include(_p)	(switch_buffer((_p)))
#define	pop_include()		(switch_buffer(NULL))

/* realloc() to size + COMMANDARGINC to make room for command args */
#define COMMANDARGINC	64

#ifdef TRACELEXER
#define LEXTRACE(msg)	fputs(msg, stderr)
#else
#define LEXTRACE(msg)
#endif
%}

HEX16			[0-9A-Fa-f]{1,4}
OCTET			(1?[0-9]{1,2})|(2[0-4][0-9])|(25[0-5])
IPV4ADDR		{OCTET}(\.{OCTET}){3}
IPV6ADDR		({HEX16}?:){2,7}{HEX16}?|({HEX16}?:){2,6}:{IPV4ADDR}

HOSTNAME		[[:alnum:]_-]+
WORD			([^#>!=:,\(\) \t\n\\]|\\[^\n])+
ID			#-?[0-9]+
PATH			\/(\\[\,:= \t#]|[^\,:=\\ \t\n#])+
ENVAR			([^#!=, \t\n\\\"]|\\[^\n])([^#=, \t\n\\\"]|\\[^\n])*
DEFVAR			[a-z_]+

%option nounput
%option noyywrap

%s	GOTDEFS
%x	GOTCMND
%x	STARTDEFS
%x	INDEFS
%x	INSTR

%%
<GOTDEFS>[[:blank:]]+	BEGIN STARTDEFS;

<STARTDEFS>{DEFVAR}	{
			    BEGIN INDEFS;
			    LEXTRACE("DEFVAR ");
			    if (!fill(yytext, yyleng))
				yyterminate();
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

    \"			{
			    LEXTRACE("BEGINSTR ");
			    yylval.string = NULL;
			    BEGIN INSTR;
			}

    {ENVAR}		{
			    LEXTRACE("WORD(2) ");
			    if (!fill(yytext, yyleng))
				yyterminate();
			    return(WORD);
			}
}

<INSTR>{
    \\[[:blank:]]*\n[[:blank:]]*	{
			    /* Line continuation char followed by newline. */
			    ++sudolineno;
			    LEXTRACE("\n");
			}

    \"			{
			    LEXTRACE("ENDSTR ");
			    BEGIN INDEFS;
			    return(WORD);
			}

    \\			{
			    LEXTRACE("BACKSLASH ");
			    if (!append(yytext, yyleng))
				yyterminate();
			}

    ([^\"\n\\]|\\\")+	{
			    LEXTRACE("STRBODY ");
			    if (!append(yytext, yyleng))
				yyterminate();
			}
}

<GOTCMND>{
    \\[\*\?\[\]\!]	{
			    /* quoted fnmatch glob char, pass verbatim */
			    LEXTRACE("QUOTEDCHAR ");
			    if (!fill_args(yytext, 2, sawspace))
				yyterminate();
			    sawspace = FALSE;
			}

    \\[:\\,= \t#]	{
			    /* quoted sudoers special char, strip backslash */
			    LEXTRACE("QUOTEDCHAR ");
			    if (!fill_args(yytext + 1, 1, sawspace))
				yyterminate();
			    sawspace = FALSE;
			}

    [#:\,=\n]		{
			    BEGIN INITIAL;
			    yyless(0);
			    return(COMMAND);
			}			/* end of command line args */

    [^\\:, \t\n]+ 	{
			    LEXTRACE("ARG ");
			    if (!fill_args(yytext, yyleng, sawspace))
				yyterminate();
			    sawspace = FALSE;
			}			/* a command line arg */
}

<INITIAL>^#include[[:blank:]]+\/.*\n {
			    char *path;

			    if ((path = parse_include(yytext)) == NULL)
				yyterminate();

			    LEXTRACE("INCLUDE\n");

			    /* Push current buffer and switch to include file */
			    if (!push_include(path))
				yyterminate();
			}

<INITIAL>^[[:blank:]]*Defaults([:@>\!]{WORD})? {
			    int n;
			    for (n = 0; isblank((unsigned char)yytext[n]); n++)
				continue;
			    n += 8;
			    BEGIN GOTDEFS;
			    switch (yytext[n++]) {
				case ':':
				    yyless(n);
				    LEXTRACE("DEFAULTS_USER ");
				    return(DEFAULTS_USER);
				case '>':
				    yyless(n);
				    LEXTRACE("DEFAULTS_RUNAS ");
				    return(DEFAULTS_RUNAS);
				case '@':
				    yyless(n);
				    LEXTRACE("DEFAULTS_HOST ");
				    return(DEFAULTS_HOST);
				case '!':
				    yyless(n);
				    LEXTRACE("DEFAULTS_CMND ");
				    return(DEFAULTS_CMND);
				default:
				    LEXTRACE("DEFAULTS ");
				    return(DEFAULTS);
			    }
			}

<INITIAL>^[[:blank:]]*(Host|Cmnd|User|Runas)_Alias	{
			    int n;
			    for (n = 0; isblank((unsigned char)yytext[n]); n++)
				continue;
			    switch (yytext[n]) {
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
			    if (!fill(yytext, yyleng))
				yyterminate();
			    LEXTRACE("NETGROUP ");
			    return(NETGROUP);
			}

\%{WORD}		{
			    /* UN*X group */
			    if (!fill(yytext, yyleng))
				yyterminate();
			    LEXTRACE("USERGROUP ");
			    return(USERGROUP);
			}

{IPV4ADDR}(\/{IPV4ADDR})? {
			    if (!fill(yytext, yyleng))
				yyterminate();
			    LEXTRACE("NTWKADDR ");
			    return(NTWKADDR);
			}

{IPV4ADDR}\/([12][0-9]*|3[0-2]*) {
			    if (!fill(yytext, yyleng))
				yyterminate();
			    LEXTRACE("NTWKADDR ");
			    return(NTWKADDR);
			}

{IPV6ADDR}(\/{IPV6ADDR})? {
			    if (!ipv6_valid(yytext)) {
				LEXTRACE("ERROR ");
				return(ERROR);
			    }
			    if (!fill(yytext, yyleng))
				yyterminate();
			    LEXTRACE("NTWKADDR ");
			    return(NTWKADDR);
			}

{IPV6ADDR}\/([0-9]|[1-9][0-9]|1[01][0-9]|12[0-8]) {
			    if (!ipv6_valid(yytext)) {
				LEXTRACE("ERROR ");
				return(ERROR);
			    }
			    if (!fill(yytext, yyleng))
				yyterminate();
			    LEXTRACE("NTWKADDR ");
			    return(NTWKADDR);
			}

[[:upper:]][[:upper:][:digit:]_]* {
			    if (strcmp(yytext, "ALL") == 0) {
				LEXTRACE("ALL ");
				return(ALL);
			    }
#ifdef HAVE_SELINUX
			    /* XXX - restrict type/role to initial state */
			    if (strcmp(yytext, "TYPE") == 0) {
				LEXTRACE("TYPE ");
				return(TYPE);
			    }
			    if (strcmp(yytext, "ROLE") == 0) {
				LEXTRACE("ROLE ");
				return(ROLE);
			    }
#endif /* HAVE_SELINUX */
			    if (!fill(yytext, yyleng))
				yyterminate();
			    LEXTRACE("ALIAS ");
			    return(ALIAS);
			}

<GOTDEFS>({PATH}|sudoedit) {
			    /* no command args allowed for Defaults!/path */
			    if (!fill_cmnd(yytext, yyleng))
				yyterminate();
			    LEXTRACE("COMMAND ");
			    return(COMMAND);
			}

sudoedit		{
			    BEGIN GOTCMND;
			    LEXTRACE("COMMAND ");
			    if (!fill_cmnd(yytext, yyleng))
				yyterminate();
			}			/* sudo -e */

{PATH}			{
			    /* directories can't have args... */
			    if (yytext[yyleng - 1] == '/') {
				LEXTRACE("COMMAND ");
				if (!fill_cmnd(yytext, yyleng))
				    yyterminate();
				return(COMMAND);
			    } else {
				BEGIN GOTCMND;
				LEXTRACE("COMMAND ");
				if (!fill_cmnd(yytext, yyleng))
				    yyterminate();
			    }
			}			/* a pathname */

<INITIAL,GOTDEFS>({ID}|{WORD}) {
			    /* a word */
			    if (!fill(yytext, yyleng))
				yyterminate();
			    LEXTRACE("WORD(4) ");
			    return(WORD);
			}

\(			{
			    LEXTRACE("( ");
			    return ('(');
			}

\)			{
			    LEXTRACE(") ");
			    return(')');
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

<INITIAL,STARTDEFS,INDEFS>#([^\n0-9-].*)?\n	{
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
			    if (!pop_include())
				yyterminate();
			}

%%
static int
_fill(src, len, olen)
    char *src;
    int len, olen;
{
    int i, j;
    char *dst;

    dst = olen ? realloc(yylval.string, olen + len + 1) : malloc(len + 1);
    if (dst == NULL) {
	yyerror("unable to allocate memory");
	return(FALSE);
    }
    yylval.string = dst;

    /* Copy the string and collapse any escaped characters. */
    dst += olen;
    for (i = 0, j = 0; i < len; i++, j++) {
	if (src[i] == '\\' && i != len - 1)
	    dst[j] = src[++i];
	else
	    dst[j] = src[i];
    }
    dst[j] = '\0';
    return(TRUE);
}

static int
append(src, len)
    char *src;
    int len;
{
    int olen = 0;

    if (yylval.string != NULL)
	olen = strlen(yylval.string);

    return(_fill(src, len, olen));
}

#define SPECIAL(c) \
    ((c) == ',' || (c) == ':' || (c) == '=' || (c) == ' ' || (c) == '\t' || (c) == '#')

static int
fill_cmnd(src, len)
    char *src;
    int len;
{
    char *dst;
    int i;

    arg_len = arg_size = 0;

    dst = yylval.command.cmnd = (char *) malloc(len + 1);
    if (yylval.command.cmnd == NULL) {
	yyerror("unable to allocate memory");
	return(FALSE);
    }

    /* Copy the string and collapse any escaped sudo-specific characters. */
    for (i = 0; i < len; i++) {
	if (src[i] == '\\' && i != len - 1 && SPECIAL(src[i + 1]))
	    *dst++ = src[++i];
	else
	    *dst++ = src[i];
    }
    *dst = '\0';

    yylval.command.args = NULL;
    return(TRUE);
}

static int
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
	    return(FALSE);
	} else
	    yylval.command.args = p;
    }

    /* Efficiently append the arg (with a leading space if needed). */
    p = yylval.command.args + arg_len;
    if (addspace)
	*p++ = ' ';
    if (strlcpy(p, s, arg_size - (p - yylval.command.args)) != len) {
	yyerror("fill_args: buffer overflow");	/* paranoia */
	return(FALSE);
    }
    arg_len = new_len;
    return(TRUE);
}

struct sudoers_state {
    YY_BUFFER_STATE bs;
    char *path;
    int lineno;
};

#define MAX_SUDOERS_DEPTH	128
#define SUDOERS_STACK_INCREMENT	16

static int
switch_buffer(path)
    char *path;
{
    static size_t stacksize, depth;
    static struct sudoers_state *state;
    static int keepopen;
    FILE *fp;

    if (path != NULL) {
	/* push current state */
	if (depth >= stacksize) {
	    if (depth > MAX_SUDOERS_DEPTH) {
		yyerror("too many levels of includes");
		return(FALSE);
	    }
	    stacksize += SUDOERS_STACK_INCREMENT;
	    state = (struct sudoers_state *) realloc(state,
		sizeof(state) * stacksize);
	    if (state == NULL) {
		yyerror("unable to allocate memory");
		return(FALSE);
	    }
	}
	if ((fp = open_sudoers(path, &keepopen)) == NULL) {
	    yyerror(path);
	    return(FALSE);
	}
	state[depth].bs = YY_CURRENT_BUFFER;
	state[depth].path = sudoers;
	state[depth].lineno = sudolineno;
	depth++;
	sudolineno = 1;
	sudoers = path;
	yy_switch_to_buffer(yy_create_buffer(fp, YY_BUF_SIZE));
    } else {
	/* pop */
	if (depth == 0)
	    return(FALSE);
	depth--;
	if (!keepopen)
	    fclose(YY_CURRENT_BUFFER->yy_input_file);
	yy_delete_buffer(YY_CURRENT_BUFFER);
	yy_switch_to_buffer(state[depth].bs);
	efree(sudoers);
	sudoers = state[depth].path;
	sudolineno = state[depth].lineno;
	keepopen = FALSE;
    }
    return(TRUE);
}

static char *
parse_include(base)
    char *base;
{
    char *cp, *ep, *path;
    int len;

    /* Pull out path from #include line. */
    cp = base + sizeof("#include");
    while (isblank((unsigned char) *cp))
	cp++;
    ep = cp;
    while (*ep != '\0' && !isspace((unsigned char) *ep))
	ep++;

    /* Make a copy of path and return it. */
    len = (int)(ep - cp);
    if ((path = malloc(len + 1)) == NULL)
	yyerror("unable to allocate memory");
    memcpy(path, cp, len);
    path[len] = '\0';

    /* Push any excess characters (e.g. comment, newline) back to the lexer */
    if (*ep != '\0')
	yyless((int)(ep - base));

    return(path);
}

/*
 * Check to make sure an IPv6 address does not contain multiple instances
 * of the string "::".  Assumes strlen(s) >= 1.
 * Returns TRUE if address is valid else FALSE.
 */
static int
ipv6_valid(s)
    const char *s;
{
    int nmatch = 0;

    for (; *s != '\0'; s++) {
	if (s[0] == ':' && s[1] == ':') {
	    if (++nmatch > 1)
		break;
	}
	if (s[0] == '/')
	    nmatch = 0;			/* reset if we hit netmask */
    }

    return (nmatch <= 1);
}
