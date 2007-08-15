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
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

/*
 * XXX - the whole opFOO naming thing is somewhat bogus.
 *
 * XXX - the way things are stored for printmatches is stupid,
 *       they should be stored as elements in an array and then
 *       list_matches() can format things the way it wants.
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
#include <pwd.h>
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
# include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#if defined(YYBISON) && defined(HAVE_ALLOCA_H) && !defined(__GNUC__)
# include <alloca.h>
#endif /* YYBISON && HAVE_ALLOCA_H && !__GNUC__ */
#ifdef HAVE_LSEARCH
# include <search.h>
#endif /* HAVE_LSEARCH */

#include "sudo.h"
#include "parse.h"

#ifndef HAVE_LSEARCH
#include "emul/search.h"
#endif /* HAVE_LSEARCH */

#ifndef lint
__unused static const char rcsid[] = "$Sudo: parse.yacc,v 1.204.2.6 2007/08/13 16:30:02 millert Exp $";
#endif /* lint */

/*
 * Globals
 */
extern int sudolineno, parse_error;
int errorlineno = -1;
int clearaliases = TRUE;
int printmatches = FALSE;
int pedantic = FALSE;
int keepall = FALSE;
int quiet = FALSE;
int used_runas = FALSE;

/*
 * Alias types
 */
#define HOST_ALIAS		 1
#define CMND_ALIAS		 2
#define USER_ALIAS		 3
#define RUNAS_ALIAS		 4

#define SETMATCH(_var, _val)	do { \
	if ((_var) == UNSPEC || (_val) != NOMATCH) \
	    (_var) = (_val); \
} while (0)

#define SETNMATCH(_var, _val)	do { \
	if ((_val) != NOMATCH) \
	    (_var) = ! (_val); \
	else if ((_var) == UNSPEC) \
	    (_var) = NOMATCH; \
} while (0)

/*
 * The matching stack, initial space allocated in init_parser().
 */
struct matchstack *match;
int top = 0, stacksize = 0;

#define push \
    do { \
	if (top >= stacksize) { \
	    while ((stacksize += STACKINCREMENT) < top); \
	    match = (struct matchstack *) erealloc3(match, stacksize, sizeof(struct matchstack)); \
	} \
	match[top].user   = UNSPEC; \
	match[top].cmnd   = UNSPEC; \
	match[top].host   = UNSPEC; \
	match[top].runas  = UNSPEC; \
	match[top].nopass = def_authenticate ? UNSPEC : TRUE; \
	match[top].noexec = def_noexec ? TRUE : UNSPEC; \
	match[top].setenv = def_setenv ? TRUE : UNSPEC; \
	top++; \
    } while (0)

#define pushcp \
    do { \
	if (top >= stacksize) { \
	    while ((stacksize += STACKINCREMENT) < top); \
	    match = (struct matchstack *) erealloc3(match, stacksize, sizeof(struct matchstack)); \
	} \
	match[top].user   = match[top-1].user; \
	match[top].cmnd   = match[top-1].cmnd; \
	match[top].host   = match[top-1].host; \
	match[top].runas  = match[top-1].runas; \
	match[top].nopass = match[top-1].nopass; \
	match[top].noexec = match[top-1].noexec; \
	match[top].setenv = match[top-1].setenv; \
	top++; \
    } while (0)

#define pop \
    do { \
	if (top == 0) \
	    yyerror("matching stack underflow"); \
	else \
	    top--; \
    } while (0)


/*
 * For testing if foo_matches variable was set to TRUE or FALSE
 */
#define	MATCHED(_v)	((_v) >= 0)

/*
 * Shortcuts for append()
 */
#define append_cmnd(s, p) append(s, &cm_list[cm_list_len].cmnd, \
	&cm_list[cm_list_len].cmnd_len, &cm_list[cm_list_len].cmnd_size, p)

#define append_runas(s, p) append(s, &cm_list[cm_list_len].runas, \
	&cm_list[cm_list_len].runas_len, &cm_list[cm_list_len].runas_size, p)

#define append_entries(s, p) append(s, &ga_list[ga_list_len-1].entries, \
	&ga_list[ga_list_len-1].entries_len, \
	&ga_list[ga_list_len-1].entries_size, p)

/*
 * The stack for printmatches.  A list of allowed commands for the user.
 */
