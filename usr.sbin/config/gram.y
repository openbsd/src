%{
/*	$OpenBSD: gram.y,v 1.4 1996/07/07 22:02:20 maja Exp $	*/
/*	$NetBSD: gram.y,v 1.7 1996/03/17 13:18:18 cgd Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *
 *	from: @(#)gram.y	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>
#include <ctype.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "sem.h"

#define	FORMAT(n) ((n) > -10 && (n) < 10 ? "%d" : "0x%x")

#define	stop(s)	error(s), exit(1)

int	include __P((const char *, int));
void	yyerror __P((const char *));
int	yylex __P((void));
extern const char *lastfile;

static	struct	config conf;	/* at most one active at a time */

/* the following is used to recover nvlist space after errors */
static	struct	nvlist *alloc[1000];
static	int	adepth;
#define	new0(n,s,p,i,x)	(alloc[adepth++] = newnv(n, s, p, i, x))
#define	new_n(n)	new0(n, NULL, NULL, 0, NULL)
#define	new_nx(n, x)	new0(n, NULL, NULL, 0, x)
#define	new_ns(n, s)	new0(n, s, NULL, 0, NULL)
#define	new_si(s, i)	new0(NULL, s, NULL, i, NULL)
#define	new_nsi(n,s,i)	new0(n, s, NULL, i, NULL)
#define	new_np(n, p)	new0(n, NULL, p, 0, NULL)
#define	new_s(s)	new0(NULL, s, NULL, 0, NULL)
#define	new_p(p)	new0(NULL, NULL, p, 0, NULL)
#define	new_px(p, x)	new0(NULL, NULL, p, 0, x)

#define	fx_atom(s)	new0(s, NULL, NULL, FX_ATOM, NULL)
#define	fx_not(e)	new0(NULL, NULL, NULL, FX_NOT, e)
#define	fx_and(e1, e2)	new0(NULL, NULL, e1, FX_AND, e2)
#define	fx_or(e1, e2)	new0(NULL, NULL, e1, FX_OR, e2)

static	void	cleanup __P((void));
static	void	setmachine __P((const char *, const char *));
static	void	setmaxpartitions __P((int));

%}

%union {
	struct	attr *attr;
	struct	devbase *devb;
	struct	deva *deva;
	struct	nvlist *list;
	const char *str;
	int	val;
}

%token	AND AT ATTACH COMPILE_WITH CONFIG DEFINE DEVICE DISABLE DUMPS ENDFILE
%token	XFILE FLAGS INCLUDE XMACHINE MAJOR MAKEOPTIONS MAXUSERS MAXPARTITIONS
%token	MINOR ON OPTIONS PSEUDO_DEVICE ROOT SWAP VECTOR WITH
%token	<val> FFLAG NUMBER
%token	<str> PATHNAME WORD

%type	<list>	fopts fexpr fatom
%type	<val>	fflgs
%type	<str>	rule
%type	<attr>	attr
%type	<devb>	devbase
%type	<deva>	devattach_opt
%type	<val>	disable
%type	<list>	atlist interface_opt
%type	<str>	atname
%type	<list>	loclist_opt loclist locdef
%type	<str>	locdefault
%type	<list>	veclist_opt veclist
%type	<list>	attrs_opt attrs
%type	<list>	locators locator
%type	<list>	swapdev_list dev_spec
%type	<str>	device_instance
%type	<str>	attachment
%type	<str>	value
%type	<val>	major_minor signed_number npseudo
%type	<val>	flags_opt

%%

/*
 * A configuration consists of a machine type, followed by the machine
 * definition files (via the include() mechanism), followed by the
 * configuration specification(s) proper.  In effect, this is two
 * separate grammars, with some shared terminals and nonterminals.
 */
Configuration:
	hdrs machine_spec		/* "machine foo" from machine descr. */
	dev_defs dev_eof		/* ../../conf/devices */
	dev_defs dev_eof		/* devices.foo */
	maxpart_spec dev_defs dev_eof	/* ../../conf/devices */
	specs;				/* rest of machine description */

hdrs:
	hdrs hdr |
	/* empty */;

hdr:
	include |
	'\n';

machine_spec:
	XMACHINE WORD			= { setmachine($2,NULL); } |
	XMACHINE WORD WORD		= { setmachine($2,$3); } |
	error = { stop("cannot proceed without machine specifier"); };

