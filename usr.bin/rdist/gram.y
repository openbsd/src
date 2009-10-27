%{
/*	$OpenBSD: gram.y,v 1.9 2009/10/27 23:59:42 deraadt Exp $	*/

/*
 * Copyright (c) 1993 Michael A. Cooper
 * Copyright (c) 1993 Regents of the University of California.
 * All rights reserved.
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

#include "defs.h"

static struct namelist *addnl __P((struct namelist *, struct namelist *));
static struct namelist *subnl __P((struct namelist *, struct namelist *));
static struct namelist *andnl __P((struct namelist *, struct namelist *));
static int innl __P((struct namelist *nl, char *p));

struct	cmd *cmds = NULL;
struct	cmd *last_cmd;
struct	namelist *last_n;
struct	subcmd *last_sc;
int	parendepth = 0;

%}

%term ARROW		1
%term COLON		2
%term DCOLON		3
%term NAME		4
%term STRING		5
%term INSTALL		6
%term NOTIFY		7
%term EXCEPT		8
%term PATTERN		9
%term SPECIAL		10
%term CMDSPECIAL	11
%term OPTION		12

%union {
	opt_t 			optval;
	char 		       *string;
	struct subcmd 	       *subcmd;
	struct namelist        *namel;
}

%type <optval> OPTION, options
%type <string> NAME, STRING
%type <subcmd> INSTALL, NOTIFY, EXCEPT, PATTERN, SPECIAL, CMDSPECIAL, cmdlist, cmd
%type <namel> namelist, names, opt_namelist nlist

%%

file:		  /* VOID */
		| file command
		;

command:	  NAME '=' namelist = {
			(void) lookup($1, INSERT, $3);
		}
		| namelist ARROW namelist cmdlist = {
			insert((char *)NULL, $1, $3, $4);
		}
		| NAME COLON namelist ARROW namelist cmdlist = {
			insert($1, $3, $5, $6);
		}
		| namelist DCOLON NAME cmdlist = {
			append((char *)NULL, $1, $3, $4);
		}
		| NAME COLON namelist DCOLON NAME cmdlist = {
			append($1, $3, $5, $6);
		}
		| error
		;

namelist: 	nlist { 
			$$ = $1; 
		}
		| nlist '-' nlist { 
			$$ = subnl($1, $3); 
		}
		| nlist '+' nlist { 
			$$ = addnl($1, $3); 
		}
		| nlist '&' nlist { 
			$$ = andnl($1, $3); 
		}
		;

nlist:	  NAME = {
			$$ = makenl($1);
		}
		| '(' names ')' = {
			$$ = $2;
		}
		;

names:		  /* VOID */ {
			$$ = last_n = NULL;
		}
		| names NAME = {
			if (last_n == NULL)
				$$ = last_n = makenl($2);
			else {
				last_n->n_next = makenl($2);
				last_n = last_n->n_next;
				$$ = $1;
			}
		}
		;

cmdlist:	  /* VOID */ {
			$$ = last_sc = NULL;
		}
		| cmdlist cmd = {
			if (last_sc == NULL)
				$$ = last_sc = $2;
			else {
				last_sc->sc_next = $2;
				last_sc = $2;
				$$ = $1;
			}
		}
		;

cmd:		  INSTALL options opt_namelist ';' = {
			struct namelist *nl;

			$1->sc_options = $2 | options;
			if ($3 != NULL) {
				nl = expand($3, E_VARS);
				if (nl) {
					if (nl->n_next != NULL)
					    yyerror("only one name allowed\n");
					$1->sc_name = nl->n_name;
					free(nl);
				} else
					$1->sc_name = NULL;
			}
			$$ = $1;
		}
		| NOTIFY namelist ';' = {
			if ($2 != NULL)
				$1->sc_args = expand($2, E_VARS);
			$$ = $1;
		}
		| EXCEPT namelist ';' = {
			if ($2 != NULL)
				$1->sc_args = expand($2, E_ALL);
			$$ = $1;
		}
		| PATTERN namelist ';' = {
			struct namelist *nl;
			char ebuf[BUFSIZ];
			regex_t reg;
			int ecode;

			for (nl = $2; nl != NULL; nl = nl->n_next) {
				/* check for a valid regex */
				ecode = regcomp(&reg, nl->n_name, REG_NOSUB);
				if (ecode) {
					regerror(ecode, &reg, ebuf,
					    sizeof(ebuf));
					yyerror(ebuf);
				}
				regfree(&reg);
			}
			$1->sc_args = expand($2, E_VARS);
			$$ = $1;
		}
		| SPECIAL opt_namelist STRING ';' = {
			if ($2 != NULL)
				$1->sc_args = expand($2, E_ALL);
			$1->sc_name = $3;
			$$ = $1;
		}
		| CMDSPECIAL opt_namelist STRING ';' = {
			if ($2 != NULL)
				$1->sc_args = expand($2, E_ALL);
			$1->sc_name = $3;
			$$ = $1;
		}
		;

options:	  /* VOID */ = {
			$$ = 0;
		}
		| options OPTION = {
			$$ |= $2;
		}
		;

opt_namelist:	  /* VOID */ = {
			$$ = NULL;
		}
		| namelist = {
			$$ = $1;
		}
		;

%%

int	yylineno = 1;
extern	FILE *fin;

int
yylex(void)
{
	static char yytext[INMAX];
	int c;
	char *cp1, *cp2;
	static char quotechars[] = "[]{}*?$";
	
again:
	switch (c = getc(fin)) {
	case EOF:  /* end of file */
		return(0);

	case '#':  /* start of comment */
		while ((c = getc(fin)) != EOF && c != '\n')
			;
		if (c == EOF)
			return(0);
	case '\n':
		yylineno++;
	case ' ':
	case '\t':  /* skip blanks */
		goto again;

	case '=':  /* EQUAL */
	case ';':  /* SM */
	case '+': 
	case '&': 
		return(c);

	case '(':  /* LP */
		++parendepth;
		return(c);

	case ')':  /* RP */
		--parendepth;
		return(c);

	case '-':  /* -> */
		if ((c = getc(fin)) == '>')
			return(ARROW);
		(void) ungetc(c, fin);
		c = '-';
		break;

	case '"':  /* STRING */
		cp1 = yytext;
		cp2 = &yytext[INMAX - 1];
		for (;;) {
			if (cp1 >= cp2) {
				yyerror("command string too long\n");
				break;
			}
			c = getc(fin);
			if (c == EOF || c == '"')
				break;
			if (c == '\\') {
				if ((c = getc(fin)) == EOF) {
					*cp1++ = '\\';
					break;
				}
			}
			if (c == '\n') {
				yylineno++;
				c = ' '; /* can't send '\n' */
			}
			*cp1++ = c;
		}
		if (c != '"')
			yyerror("missing closing '\"'\n");
		*cp1 = '\0';
		yylval.string = xstrdup(yytext);
		return(STRING);

	case ':':  /* : or :: */
		if ((c = getc(fin)) == ':')
			return(DCOLON);
		(void) ungetc(c, fin);
		return(COLON);
	}
	cp1 = yytext;
	cp2 = &yytext[INMAX - 1];
	for (;;) {
		if (cp1 >= cp2) {
			yyerror("input line too long\n");
			break;
		}
		if (c == '\\') {
			if ((c = getc(fin)) != EOF) {
				if (any(c, quotechars))
					*cp1++ = QUOTECHAR;
			} else {
				*cp1++ = '\\';
				break;
			}
		}
		*cp1++ = c;
		c = getc(fin);
		if (c == EOF || any(c, " \"'\t()=;:\n")) {
			(void) ungetc(c, fin);
			break;
		}
	}
	*cp1 = '\0';
	if (yytext[0] == '-' && yytext[1] == CNULL) 
		return '-';
	if (yytext[0] == '-' && parendepth <= 0) {
		opt_t opt = 0;
		static char ebuf[BUFSIZ];

		switch (yytext[1]) {
		case 'o':
			if (parsedistopts(&yytext[2], &opt, TRUE)) {
				(void) snprintf(ebuf, sizeof(ebuf),
					        "Bad distfile options \"%s\".", 
					        &yytext[2]);
				yyerror(ebuf);
			}
			break;

			/*
			 * These options are obsoleted by -o.
			 */
		case 'b':	opt = DO_COMPARE;		break;
		case 'R':	opt = DO_REMOVE;		break;
		case 'v':	opt = DO_VERIFY;		break;
		case 'w':	opt = DO_WHOLE;			break;
		case 'y':	opt = DO_YOUNGER;		break;
		case 'h':	opt = DO_FOLLOW;		break;
		case 'i':	opt = DO_IGNLNKS;		break;
		case 'q':	opt = DO_QUIET;			break;
		case 'x':	opt = DO_NOEXEC;		break;
		case 'N':	opt = DO_CHKNFS;		break;
		case 'O':	opt = DO_CHKREADONLY;		break;
		case 's':	opt = DO_SAVETARGETS;		break;
		case 'r':	opt = DO_NODESCEND;		break;

		default:
			(void) snprintf(ebuf, sizeof(ebuf),
					"Unknown option \"%s\".", yytext);
			yyerror(ebuf);
		}

		yylval.optval = opt;
		return(OPTION);
	}
	if (!strcmp(yytext, "install"))
		c = INSTALL;
	else if (!strcmp(yytext, "notify"))
		c = NOTIFY;
	else if (!strcmp(yytext, "except"))
		c = EXCEPT;
	else if (!strcmp(yytext, "except_pat"))
		c = PATTERN;
	else if (!strcmp(yytext, "special"))
		c = SPECIAL;
	else if (!strcmp(yytext, "cmdspecial"))
		c = CMDSPECIAL;
	else {
		yylval.string = xstrdup(yytext);
		return(NAME);
	}
	yylval.subcmd = makesubcmd(c);
	return(c);
}

/*
 * XXX We should use strchr(), but most versions can't handle
 * some of the characters we use.
 */
int any(int c, char *str)
{
	while (*str)
		if (c == *str++)
			return(1);
	return(0);
}

/*
 * Insert or append ARROW command to list of hosts to be updated.
 */
void
insert(char *label, struct namelist *files, struct namelist *hosts,
    struct subcmd *subcmds)
{
	struct cmd *c, *prev, *nc;
	struct namelist *h, *lasth;

	debugmsg(DM_CALL, "insert(%s, %x, %x, %x) start, files = %s", 
		 label == NULL ? "(null)" : label,
		 files, hosts, subcmds, getnlstr(files));

	files = expand(files, E_VARS|E_SHELL);
	hosts = expand(hosts, E_ALL);
	for (h = hosts; h != NULL; lasth = h, h = h->n_next, 
	     free((char *)lasth)) {
		/*
		 * Search command list for an update to the same host.
		 */
		for (prev = NULL, c = cmds; c!=NULL; prev = c, c = c->c_next) {
			if (strcmp(c->c_name, h->n_name) == 0) {
				do {
					prev = c;
					c = c->c_next;
				} while (c != NULL &&
					strcmp(c->c_name, h->n_name) == 0);
				break;
			}
		}
		/*
		 * Insert new command to update host.
		 */
		nc = ALLOC(cmd);
		nc->c_type = ARROW;
		nc->c_name = h->n_name;
		nc->c_label = label;
		nc->c_files = files;
		nc->c_cmds = subcmds;
		nc->c_flags = 0;
		nc->c_next = c;
		if (prev == NULL)
			cmds = nc;
		else
			prev->c_next = nc;
		/* update last_cmd if appending nc to cmds */
		if (c == NULL)
			last_cmd = nc;
	}
}

/*
 * Append DCOLON command to the end of the command list since these are always
 * executed in the order they appear in the distfile.
 */
void
append(char *label, struct namelist *files, char *stamp, struct subcmd *subcmds)
{
	struct cmd *c;

	c = ALLOC(cmd);
	c->c_type = DCOLON;
	c->c_name = stamp;
	c->c_label = label;
	c->c_files = expand(files, E_ALL);
	c->c_cmds = subcmds;
	c->c_next = NULL;
	if (cmds == NULL)
		cmds = last_cmd = c;
	else {
		last_cmd->c_next = c;
		last_cmd = c;
	}
}

/*
 * Error printing routine in parser.
 */
void
yyerror(char *s)
{
	error("Error in distfile: line %d: %s", yylineno, s);
}

/*
 * Allocate a namelist structure.
 */
struct namelist *
makenl(char *name)
{
	struct namelist *nl;

	debugmsg(DM_CALL, "makenl(%s)", name == NULL ? "null" : name);

	nl = ALLOC(namelist);
	nl->n_name = name;
	nl->n_regex = NULL;
	nl->n_next = NULL;

	return(nl);
}


/*
 * Is the name p in the namelist nl?
 */
static int
innl(struct namelist *nl, char *p)
{
	for ( ; nl; nl = nl->n_next)
		if (!strcmp(p, nl->n_name))
			return(1);
	return(0);
}

/*
 * Join two namelists.
 */
static struct namelist *
addnl(struct namelist *n1, struct namelist *n2)
{
	struct namelist *nl, *prev;

	n1 = expand(n1, E_VARS);
	n2 = expand(n2, E_VARS);
	for (prev = NULL, nl = NULL; n1; n1 = n1->n_next, prev = nl) {
		nl = makenl(n1->n_name);
		nl->n_next = prev;
	}
	for (; n2; n2 = n2->n_next)
		if (!innl(nl, n2->n_name)) {
			nl = makenl(n2->n_name);
			nl->n_next = prev;
			prev = nl;
		}
	return(prev);
}

/*
 * Copy n1 except for elements that are in n2.
 */
static struct namelist *
subnl(struct namelist *n1, struct namelist *n2)
{
	struct namelist *nl, *prev;

	n1 = expand(n1, E_VARS);
	n2 = expand(n2, E_VARS);
	for (prev = NULL; n1; n1 = n1->n_next)
		if (!innl(n2, n1->n_name)) {
			nl = makenl(n1->n_name);
			nl->n_next = prev;
			prev = nl;
		}
	return(prev);
}

/*
 * Copy all items of n1 that are also in n2.
 */
static struct namelist *
andnl(struct namelist *n1, struct namelist *n2)
{
	struct namelist *nl, *prev;

	n1 = expand(n1, E_VARS);
	n2 = expand(n2, E_VARS);
	for (prev = NULL; n1; n1 = n1->n_next)
		if (innl(n2, n1->n_name)) {
			nl = makenl(n1->n_name);
			nl->n_next = prev;
			prev = nl;
		}
	return(prev);
}

/*
 * Make a sub command for lists of variables, commands, etc.
 */
struct subcmd *
makesubcmd(int type)
{
	struct subcmd *sc;

	sc = ALLOC(subcmd);
	sc->sc_type = type;
	sc->sc_args = NULL;
	sc->sc_next = NULL;
	sc->sc_name = NULL;

	return(sc);
}