static struct command_match *cm_list = NULL;
static size_t cm_list_len = 0, cm_list_size = 0;

/*
 * List of Cmnd_Aliases and expansions for `sudo -l'
 */
static int in_alias = FALSE;
static size_t ga_list_len = 0, ga_list_size = 0;
static struct generic_alias *ga_list = NULL;

/*
 * Does this Defaults list pertain to this user?
 */
static int defaults_matches = FALSE;

/*
 * Local protoypes
 */
static int  add_alias		__P((char *, int, int));
static void append		__P((char *, char **, size_t *, size_t *, char *));
static void expand_ga_list	__P((void));
static void expand_match_list	__P((void));
static aliasinfo *find_alias	__P((char *, int));
static int  more_aliases	__P((void));
       void init_parser		__P((void));
       void yyerror		__P((char *));

void
yyerror(s)
    char *s;
{
    /* Save the line the first error occurred on. */
    if (errorlineno == -1)
	errorlineno = sudolineno ? sudolineno - 1 : 0;
    if (s && !quiet) {
#ifndef TRACELEXER
	(void) fprintf(stderr, ">>> sudoers file: %s, line %d <<<\n", s,
	    sudolineno ? sudolineno - 1 : 0);
#else
	(void) fprintf(stderr, "<*> ");
#endif
    }
    parse_error = TRUE;
}
%}

%union {
    char *string;
    int BOOLEAN;
    struct sudo_command command;
    int tok;
}

%start file				/* special start symbol */
%token <command> COMMAND		/* absolute pathname w/ optional args */
%token <string>  ALIAS			/* an UPPERCASE alias name */
%token <string>	 DEFVAR			/* a Defaults variable name */
%token <string>  NTWKADDR		/* w.x.y.z or ipv6 address */
%token <string>  NETGROUP		/* a netgroup (+NAME) */
%token <string>  USERGROUP		/* a usergroup (%NAME) */
%token <string>  WORD			/* a word */
%token <tok>	 DEFAULTS		/* Defaults entry */
%token <tok>	 DEFAULTS_HOST		/* Host-specific defaults entry */
%token <tok>	 DEFAULTS_USER		/* User-specific defaults entry */
%token <tok>	 DEFAULTS_RUNAS		/* Runas-specific defaults entry */
%token <tok> 	 RUNAS			/* ( runas_list ) */
%token <tok> 	 NOPASSWD		/* no passwd req for command */
%token <tok> 	 PASSWD			/* passwd req for command (default) */
%token <tok> 	 NOEXEC			/* preload dummy execve() for cmnd */
%token <tok> 	 EXEC			/* don't preload dummy execve() */
%token <tok> 	 SETENV			/* user may set environment for cmnd */
%token <tok> 	 NOSETENV		/* user may not set environment */
%token <tok>	 ALL			/* ALL keyword */
%token <tok>	 COMMENT		/* comment and/or carriage return */
%token <tok>	 HOSTALIAS		/* Host_Alias keyword */
%token <tok>	 CMNDALIAS		/* Cmnd_Alias keyword */
%token <tok>	 USERALIAS		/* User_Alias keyword */
%token <tok>	 RUNASALIAS		/* Runas_Alias keyword */
%token <tok>	 ':' '=' ',' '!' '+' '-' /* union member tokens */
%token <tok>	 ERROR

/*
 * NOTE: these are not true booleans as there are actually 4 possible values:
 *        1) TRUE (positive match)
 *        0) FALSE (negative match due to a '!' somewhere)
 *       -1) NOMATCH (don't change the value of *_matches)
 *       -2) UNSPEC (uninitialized value)
 */
%type <BOOLEAN>	 cmnd
%type <BOOLEAN>	 host
%type <BOOLEAN>	 runasuser
%type <BOOLEAN>	 oprunasuser
%type <BOOLEAN>	 runaslist
%type <BOOLEAN>	 user

%%

file		:	entry
		|	file entry
		;

entry		:	COMMENT
			    { ; }
                |       error COMMENT
			    { yyerrok; }
		|	{ push; } userlist privileges {
			    while (top && user_matches != TRUE)
				pop;
			}
		|	USERALIAS useraliases
			    { ; }
		|	HOSTALIAS hostaliases
			    { ; }
		|	CMNDALIAS cmndaliases
			    { ; }
		|	RUNASALIAS runasaliases
			    { ; }
		|	defaults_line
			    { ; }
		;

