%{
/*
 * Copyright (c) 1996, 1998-2005, 2007-2009
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
#if defined(YYBISON) && defined(HAVE_ALLOCA_H) && !defined(__GNUC__)
# include <alloca.h>
#endif /* YYBISON && HAVE_ALLOCA_H && !__GNUC__ */
#include <limits.h>

#include "sudo.h"
#include "parse.h"

/*
 * We must define SIZE_MAX for yacc's skeleton.c.
 * If there is no SIZE_MAX or SIZE_T_MAX we have to assume that size_t
 * could be signed (as it is on SunOS 4.x).
 */
#ifndef SIZE_MAX
# ifdef SIZE_T_MAX
#  define SIZE_MAX	SIZE_T_MAX
# else
#  define SIZE_MAX	INT_MAX
# endif /* SIZE_T_MAX */
#endif /* SIZE_MAX */

/*
 * Globals
 */
extern int sudolineno;
extern char *sudoers;
int parse_error;
int pedantic = FALSE;
int verbose = FALSE;
int errorlineno = -1;
char *errorfile = NULL;

struct defaults_list defaults;
struct userspec_list userspecs;

/*
 * Local protoypes
 */
static void  add_defaults	__P((int, struct member *, struct defaults *));
static void  add_userspec	__P((struct member *, struct privilege *));
static struct defaults *new_default __P((char *, char *, int));
static struct member *new_member __P((char *, int));
       void  yyerror		__P((const char *));

void
yyerror(s)
    const char *s;
{
    /* Save the line the first error occurred on. */
    if (errorlineno == -1) {
	errorlineno = sudolineno ? sudolineno - 1 : 0;
	errorfile = estrdup(sudoers);
    }
    if (verbose && s != NULL) {
#ifndef TRACELEXER
	(void) fprintf(stderr, ">>> %s: %s near line %d <<<\n", sudoers, s,
	    sudolineno ? sudolineno - 1 : 0);
#else
	(void) fprintf(stderr, "<*> ");
#endif
    }
    parse_error = TRUE;
}
%}

%union {
    struct cmndspec *cmndspec;
    struct defaults *defaults;
    struct member *member;
    struct runascontainer *runas;
    struct privilege *privilege;
    struct sudo_command command;
    struct cmndtag tag;
    struct selinux_info seinfo;
    char *string;
    int tok;
}

%start file				/* special start symbol */
%token <command> COMMAND		/* absolute pathname w/ optional args */
%token <string>  ALIAS			/* an UPPERCASE alias name */
%token <string>	 DEFVAR			/* a Defaults variable name */
%token <string>  NTWKADDR		/* ipv4 or ipv6 address */
%token <string>  NETGROUP		/* a netgroup (+NAME) */
%token <string>  USERGROUP		/* a usergroup (%NAME) */
%token <string>  WORD			/* a word */
%token <tok>	 DEFAULTS		/* Defaults entry */
%token <tok>	 DEFAULTS_HOST		/* Host-specific defaults entry */
%token <tok>	 DEFAULTS_USER		/* User-specific defaults entry */
%token <tok>	 DEFAULTS_RUNAS		/* Runas-specific defaults entry */
%token <tok>	 DEFAULTS_CMND		/* Command-specific defaults entry */
%token <tok> 	 NOPASSWD		/* no passwd req for command */
%token <tok> 	 PASSWD			/* passwd req for command (default) */
%token <tok> 	 NOEXEC			/* preload dummy execve() for cmnd */
%token <tok> 	 EXEC			/* don't preload dummy execve() */
%token <tok>	 SETENV			/* user may set environment for cmnd */
%token <tok>	 NOSETENV		/* user may not set environment */
%token <tok>	 ALL			/* ALL keyword */
%token <tok>	 COMMENT		/* comment and/or carriage return */
%token <tok>	 HOSTALIAS		/* Host_Alias keyword */
%token <tok>	 CMNDALIAS		/* Cmnd_Alias keyword */
%token <tok>	 USERALIAS		/* User_Alias keyword */
%token <tok>	 RUNASALIAS		/* Runas_Alias keyword */
%token <tok>	 ':' '=' ',' '!' '+' '-' /* union member tokens */
%token <tok>	 '(' ')'		/* runas tokens */
%token <tok>	 ERROR
%token <tok>	 TYPE			/* SELinux type */
%token <tok>	 ROLE			/* SELinux role */