dev_eof:
	ENDFILE				= { enddefs(lastfile); checkfiles(); };

maxpart_blanks:
	maxpart_blanks '\n' |
	/* empty */;

maxpart_spec:
	maxpart_blanks MAXPARTITIONS NUMBER	= { setmaxpartitions($3); } |
	error = { stop("cannot proceed without maxpartitions specifier"); };

/*
 * Various nonterminals shared between the grammars.
 */
file:
	XFILE PATHNAME fopts fflgs rule	= { addfile($2, $3, $4, $5); };

/* order of options is important, must use right recursion */
fopts:
	fexpr				= { $$ = $1; } |
	/* empty */			= { $$ = NULL; };

fexpr:
	fatom				= { $$ = $1; } |
	'!' fatom			= { $$ = fx_not($2); } |
	fexpr '&' fexpr			= { $$ = fx_and($1, $3); } |
	fexpr '|' fexpr			= { $$ = fx_or($1, $3); } |
	'(' fexpr ')'			= { $$ = $2; };

fatom:
	WORD				= { $$ = fx_atom($1); };

fflgs:
	fflgs FFLAG			= { $$ = $1 | $2; } |
	/* empty */			= { $$ = 0; };

rule:
	COMPILE_WITH WORD		= { $$ = $2; } |
	/* empty */			= { $$ = NULL; };

include:
	INCLUDE WORD			= { (void)include($2, '\n'); };

/*
 * The machine definitions grammar.
 */
dev_defs:
	dev_defs dev_def |
	/* empty */;

dev_def:
	one_def '\n'			= { adepth = 0; } |
	'\n' |
	error '\n'			= { cleanup(); };

one_def:
	file |
	include |
	DEFINE WORD interface_opt	= { (void)defattr($2, $3); } |
	DEVICE devbase interface_opt attrs_opt
					= { defdev($2, 0, $3, $4); } |
	ATTACH devbase AT atlist veclist_opt devattach_opt attrs_opt
					= { defdevattach($6, $2, $4, $5	,
				    $7); } |
	MAXUSERS NUMBER NUMBER NUMBER	= { setdefmaxusers($2, $3, $4); } |
	PSEUDO_DEVICE devbase attrs_opt = { defdev($2,1,NULL,$3); } |
	MAJOR '{' majorlist '}';

disable:
	DISABLE				= { $$ = 1; } |
	/* empty */			= { $$ = 0; };

atlist:
	atlist ',' atname		= { $$ = new_nx($3, $1); } |
	atname				= { $$ = new_n($1); };

atname:
	WORD				= { $$ = $1; } |
	ROOT				= { $$ = NULL; };

veclist_opt:
	VECTOR veclist			= { $$ = $2; } |
	/* empty */			= { $$ = NULL; };

/* veclist order matters, must use right recursion */
veclist:
	WORD veclist			= { $$ = new_nx($1, $2); } |
	WORD				= { $$ = new_n($1); };

devbase:
	WORD				= { $$ = getdevbase($1); };

devattach_opt:
	WITH WORD			= { $$ = getdevattach($2); } |
	/* empty */			= { $$ = NULL; };

interface_opt:
	'{' loclist_opt '}'		= { $$ = new_nx("", $2); } |
	/* empty */			= { $$ = NULL; };

loclist_opt:
	loclist				= { $$ = $1; } |
	/* empty */			= { $$ = NULL; };

/* loclist order matters, must use right recursion */
loclist:
	locdef ',' loclist		= { ($$ = $1)->nv_next = $3; } |
	locdef				= { $$ = $1; };

/* "[ WORD locdefault ]" syntax may be unnecessary... */
locdef:
	WORD locdefault 		= { $$ = new_nsi($1, $2, 0); } |
	WORD				= { $$ = new_nsi($1, NULL, 0); } |
	'[' WORD locdefault ']'		= { $$ = new_nsi($2, $3, 1); };

locdefault:
	'=' value			= { $$ = $2; };

value:
	WORD				= { $$ = $1; } |
	signed_number			= { char bf[40];
					    (void)sprintf(bf, FORMAT($1), $1);
					    $$ = intern(bf); };

signed_number:
	NUMBER				= { $$ = $1; } |
	'-' NUMBER			= { $$ = -$2; };

attrs_opt:
	':' attrs			= { $$ = $2; } |
	/* empty */			= { $$ = NULL; };