defaults_line	:	defaults_type defaults_list
		;

defaults_type	:	DEFAULTS {
			    defaults_matches = TRUE;
			}
		|	DEFAULTS_USER { push; } userlist {
			    defaults_matches = user_matches;
			    pop;
			}
		|	DEFAULTS_RUNAS { push; } runaslist {
			    defaults_matches = $3 == TRUE;
			    pop;
			}
		|	DEFAULTS_HOST { push; } hostlist {
			    defaults_matches = host_matches;
			    pop;
			}
		;

defaults_list	:	defaults_entry
		|	defaults_entry ',' defaults_list
		;

defaults_entry	:	DEFVAR {
			    if (defaults_matches == TRUE &&
				!set_default($1, NULL, TRUE)) {
				yyerror(NULL);
				YYERROR;
			    }
			    efree($1);
			}
		|	'!' DEFVAR {
			    if (defaults_matches == TRUE &&
				!set_default($2, NULL, FALSE)) {
				yyerror(NULL);
				YYERROR;
			    }
			    efree($2);
			}
		|	DEFVAR '=' WORD {
			    if (defaults_matches == TRUE &&
				!set_default($1, $3, TRUE)) {
				yyerror(NULL);
				YYERROR;
			    }
			    efree($1);
			    efree($3);
			}
		|	DEFVAR '+' WORD {
			    if (defaults_matches == TRUE &&
				!set_default($1, $3, '+')) {
				yyerror(NULL);
				YYERROR;
			    }
			    efree($1);
			    efree($3);
			}
		|	DEFVAR '-' WORD {
			    if (defaults_matches == TRUE &&
				!set_default($1, $3, '-')) {
				yyerror(NULL);
				YYERROR;
			    }
			    efree($1);
			    efree($3);
			}
		;

privileges	:	privilege
		|	privileges ':' privilege
		;

privilege	:	hostlist '=' cmndspeclist {
			    /*
			     * We already did a push if necessary in
			     * cmndspec so just reset some values so
			     * the next 'privilege' gets a clean slate.
			     */
			    host_matches = UNSPEC;
			    runas_matches = UNSPEC;
			    no_passwd = def_authenticate ? UNSPEC : TRUE;
			    no_execve = def_noexec ? TRUE : UNSPEC;
			    setenv_ok = def_setenv ? TRUE : UNSPEC;
			}
		;

ophost		:	host {
			    SETMATCH(host_matches, $1);
			}
		|	'!' host {
			    SETNMATCH(host_matches, $2);
			}
		;

host		:	ALL {
			    $$ = TRUE;
			}
		|	NTWKADDR {
			    if (addr_matches($1))
				$$ = TRUE;
			    else
				$$ = NOMATCH;
			    efree($1);
			}
		|	NETGROUP {
			    if (netgr_matches($1, user_host, user_shost, NULL))
				$$ = TRUE;
			    else
				$$ = NOMATCH;
			    efree($1);
			}
		|	WORD {
			    if (hostname_matches(user_shost, user_host, $1) == 0)
				$$ = TRUE;
			    else
				$$ = NOMATCH;
			    efree($1);
			}
		|	ALIAS {
			    aliasinfo *aip = find_alias($1, HOST_ALIAS);

			    /* could be an all-caps hostname */
			    if (aip)
				$$ = aip->val;
			    else if (strcasecmp(user_shost, $1) == 0)
				$$ = TRUE;
			    else {
				if (pedantic) {
				    (void) fprintf(stderr,
					"%s: undeclared Host_Alias `%s' referenced near line %d\n",
					(pedantic == 1) ? "Warning" : "Error", $1, sudolineno);
				    if (pedantic > 1) {
					yyerror(NULL);
					YYERROR;
				    }
				}
				$$ = NOMATCH;
			    }
			    efree($1);
			}
		;

cmndspeclist	:	cmndspec
		|	cmndspeclist ',' cmndspec
		;