%type <cmndspec>  cmndspec
%type <cmndspec>  cmndspeclist
%type <defaults>  defaults_entry
%type <defaults>  defaults_list
%type <member>	  cmnd
%type <member>	  opcmnd
%type <member>	  cmndlist
%type <member>	  host
%type <member>	  hostlist
%type <member>	  ophost
%type <member>	  opuser
%type <member>	  user
%type <member>	  userlist
%type <member>	  opgroup
%type <member>	  group
%type <member>	  grouplist
%type <runas>	  runasspec
%type <runas>	  runaslist
%type <privilege> privilege
%type <privilege> privileges
%type <tag>	  cmndtag
%type <seinfo>	  selinux
%type <string>	  rolespec
%type <string>	  typespec

%%

file		:	{ ; }
		|	line
		;

line		:	entry
		|	line entry
		;

entry		:	COMMENT {
			    ;
			}
                |       error COMMENT {
			    yyerrok;
			}
		|	userlist privileges {
			    add_userspec($1, $2);
			}
		|	USERALIAS useraliases {
			    ;
			}
		|	HOSTALIAS hostaliases {
			    ;
			}
		|	CMNDALIAS cmndaliases {
			    ;
			}
		|	RUNASALIAS runasaliases {
			    ;
			}
		|	DEFAULTS defaults_list {
			    add_defaults(DEFAULTS, NULL, $2);
			}
		|	DEFAULTS_USER userlist defaults_list {
			    add_defaults(DEFAULTS_USER, $2, $3);
			}
		|	DEFAULTS_RUNAS userlist defaults_list {
			    add_defaults(DEFAULTS_RUNAS, $2, $3);
			}
		|	DEFAULTS_HOST hostlist defaults_list {
			    add_defaults(DEFAULTS_HOST, $2, $3);
			}
		|	DEFAULTS_CMND cmndlist defaults_list {
			    add_defaults(DEFAULTS_CMND, $2, $3);
			}
		;

defaults_list	:	defaults_entry
		|	defaults_list ',' defaults_entry {
			    list_append($1, $3);
			    $$ = $1;
			}
		;

defaults_entry	:	DEFVAR {
			    $$ = new_default($1, NULL, TRUE);
			}
		|	'!' DEFVAR {
			    $$ = new_default($2, NULL, FALSE);
			}
		|	DEFVAR '=' WORD {
			    $$ = new_default($1, $3, TRUE);
			}
		|	DEFVAR '+' WORD {
			    $$ = new_default($1, $3, '+');
			}
		|	DEFVAR '-' WORD {
			    $$ = new_default($1, $3, '-');
			}
		;

privileges	:	privilege
		|	privileges ':' privilege {
			    list_append($1, $3);
			    $$ = $1;
			}
		;

privilege	:	hostlist '=' cmndspeclist {
			    struct privilege *p = emalloc(sizeof(*p));
			    list2tq(&p->hostlist, $1);
			    list2tq(&p->cmndlist, $3);
			    p->prev = p;
			    p->next = NULL;
			    $$ = p;
			}
		;

ophost		:	host {
			    $$ = $1;
			    $$->negated = FALSE;
			}
		|	'!' host {
			    $$ = $2;
			    $$->negated = TRUE;
			}
		;

host		:	ALIAS {
			    $$ = new_member($1, ALIAS);
			}
		|	ALL {
			    $$ = new_member(NULL, ALL);
			}
		|	NETGROUP {
			    $$ = new_member($1, NETGROUP);
			}
		|	NTWKADDR {
			    $$ = new_member($1, NTWKADDR);
			}
		|	WORD {
			    $$ = new_member($1, WORD);
			}
		;