attrs:
	attrs ',' attr			= { $$ = new_px($3, $1); } |
	attr				= { $$ = new_p($1); };

attr:
	WORD				= { $$ = getattr($1); };

majorlist:
	majorlist ',' majordef |
	majordef;

majordef:
	devbase '=' NUMBER		= { setmajor($1, $3); };



/*
 * The configuration grammar.
 */
specs:
	specs spec |
	/* empty */;

spec:
	config_spec '\n'		= { adepth = 0; } |
	'\n' |
	error '\n'			= { cleanup(); };

config_spec:
	file |
	include |
	OPTIONS opt_list |
	MAKEOPTIONS mkopt_list |
	MAXUSERS NUMBER			= { setmaxusers($2); } |
	CONFIG conf sysparam_list	= { addconf(&conf); } |
	PSEUDO_DEVICE WORD npseudo	= { addpseudo($2, $3); } |
	device_instance AT attachment disable locators flags_opt
					= { adddev($1, $3, $5, $6, $4); };

mkopt_list:
	mkopt_list ',' mkoption |
	mkoption;

mkoption:
	WORD '=' value			= { addmkoption($1, $3); }

opt_list:
	opt_list ',' option |
	option;

option:
	WORD				= { addoption($1, NULL); } |
	WORD '=' value			= { addoption($1, $3); };

conf:
	WORD				= { conf.cf_name = $1;
					    conf.cf_lineno = currentline();
					    conf.cf_root = NULL;
					    conf.cf_swap = NULL;
					    conf.cf_dump = NULL; };

sysparam_list:
	sysparam_list sysparam |
	sysparam;

sysparam:
	ROOT on_opt dev_spec	 = { setconf(&conf.cf_root, "root", $3); } |
	SWAP on_opt swapdev_list = { setconf(&conf.cf_swap, "swap", $3); } |
	DUMPS on_opt dev_spec	 = { setconf(&conf.cf_dump, "dumps", $3); };

swapdev_list:
	dev_spec AND swapdev_list	= { ($$ = $1)->nv_next = $3; } |
	dev_spec			= { $$ = $1; };

dev_spec:
	WORD				= { $$ = new_si($1, NODEV); } |
	major_minor			= { $$ = new_si(NULL, $1); };

major_minor:
	MAJOR NUMBER MINOR NUMBER	= { $$ = makedev($2, $4); };

on_opt:
	ON | /* empty */;

npseudo:
	NUMBER				= { $$ = $1; } |
	/* empty */			= { $$ = 1; };

device_instance:
	WORD '*'			= { $$ = starref($1); } |
	WORD				= { $$ = $1; };

attachment:
	ROOT				= { $$ = NULL; } |
	WORD '?'			= { $$ = wildref($1); } |
	WORD				= { $$ = $1; };

locators:
	locators locator		= { ($$ = $2)->nv_next = $1; } |
	/* empty */			= { $$ = NULL; };

locator:
	WORD value			= { $$ = new_ns($1, $2); } |
	WORD '?'			= { $$ = new_ns($1, NULL); };

flags_opt:
	FLAGS NUMBER			= { $$ = $2; } |
	/* empty */			= { $$ = 0; };

%%

void
yyerror(s)
	const char *s;
{

	error("%s", s);
}

/*
 * Cleanup procedure after syntax error: release any nvlists
 * allocated during parsing the current line.
 */
static void
cleanup()
{
	register struct nvlist **np;
	register int i;

	for (np = alloc, i = adepth; --i >= 0; np++)
		nvfree(*np);
	adepth = 0;
}

static void
setmachine(mch, mcharch)
	const char *mch;
	const char *mcharch;
{
	char buf[MAXPATHLEN], archbuf[MAXPATHLEN];

	machine = mch;
	machinearch = mcharch;
	if (machinearch != NULL)
		(void)sprintf(archbuf, "../../%s/conf/files.%s",
		    machinearch, machinearch);
	else
		strncpy(archbuf, _PATH_DEVNULL, MAXPATHLEN);
	(void)sprintf(buf, "files.%s", machine);

	if (include(buf, ENDFILE) ||
	    include(archbuf, ENDFILE) ||
	    include("../../../conf/files", ENDFILE))
		exit(1);
}

static void
setmaxpartitions(n)
	int n;
{

	maxpartitions = n;
}