cmndspec	:	runasspec cmndtag opcmnd {
			    /*
			     * Push the entry onto the stack if it is worth
			     * saving and reset cmnd_matches for next cmnd.
			     *
			     * We need to save at least one entry on
			     * the stack so sudoers_lookup() can tell that
			     * the user was listed in sudoers.  Also, we
			     * need to be able to tell whether or not a
			     * user was listed for this specific host.
			     *
			     * If keepall is set and the user matches then
			     * we need to keep entries around too...
			     */
			    if (MATCHED(user_matches) &&
				MATCHED(host_matches) &&
				MATCHED(cmnd_matches) &&
				MATCHED(runas_matches))
				pushcp;
			    else if (MATCHED(user_matches) && (top == 1 ||
				(top == 2 && MATCHED(host_matches) &&
				!MATCHED(match[0].host))))
				pushcp;
			    else if (user_matches == TRUE && keepall)
				pushcp;
			    cmnd_matches = UNSPEC;
			}
		;

opcmnd		:	cmnd {
			    SETMATCH(cmnd_matches, $1);
			}
		|	'!' {
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries("!", ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_cmnd("!", NULL);
			    }
			} cmnd {
			    SETNMATCH(cmnd_matches, $3);
			}
		;

runasspec	:	/* empty */ {
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE) {
				if (runas_matches == UNSPEC) {
				    cm_list[cm_list_len].runas_len = 0;
				} else {
				    /* Inherit runas data. */
				    cm_list[cm_list_len].runas =
					estrdup(cm_list[cm_list_len-1].runas);
				    cm_list[cm_list_len].runas_len =
					cm_list[cm_list_len-1].runas_len;
				    cm_list[cm_list_len].runas_size =
					cm_list[cm_list_len-1].runas_size;
				}
			    }
			    /*
			     * If this is the first entry in a command list
			     * then check against default runas user.
			     */
			    if (runas_matches == UNSPEC) {
				runas_matches =
				    userpw_matches(def_runas_default,
					*user_runas, runas_pw);
			    }
			}
		|	RUNAS runaslist {
			    runas_matches = $2;
			}
		;

runaslist	:	oprunasuser { ; }
		|	runaslist ',' oprunasuser {
			    /* Later entries override earlier ones. */
			    if ($3 != NOMATCH)
				$$ = $3;
			    else
				$$ = $1;
			}
		;

oprunasuser	:	runasuser { ; }
		|	'!' {
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries("!", ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas("!", ", ");
			    }
			} runasuser {
			    /* Set $$ to the negation of runasuser */
			    $$ = ($3 == NOMATCH ? NOMATCH : ! $3);
			}
		;

runasuser	:	WORD {
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries($1, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas($1, ", ");
			    }
			    if (userpw_matches($1, *user_runas, runas_pw))
				$$ = TRUE;
			    else
				$$ = NOMATCH;
			    efree($1);
			    used_runas = TRUE;
			}
		|	USERGROUP {
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries($1, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas($1, ", ");
			    }
			    if (usergr_matches($1, *user_runas, runas_pw))
				$$ = TRUE;
			    else
				$$ = NOMATCH;
			    efree($1);
			    used_runas = TRUE;
			}
		|	NETGROUP {
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries($1, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas($1, ", ");
			    }
			    if (netgr_matches($1, NULL, NULL, *user_runas))
				$$ = TRUE;
			    else
				$$ = NOMATCH;
			    efree($1);
			    used_runas = TRUE;
			}
		|	ALIAS {
			    aliasinfo *aip = find_alias($1, RUNAS_ALIAS);

			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries($1, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas($1, ", ");
			    }
			    /* could be an all-caps username */
			    if (aip)
				$$ = aip->val;
			    else if (strcmp($1, *user_runas) == 0)
				$$ = TRUE;
			    else {
				if (pedantic) {
				    (void) fprintf(stderr,
					"%s: undeclared Runas_Alias `%s' referenced near line %d\n",
					(pedantic == 1) ? "Warning" : "Error", $1, sudolineno);
				    if (pedantic > 1) {
					yyerror(NULL);
					YYERROR;
				    }
				}
				$$ = NOMATCH;
			    }
			    efree($1);
			    used_runas = TRUE;
			}
		|	ALL {
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries("ALL", ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE)
				    append_runas("ALL", ", ");
			    }
			    $$ = TRUE;
			}
		;