cmndspeclist	:	cmndspec
		|	cmndspeclist ',' cmndspec {
			    list_append($1, $3);
#ifdef HAVE_SELINUX
			    /* propagate role and type */
			    if ($3->role == NULL)
				$3->role = $3->prev->role;
			    if ($3->type == NULL)
				$3->type = $3->prev->type;
#endif /* HAVE_SELINUX */
			    /* propagate tags and runas list */
			    if ($3->tags.nopasswd == UNSPEC)
				$3->tags.nopasswd = $3->prev->tags.nopasswd;
			    if ($3->tags.noexec == UNSPEC)
				$3->tags.noexec = $3->prev->tags.noexec;
			    if ($3->tags.setenv == UNSPEC &&
				$3->prev->tags.setenv != IMPLIED)
				$3->tags.setenv = $3->prev->tags.setenv;
			    if ((tq_empty(&$3->runasuserlist) &&
				 tq_empty(&$3->runasgrouplist)) &&
				(!tq_empty(&$3->prev->runasuserlist) ||
				 !tq_empty(&$3->prev->runasgrouplist))) {
				$3->runasuserlist = $3->prev->runasuserlist;
				$3->runasgrouplist = $3->prev->runasgrouplist;
			    }
			    $$ = $1;
			}
		;

cmndspec	:	runasspec selinux cmndtag opcmnd {
			    struct cmndspec *cs = emalloc(sizeof(*cs));
			    if ($1 != NULL) {
				list2tq(&cs->runasuserlist, $1->runasusers);
				list2tq(&cs->runasgrouplist, $1->runasgroups);
				efree($1);
			    } else {
				tq_init(&cs->runasuserlist);
				tq_init(&cs->runasgrouplist);
			    }
#ifdef HAVE_SELINUX
			    cs->role = $2.role;
			    cs->type = $2.type;
#endif
			    cs->tags = $3;
			    cs->cmnd = $4;
			    cs->prev = cs;
			    cs->next = NULL;
			    /* sudo "ALL" implies the SETENV tag */
			    if (cs->cmnd->type == ALL && !cs->cmnd->negated &&
				cs->tags.setenv == UNSPEC)
				cs->tags.setenv = IMPLIED;
			    $$ = cs;
			}
		;

opcmnd		:	cmnd {
			    $$ = $1;
			    $$->negated = FALSE;
			}
		|	'!' cmnd {
			    $$ = $2;
			    $$->negated = TRUE;
			}
		;

rolespec	:	ROLE '=' WORD {
			    $$ = $3;
			}
		;

typespec	:	TYPE '=' WORD {
			    $$ = $3;
			}
		;

selinux		:	/* empty */ {
			    $$.role = NULL;
			    $$.type = NULL;
			}
		|	rolespec {
			    $$.role = $1;
			    $$.type = NULL;
			}
		|	typespec {
			    $$.type = $1;
			    $$.role = NULL;
			}
		|	rolespec typespec {
			    $$.role = $1;
			    $$.type = $2;
			}
		|	typespec rolespec {
			    $$.type = $1;
			    $$.role = $2;
			}
		;

runasspec	:	/* empty */ {
			    $$ = NULL;
			}
		|	'(' runaslist ')' {
			    $$ = $2;
			}
		;

runaslist	:	userlist {
			    $$ = emalloc(sizeof(struct runascontainer));
			    $$->runasusers = $1;
			    $$->runasgroups = NULL;
			}
		|	userlist ':' grouplist {
			    $$ = emalloc(sizeof(struct runascontainer));
			    $$->runasusers = $1;
			    $$->runasgroups = $3;
			}
		|	':' grouplist {
			    $$ = emalloc(sizeof(struct runascontainer));
			    $$->runasusers = NULL;
			    $$->runasgroups = $2;
			}
		;

cmndtag		:	/* empty */ {
			    $$.nopasswd = $$.noexec = $$.setenv = UNSPEC;
			}
		|	cmndtag NOPASSWD {
			    $$.nopasswd = TRUE;
			}
		|	cmndtag PASSWD {
			    $$.nopasswd = FALSE;
			}
		|	cmndtag NOEXEC {
			    $$.noexec = TRUE;
			}
		|	cmndtag EXEC {
			    $$.noexec = FALSE;
			}
		|	cmndtag SETENV {
			    $$.setenv = TRUE;
			}
		|	cmndtag NOSETENV {
			    $$.setenv = FALSE;
			}
		;