cmndtag		:	/* empty */ {
			    /* Inherit {NO,}{PASSWD,EXEC,SETENV} status. */
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE) {
				if (no_passwd == TRUE)
				    cm_list[cm_list_len].nopasswd = TRUE;
				else
				    cm_list[cm_list_len].nopasswd = FALSE;
				if (no_execve == TRUE)
				    cm_list[cm_list_len].noexecve = TRUE;
				else
				    cm_list[cm_list_len].noexecve = FALSE;
				if (setenv_ok == TRUE)
				    cm_list[cm_list_len].setenv = TRUE;
				else
				    cm_list[cm_list_len].setenv = FALSE;
			    }
			}
		|	cmndtag NOPASSWD {
			    no_passwd = TRUE;
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE)
				cm_list[cm_list_len].nopasswd = TRUE;
			}
		|	cmndtag PASSWD {
			    no_passwd = FALSE;
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE)
				cm_list[cm_list_len].nopasswd = FALSE;
			}
		|	cmndtag NOEXEC {
			    no_execve = TRUE;
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE)
				cm_list[cm_list_len].noexecve = TRUE;
			}
		|	cmndtag EXEC {
			    no_execve = FALSE;
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE)
				cm_list[cm_list_len].noexecve = FALSE;
			}
		|	cmndtag SETENV {
			    setenv_ok = TRUE;
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE)
				cm_list[cm_list_len].setenv = TRUE;
			}
		|	cmndtag NOSETENV {
			    setenv_ok = FALSE;
			    if (printmatches == TRUE && host_matches == TRUE &&
				user_matches == TRUE)
				cm_list[cm_list_len].setenv = FALSE;
			}
		;

cmnd		:	ALL {
			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries("ALL", ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE) {
				    append_cmnd("ALL", NULL);
				    expand_match_list();
				}
			    }

			    efree(safe_cmnd);
			    safe_cmnd = NULL;
			    $$ = TRUE;
			}
		|	ALIAS {
			    aliasinfo *aip;

			    if (printmatches == TRUE) {
				if (in_alias == TRUE)
				    append_entries($1, ", ");
				else if (host_matches == TRUE &&
				    user_matches == TRUE) {
				    append_cmnd($1, NULL);
				    expand_match_list();
				}
			    }

			    if ((aip = find_alias($1, CMND_ALIAS)))
				$$ = aip->val;
			    else {
				if (pedantic) {
				    (void) fprintf(stderr,
					"%s: undeclared Cmnd_Alias `%s' referenced near line %d\n",
					(pedantic == 1) ? "Warning" : "Error", $1, sudolineno);
				    if (pedantic > 1) {
					yyerror(NULL);
					YYERROR;
				    }
				}
				$$ = NOMATCH;
			    }
			    efree($1);
			}
		|	 COMMAND {
			    if (printmatches == TRUE) {
				if (in_alias == TRUE) {
				    append_entries($1.cmnd, ", ");
				    if ($1.args)
					append_entries($1.args, " ");
				}
				if (host_matches == TRUE &&
				    user_matches == TRUE)  {
				    append_cmnd($1.cmnd, NULL);
				    if ($1.args)
					append_cmnd($1.args, " ");
				    expand_match_list();
				}
			    }

			    if (command_matches($1.cmnd, $1.args))
				$$ = TRUE;
			    else
				$$ = NOMATCH;

			    efree($1.cmnd);
			    efree($1.args);
			}
		;

hostaliases	:	hostalias
		|	hostaliases ':' hostalias
		;

hostalias	:	ALIAS { push; } '=' hostlist {
			    if ((MATCHED(host_matches) || pedantic) &&
				!add_alias($1, HOST_ALIAS, host_matches)) {
				yyerror(NULL);
				YYERROR;
			    }
			    pop;
			}
		;

hostlist	:	ophost
		|	hostlist ',' ophost
		;

cmndaliases	:	cmndalias
		|	cmndaliases ':' cmndalias
		;

cmndalias	:	ALIAS {
			    push;
			    if (printmatches == TRUE) {
				in_alias = TRUE;
				/* Allocate space for ga_list if necessary. */
				expand_ga_list();
				ga_list[ga_list_len-1].type = CMND_ALIAS;
				ga_list[ga_list_len-1].alias = estrdup($1);
			     }
			} '=' cmndlist {
			    if ((MATCHED(cmnd_matches) || pedantic) &&
				!add_alias($1, CMND_ALIAS, cmnd_matches)) {
				yyerror(NULL);
				YYERROR;
			    }
			    pop;
			    efree($1);

			    if (printmatches == TRUE)
				in_alias = FALSE;
			}
		;