cmnd		:	ALL {
			    $$ = new_member(NULL, ALL);
			}
		|	ALIAS {
			    $$ = new_member($1, ALIAS);
			}
		|	COMMAND {
			    struct sudo_command *c = emalloc(sizeof(*c));
			    c->cmnd = $1.cmnd;
			    c->args = $1.args;
			    $$ = new_member((char *)c, COMMAND);
			}
		;

hostaliases	:	hostalias
		|	hostaliases ':' hostalias
		;

hostalias	:	ALIAS '=' hostlist {
			    char *s;
			    if ((s = alias_add($1, HOSTALIAS, $3)) != NULL) {
				yyerror(s);
				YYERROR;
			    }
			}
		;

hostlist	:	ophost
		|	hostlist ',' ophost {
			    list_append($1, $3);
			    $$ = $1;
			}
		;

cmndaliases	:	cmndalias
		|	cmndaliases ':' cmndalias
		;

cmndalias	:	ALIAS '=' cmndlist {
			    char *s;
			    if ((s = alias_add($1, CMNDALIAS, $3)) != NULL) {
				yyerror(s);
				YYERROR;
			    }
			}
		;

cmndlist	:	opcmnd
		|	cmndlist ',' opcmnd {
			    list_append($1, $3);
			    $$ = $1;
			}
		;

runasaliases	:	runasalias
		|	runasaliases ':' runasalias
		;

runasalias	:	ALIAS '=' userlist {
			    char *s;
			    if ((s = alias_add($1, RUNASALIAS, $3)) != NULL) {
				yyerror(s);
				YYERROR;
			    }
			}
		;

useraliases	:	useralias
		|	useraliases ':' useralias
		;

useralias	:	ALIAS '=' userlist {
			    char *s;
			    if ((s = alias_add($1, USERALIAS, $3)) != NULL) {
				yyerror(s);
				YYERROR;
			    }
			}
		;

userlist	:	opuser
		|	userlist ',' opuser {
			    list_append($1, $3);
			    $$ = $1;
			}
		;

opuser		:	user {
			    $$ = $1;
			    $$->negated = FALSE;
			}
		|	'!' user {
			    $$ = $2;
			    $$->negated = TRUE;
			}
		;

user		:	ALIAS {
			    $$ = new_member($1, ALIAS);
			}
		|	ALL {
			    $$ = new_member(NULL, ALL);
			}
		|	NETGROUP {
			    $$ = new_member($1, NETGROUP);
			}
		|	USERGROUP {
			    $$ = new_member($1, USERGROUP);
			}
		|	WORD {
			    $$ = new_member($1, WORD);
			}
		;

grouplist	:	opgroup
		|	grouplist ',' opgroup {
			    list_append($1, $3);
			    $$ = $1;
			}
		;

opgroup		:	group {
			    $$ = $1;
			    $$->negated = FALSE;
			}
		|	'!' group {
			    $$ = $2;
			    $$->negated = TRUE;
			}
		;

group		:	ALIAS {
			    $$ = new_member($1, ALIAS);
			}
		|	ALL {
			    $$ = new_member(NULL, ALL);
			}
		|	WORD {
			    $$ = new_member($1, WORD);
			}
		;

%%
static struct defaults *
new_default(var, val, op)
    char *var;
    char *val;
    int op;
{
    struct defaults *d;

    d = emalloc(sizeof(struct defaults));
    d->var = var;
    d->val = val;
    tq_init(&d->binding);
    d->type = 0;
    d->op = op;
    d->prev = d;
    d->next = NULL;

    return(d);
}

static struct member *
new_member(name, type)
    char *name;
    int type;
{
    struct member *m;

    m = emalloc(sizeof(struct member));
    m->name = name;
    m->type = type;
    m->prev = m;
    m->next = NULL;

    return(m);
}

/*
 * Add a list of defaults structures to the defaults list.
 * The binding, if non-NULL, specifies a list of hosts, users, or
 * runas users the entries apply to (specified by the type).
 */