cmndlist	:	opcmnd { ; }
		|	cmndlist ',' opcmnd
		;

runasaliases	:	runasalias
		|	runasaliases ':' runasalias
		;

runasalias	:	ALIAS {
			    if (printmatches == TRUE) {
				in_alias = TRUE;
				/* Allocate space for ga_list if necessary. */
				expand_ga_list();
				ga_list[ga_list_len-1].type = RUNAS_ALIAS;
				ga_list[ga_list_len-1].alias = estrdup($1);
			    }
			} '=' runaslist {
			    if (($4 != NOMATCH || pedantic) &&
				!add_alias($1, RUNAS_ALIAS, $4)) {
				yyerror(NULL);
				YYERROR;
			    }
			    efree($1);

			    if (printmatches == TRUE)
				in_alias = FALSE;
			}
		;

useraliases	:	useralias
		|	useraliases ':' useralias
		;

useralias	:	ALIAS { push; }	'=' userlist {
			    if ((MATCHED(user_matches) || pedantic) &&
				!add_alias($1, USER_ALIAS, user_matches)) {
				yyerror(NULL);
				YYERROR;
			    }
			    pop;
			    efree($1);
			}
		;

userlist	:	opuser
		|	userlist ',' opuser
		;

opuser		:	user {
			    SETMATCH(user_matches, $1);
			}
		|	'!' user {
			    SETNMATCH(user_matches, $2);
			}
		;

user		:	WORD {
			    if (userpw_matches($1, user_name, sudo_user.pw))
				$$ = TRUE;
			    else
				$$ = NOMATCH;
			    efree($1);
			}
		|	USERGROUP {
			    if (usergr_matches($1, user_name, sudo_user.pw))
				$$ = TRUE;
			    else
				$$ = NOMATCH;
			    efree($1);
			}
		|	NETGROUP {
			    if (netgr_matches($1, NULL, NULL, user_name))
				$$ = TRUE;
			    else
				$$ = NOMATCH;
			    efree($1);
			}
		|	ALIAS {
			    aliasinfo *aip = find_alias($1, USER_ALIAS);

			    /* could be an all-caps username */
			    if (aip)
				$$ = aip->val;
			    else if (strcmp($1, user_name) == 0)
				$$ = TRUE;
			    else {
				if (pedantic) {
				    (void) fprintf(stderr,
					"%s: undeclared User_Alias `%s' referenced near line %d\n",
					(pedantic == 1) ? "Warning" : "Error", $1, sudolineno);
				    if (pedantic > 1) {
					yyerror(NULL);
					YYERROR;
				    }
				}
				$$ = NOMATCH;
			    }
			    efree($1);
			}
		|	ALL {
			    $$ = TRUE;
			}
		;

%%

#define MOREALIASES (32)
aliasinfo *aliases = NULL;
size_t naliases = 0;
size_t nslots = 0;


/*
 * Compare two aliasinfo structures, strcmp() style.
 * Note that we do *not* compare their values.
 */
static int
aliascmp(a1, a2)
    const VOID *a1, *a2;
{
    int r;
    aliasinfo *ai1, *ai2;

    ai1 = (aliasinfo *) a1;
    ai2 = (aliasinfo *) a2;
    if ((r = strcmp(ai1->name, ai2->name)) == 0)
	r = ai1->type - ai2->type;

    return(r);
}

/*
 * Compare two generic_alias structures, strcmp() style.
 */
static int
genaliascmp(entry, key)
    const VOID *entry, *key;
{
    int r;
    struct generic_alias *ga1, *ga2;

    ga1 = (struct generic_alias *) key;
    ga2 = (struct generic_alias *) entry;
    if ((r = strcmp(ga1->alias, ga2->alias)) == 0)
	r = ga1->type - ga2->type;

    return(r);
}


/*
 * Adds the named alias of the specified type to the aliases list.
 */