static void
add_defaults(type, bmem, defs)
    int type;
    struct member *bmem;
    struct defaults *defs;
{
    struct defaults *d;
    struct member_list binding;

    /*
     * We can only call list2tq once on bmem as it will zero
     * out the prev pointer when it consumes bmem.
     */
    list2tq(&binding, bmem);

    /*
     * Set type and binding (who it applies to) for new entries.
     */
    for (d = defs; d != NULL; d = d->next) {
	d->type = type;
	d->binding = binding;
    }
    tq_append(&defaults, defs);
}

/*
 * Allocate a new struct userspec, populate it, and insert it at the
 * and of the userspecs list.
 */
static void
add_userspec(members, privs)
    struct member *members;
    struct privilege *privs;
{
    struct userspec *u;

    u = emalloc(sizeof(*u));
    list2tq(&u->users, members);
    list2tq(&u->privileges, privs);
    u->prev = u;
    u->next = NULL;
    tq_append(&userspecs, u);
}

/*
 * Free up space used by data structures from a previous parser run and sets
 * the current sudoers file to path.
 */
void
init_parser(path, quiet)
    char *path;
    int quiet;
{
    struct defaults *d;
    struct member *m, *binding;
    struct userspec *us;
    struct privilege *priv;
    struct cmndspec *cs;
    struct sudo_command *c;

    while ((us = tq_pop(&userspecs)) != NULL) {
	while ((m = tq_pop(&us->users)) != NULL) {
	    efree(m->name);
	    efree(m);
	}
	while ((priv = tq_pop(&us->privileges)) != NULL) {
	    struct member *runasuser = NULL, *runasgroup = NULL;
#ifdef HAVE_SELINUX
	    char *role = NULL, *type = NULL;
#endif /* HAVE_SELINUX */

	    while ((m = tq_pop(&priv->hostlist)) != NULL) {
		efree(m->name);
		efree(m);
	    }
	    while ((cs = tq_pop(&priv->cmndlist)) != NULL) {
#ifdef HAVE_SELINUX
		/* Only free the first instance of a role/type. */
		if (cs->role != role) {
		    role = cs->role;
		    efree(cs->role);
		}
		if (cs->type != type) {
		    type = cs->type;
		    efree(cs->type);
		}
#endif /* HAVE_SELINUX */
		if (tq_last(&cs->runasuserlist) != runasuser) {
		    runasuser = tq_last(&cs->runasuserlist);
		    while ((m = tq_pop(&cs->runasuserlist)) != NULL) {
			efree(m->name);
			efree(m);
		    }
		}
		if (tq_last(&cs->runasgrouplist) != runasgroup) {
		    runasgroup = tq_last(&cs->runasgrouplist);
		    while ((m = tq_pop(&cs->runasgrouplist)) != NULL) {
			efree(m->name);
			efree(m);
		    }
		}
		if (cs->cmnd->type == COMMAND) {
			c = (struct sudo_command *) cs->cmnd->name;
			efree(c->cmnd);
			efree(c->args);
		}
		efree(cs->cmnd->name);
		efree(cs->cmnd);
		efree(cs);
	    }
	    efree(priv);
	}
	efree(us);
    }
    tq_init(&userspecs);

    binding = NULL;
    while ((d = tq_pop(&defaults)) != NULL) {
	if (tq_last(&d->binding) != binding) {
	    binding = tq_last(&d->binding);
	    while ((m = tq_pop(&d->binding)) != NULL) {
		if (m->type == COMMAND) {
			c = (struct sudo_command *) m->name;
			efree(c->cmnd);
			efree(c->args);
		}
		efree(m->name);
		efree(m);
	    }
	}
	efree(d->var);
	efree(d->val);
	efree(d);
    }
    tq_init(&defaults);

    init_aliases();

    init_lexer();

    efree(sudoers);
    sudoers = path ? estrdup(path) : NULL;

    parse_error = FALSE;
    errorlineno = -1;
    errorfile = NULL;
    sudolineno = 1;
    verbose = !quiet;
}