static int
add_alias(alias, type, val)
    char *alias;
    int type;
    int val;
{
    aliasinfo ai, *aip;
    size_t onaliases;
    char s[512];

    if (naliases >= nslots && !more_aliases()) {
	(void) snprintf(s, sizeof(s), "Out of memory defining alias `%s'",
			alias);
	yyerror(s);
	return(FALSE);
    }

    ai.type = type;
    ai.val = val;
    ai.name = estrdup(alias);
    onaliases = naliases;

    aip = (aliasinfo *) lsearch((VOID *)&ai, (VOID *)aliases, &naliases,
				sizeof(ai), aliascmp);
    if (aip == NULL) {
	(void) snprintf(s, sizeof(s), "Aliases corrupted defining alias `%s'",
			alias);
	yyerror(s);
	return(FALSE);
    }
    if (onaliases == naliases) {
	(void) snprintf(s, sizeof(s), "Alias `%s' already defined", alias);
	yyerror(s);
	return(FALSE);
    }

    return(TRUE);
}

/*
 * Searches for the named alias of the specified type.
 */
static aliasinfo *
find_alias(alias, type)
    char *alias;
    int type;
{
    aliasinfo ai;

    ai.name = alias;
    ai.type = type;

    return((aliasinfo *) lfind((VOID *)&ai, (VOID *)aliases, &naliases,
		 sizeof(ai), aliascmp));
}

/*
 * Allocates more space for the aliases list.
 */
static int
more_aliases()
{

    nslots += MOREALIASES;
    if (nslots == MOREALIASES)
	aliases = (aliasinfo *) malloc(nslots * sizeof(aliasinfo));
    else
	aliases = (aliasinfo *) realloc(aliases, nslots * sizeof(aliasinfo));

    return(aliases != NULL);
}

/*
 * Lists the contents of the aliases list.
 */
void
dumpaliases()
{
    size_t n;

    for (n = 0; n < naliases; n++) {
	if (aliases[n].val == -1)
	    continue;

	switch (aliases[n].type) {
	case HOST_ALIAS:
	    (void) puts("HOST_ALIAS");
	    break;

	case CMND_ALIAS:
	    (void) puts("CMND_ALIAS");
	    break;

	case USER_ALIAS:
	    (void) puts("USER_ALIAS");
	    break;

	case RUNAS_ALIAS:
	    (void) puts("RUNAS_ALIAS");
	    break;
	}
	(void) printf("\t%s: %d\n", aliases[n].name, aliases[n].val);
    }
}

/*
 * Lists the contents of cm_list and ga_list for `sudo -l'.
 */
void
list_matches()
{
    size_t count;
    char *p;
    struct generic_alias *ga, key;

    (void) printf("User %s may run the following commands on this host:\n",
	user_name);
    for (count = 0; count < cm_list_len; count++) {

	/* Print the runas list. */
	(void) fputs("    ", stdout);
	if (cm_list[count].runas) {
	    (void) putchar('(');
	    p = strtok(cm_list[count].runas, ", ");
	    do {
		if (p != cm_list[count].runas)
		    (void) fputs(", ", stdout);

		key.alias = p;
		key.type = RUNAS_ALIAS;
		if ((ga = (struct generic_alias *) lfind((VOID *) &key,
		    (VOID *) &ga_list[0], &ga_list_len, sizeof(key), genaliascmp)))
		    (void) fputs(ga->entries, stdout);
		else
		    (void) fputs(p, stdout);
	    } while ((p = strtok(NULL, ", ")));
	    (void) fputs(") ", stdout);
	} else {
	    (void) printf("(%s) ", def_runas_default);
	}

	/* Is execve(2) disabled? */
	if (cm_list[count].noexecve == TRUE && !def_noexec)
	    (void) fputs("NOEXEC: ", stdout);
	else if (cm_list[count].noexecve == FALSE && def_noexec)
	    (void) fputs("EXEC: ", stdout);

	/* Is a password required? */
	if (cm_list[count].nopasswd == TRUE && def_authenticate)
	    (void) fputs("NOPASSWD: ", stdout);
	else if (cm_list[count].nopasswd == FALSE && !def_authenticate)
	    (void) fputs("PASSWD: ", stdout);

	/* Is setenv enabled? */
	if (cm_list[count].setenv == TRUE && !def_setenv)
	    (void) fputs("SETENV: ", stdout);
	else if (cm_list[count].setenv == FALSE && def_setenv)
	    (void) fputs("NOSETENV: ", stdout);

	/* Print the actual command or expanded Cmnd_Alias. */
	key.alias = cm_list[count].cmnd;
	key.type = CMND_ALIAS;
	if ((ga = (struct generic_alias *) lfind((VOID *) &key,
	    (VOID *) &ga_list[0], &ga_list_len, sizeof(key), genaliascmp)))
	    (void) puts(ga->entries);
	else
	    (void) puts(cm_list[count].cmnd);
    }

    /* Be nice and free up space now that we are done. */
    for (count = 0; count < ga_list_len; count++) {
	efree(ga_list[count].alias);
	efree(ga_list[count].entries);
    }
    efree(ga_list);
    ga_list = NULL;

    for (count = 0; count < cm_list_len; count++) {
	efree(cm_list[count].runas);
	efree(cm_list[count].cmnd);
    }
    efree(cm_list);
    cm_list = NULL;
    cm_list_len = 0;
    cm_list_size = 0;
}

/*
 * Appends a source string to the destination, optionally prefixing a separator.
 */
static void
append(src, dstp, dst_len, dst_size, separator)
    char *src, **dstp;
    size_t *dst_len, *dst_size;
    char *separator;
{
    size_t src_len = strlen(src);
    char *dst = *dstp;

    /*
     * Only add the separator if there is something to separate from.
     * If the last char is a '!', don't apply the separator (XXX).
     */
    if (separator && dst && dst[*dst_len - 1] != '!')
	src_len += strlen(separator);
    else
	separator = NULL;

    /* Assumes dst will be NULL if not set. */
    if (dst == NULL) {
	dst = (char *) emalloc(BUFSIZ);
	*dst = '\0';
	*dst_size = BUFSIZ;
	*dst_len = 0;
	*dstp = dst;
    }

    /* Allocate more space if necessary. */
    if (*dst_size <= *dst_len + src_len) {
	while (*dst_size <= *dst_len + src_len)
	    *dst_size += BUFSIZ;

	dst = (char *) erealloc(dst, *dst_size);
	*dstp = dst;
    }

    /* Copy src -> dst adding a separator if appropriate and adjust len. */
    if (separator)
	(void) strlcat(dst, separator, *dst_size);
    (void) strlcat(dst, src, *dst_size);
    *dst_len += src_len;
}

/*
 * Frees up space used by the aliases list and resets the associated counters.
 */
void
reset_aliases()
{
    size_t n;

    if (aliases) {
	for (n = 0; n < naliases; n++)
	    efree(aliases[n].name);
	efree(aliases);
	aliases = NULL;
    }
    naliases = nslots = 0;
}

/*
 * Increments ga_list_len, allocating more space as necessary.
 */
static void
expand_ga_list()
{

    if (++ga_list_len >= ga_list_size) {
	while ((ga_list_size += STACKINCREMENT) < ga_list_len)
	    ;
	ga_list = (struct generic_alias *)
	    erealloc3(ga_list, ga_list_size, sizeof(struct generic_alias));
    }

    ga_list[ga_list_len - 1].entries = NULL;
}

/*
 * Increments cm_list_len, allocating more space as necessary.
 */
static void
expand_match_list()
{

    if (++cm_list_len >= cm_list_size) {
	while ((cm_list_size += STACKINCREMENT) < cm_list_len)
	    ;
	if (cm_list == NULL)
	    cm_list_len = 0;		/* start at 0 since it is a subscript */
	cm_list = (struct command_match *)
	    erealloc3(cm_list, cm_list_size, sizeof(struct command_match));
    }

    cm_list[cm_list_len].runas = cm_list[cm_list_len].cmnd = NULL;
    cm_list[cm_list_len].nopasswd = FALSE;
    cm_list[cm_list_len].noexecve = FALSE;
    cm_list[cm_list_len].setenv = FALSE;
}

/*
 * Frees up spaced used by a previous parser run and allocates new space
 * for various data structures.
 */
void
init_parser()
{

    /* Free up old data structures if we run the parser more than once. */
    if (match) {
	efree(match);
	match = NULL;
	top = 0;
	parse_error = FALSE;
	used_runas = FALSE;
	errorlineno = -1;
	sudolineno = 1;
    }

    /* Allocate space for the matching stack. */
    stacksize = STACKINCREMENT;
    match = (struct matchstack *) emalloc2(stacksize, sizeof(struct matchstack));

    /* Allocate space for the match list (for `sudo -l'). */
    if (printmatches == TRUE)
	expand_match_list();
}
