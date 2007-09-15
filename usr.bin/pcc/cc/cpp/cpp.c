/*	$OpenBSD: cpp.c,v 1.2 2007/09/15 22:04:39 ray Exp $	*/

/*
 * Copyright (c) 2004 Anders Magnusson (ragge@ludd.luth.se).
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code and documentation must retain the above
 * copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 * 	This product includes software developed or owned by Caldera
 *	International, Inc.
 * Neither the name of Caldera International, Inc. nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OFLIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * The C preprocessor.
 * This code originates from the V6 preprocessor with some additions
 * from V7 cpp, and at last ansi/c99 support.
 */

#include "../../config.h"

#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "cpp.h"
#include "y.tab.h"

#define	MAXARG	250	/* # of args to a macro, limited by char value */
#define	SBSIZE	600000

static usch	sbf[SBSIZE];
/* C command */

int tflag;	/* traditional cpp syntax */
#ifdef CPP_DEBUG
int dflag;	/* debug printouts */
#define	DPRINT(x) if (dflag) printf x
#define	DDPRINT(x) if (dflag > 1) printf x
#else
#define DPRINT(x)
#define DDPRINT(x)
#endif

int ofd;
static usch outbuf[CPPBUF];
static int obufp, istty;
int Cflag, Mflag;
usch *Mfile;
struct initar *initar;

/* avoid recursion */
struct recur {
	struct recur *next;
	struct symtab *sp;
};

/* include dirs */
struct incs {
	struct incs *next;
	usch *dir;
} *incdir[2];
#define	INCINC 0
#define	SYSINC 1

static struct symtab *filloc;
static struct symtab *linloc;
int	trulvl;
int	flslvl;
int	elflvl;
int	elslvl;
usch *stringbuf = sbf;

/*
 * Macro replacement list syntax:
 * - For object-type macros, replacement strings are stored as-is.
 * - For function-type macros, macro args are substituted for the
 *   character WARN followed by the argument number.
 * - The value element points to the end of the string, to simplify
 *   pushback onto the input queue.
 * 
 * The first character (from the end) in the replacement list is
 * the number of arguments:
 *   VARG  - ends with ellipsis, next char is argcount without ellips.
 *   OBJCT - object-type macro
 *   0 	   - empty parenthesis, foo()
 *   1->   - number of args.
 */

#define	VARG	0xfe	/* has varargs */
#define	OBJCT	0xff
#define	WARN	1	/* SOH, not legal char */
#define	CONC	2	/* STX, not legal char */
#define	SNUFF	3	/* ETX, not legal char */
#define	NOEXP	4	/* EOT, not legal char */
#define	EXPAND	5	/* ENQ, not legal char */

/* args for lookup() */
#define	FIND	0
#define	ENTER	1

static void expdef(usch *proto, struct recur *, int gotwarn);
void define(void);
static int canexpand(struct recur *, struct symtab *np);
void include(void);
void line(void);
void flbuf(void);
void usage(void);

int
main(int argc, char **argv)
{
	struct initar *it;
	struct incs *w, *w2;
	struct symtab *nl;
	register int ch;
//	usch *osp;

	while ((ch = getopt(argc, argv, "CD:I:MS:U:di:t")) != -1)
		switch (ch) {
		case 'C': /* Do not discard comments */
			Cflag++;
			break;

		case 'i': /* include */
		case 'U': /* undef */
		case 'D': /* define something */
			it = malloc(sizeof(struct initar));
			it->type = ch;
			it->str = optarg;
			it->next = initar;
			initar = it;
#if 0
			osp = (usch *)optarg;
			while (*osp && *osp != '=')
				osp++;
			if (*osp == '=') {
				*osp++ = 0;
				while (*osp)
					osp++;
				*osp = OBJCT;
			} else {
				static usch c[3] = { 0, '1', OBJCT };
				osp = &c[2];
			}
			nl = lookup((usch *)optarg, ENTER);
			if (nl->value) {
				/* check for redefinition */
				usch *o = nl->value, *n = osp;
				while (*o && *o == *n)
					o--, n--;
				if (*o || *o != *n)
					error("%s redefined", optarg);
			}
			nl->value = osp;
#endif
			break;

		case 'M': /* Generate dependencies for make */
			Mflag++;
			break;

		case 'S':
		case 'I':
			w = calloc(sizeof(struct incs), 1);
			w->dir = (usch *)optarg;
			w2 = incdir[ch == 'I' ? INCINC : SYSINC];
			if (w2 != NULL) {
				while (w2->next)
					w2 = w2->next;
				w2->next = w;
			} else
				incdir[ch == 'I' ? INCINC : SYSINC] = w;
			break;

#if 0
		case 'U':
			if ((nl = lookup((usch *)optarg, FIND)))
				nl->value = NULL;
			break;
#endif
#ifdef CPP_DEBUG
		case 'd':
			dflag++;
			break;
#endif
		case 't':
			tflag = 1;
			break;

#if 0
		case 'i':
			if (ifile)
				error("max 1 -i entry");
			ifile = optarg;
			break;
#endif

		case '?':
			usage();
		default:
			error("bad arg %c\n", ch);
		}
	argc -= optind;
	argv += optind;

	filloc = lookup((usch *)"__FILE__", ENTER);
	linloc = lookup((usch *)"__LINE__", ENTER);
	filloc->value = linloc->value = (usch *)""; /* Just something */

	if (tflag == 0) {
		time_t t = time(NULL);
		usch *n = (usch *)ctime(&t);

		/*
		 * Manually move in the predefined macros.
		 */
		nl = lookup((usch *)"__TIME__", ENTER);
		savch(0); savch('"');  n[19] = 0; savstr(&n[11]); savch('"');
		savch(OBJCT);
		nl->value = stringbuf-1;

		nl = lookup((usch *)"__DATE__", ENTER);
		savch(0); savch('"'); n[24] = n[11] = 0; savstr(&n[4]);
		savstr(&n[20]); savch('"'); savch(OBJCT);
		nl->value = stringbuf-1;

		nl = lookup((usch *)"__STDC__", ENTER);
		savch(0); savch('1'); savch(OBJCT);
		nl->value = stringbuf-1;
	}

	if (Mflag) {
		usch *c;

		if (argc < 1)
			error("-M and no infile");
		if ((c = (usch *)strrchr(argv[0], '/')) == NULL)
			c = (usch *)argv[0];
		else
			c++;
		Mfile = stringbuf;
		savstr(c); savch(0);
		if ((c = (usch *)strrchr((char *)Mfile, '.')) == NULL)
			error("-M and no extension: ");
		c[1] = 'o';
		c[2] = 0;
	}

	if (argc == 2) {
		if ((ofd = open(argv[1], O_WRONLY|O_CREAT, 0600)) < 0)
			error("Can't creat %s", argv[1]);
	} else
		ofd = 1; /* stdout */
	istty = isatty(ofd);

	if (pushfile((usch *)(argc && strcmp(argv[0], "-") ? argv[0] : NULL)))
		error("cannot open %s", argv[0]);

	flbuf();
	close(ofd);
	return 0;
}

/*
 * Expand the symbol nl read from input.
 * Return a pointer to the fully expanded result.
 * It is the responsibility of the caller to reset the heap usage.
 */
usch *
gotident(struct symtab *nl)
{
	struct symtab *thisnl;
	usch *osp, *ss2, *base;
	int c;

	thisnl = NULL;
	slow = 1;
	base = osp = stringbuf;
	goto found;

	while ((c = yylex()) != 0) {
		switch (c) {
		case IDENT:
			if (flslvl)
				break;
			osp = stringbuf;

			DPRINT(("IDENT0: %s\n", yytext));
			nl = lookup((usch *)yytext, FIND);
			if (nl == 0 || thisnl == 0)
				goto found;
			if (thisnl == nl) {
				nl = 0;
				goto found;
			}
			ss2 = stringbuf;
			if ((c = yylex()) == WSPACE) {
				savstr((usch *)yytext);
				c = yylex();
			}
			if (c != EXPAND) {
				unpstr((usch *)yytext);
				if (ss2 != stringbuf)
					unpstr(ss2);
				unpstr(nl->namep);
				(void)yylex(); /* get yytext correct */
				nl = 0; /* ignore */
			} else {
				thisnl = NULL;
				if (nl->value[0] == OBJCT) {
					unpstr(nl->namep);
					(void)yylex(); /* get yytext correct */
					nl = 0;
				}
			}
			stringbuf = ss2;

found:			if (nl == 0 || subst(nl, NULL) == 0) {
				if (nl)
					savstr(nl->namep);
				else
					savstr((usch *)yytext);
			} else if (osp != stringbuf) {
				DPRINT(("IDENT1: unput osp %p stringbuf %p\n",
				    osp, stringbuf));
				ss2 = stringbuf;
				cunput(EXPAND);
				while (ss2 > osp)
					cunput(*--ss2);
				thisnl = nl;
				stringbuf = osp; /* clean up heap */
			}
			break;

		case EXPAND:
			DPRINT(("EXPAND!\n"));
			thisnl = NULL;
			break;

		case STRING:
		case '\n':
		case NUMBER:
		case FPOINT:
		case WSPACE:
			savstr((usch *)yytext);
			break;

		default:
			if (c < 256)
				savch(c);
			else
				savstr((usch *)yytext);
			break;
		}
		if (thisnl == NULL) {
			slow = 0;
			savch(0);
			return base;
		}
	}
	error("preamture EOF");
	/* NOTREACHED */
	return NULL; /* XXX gcc */
}

void
line()
{
	static usch *lbuf;
	static int llen;
	int c;

	slow = 1;
	if (yylex() != WSPACE)
		goto bad;
	if ((c = yylex()) != IDENT || !isdigit((int)yytext[0]))
		goto bad;
	ifiles->lineno = atoi(yytext);

	if ((c = yylex()) != '\n' && c != WSPACE)
		goto bad;
	if (c == '\n') {
		slow = 0;
		return;
	}
	if (yylex() != STRING)
		goto bad;
	c = strlen((char *)yytext);
	if (llen < c) {
		/* XXX may loose heap space */
		lbuf = stringbuf;
		stringbuf += c;
		llen = c;
	}
	yytext[strlen(yytext)-1] = 0;
	strcpy((char *)lbuf, &yytext[1]);
	ifiles->fname = lbuf;
	if (yylex() != '\n')
		goto bad;
	slow = 0;
	return;

bad:	error("bad line directive");
}

/*
 * Include a file. Include order:
 * - For <...> files, first search -I directories, then system directories.
 * - For "..." files, first search "current" dir, then as <...> files.
 */
void
include()
{
	struct incs *w;
	struct symtab *nl;
	usch *osp;
	usch *fn;
	int i, c, it;

	if (flslvl)
		return;
	osp = stringbuf;
	slow = 1;
	if (yylex() != WSPACE)
		goto bad;
again:	if ((c = yylex()) != STRING && c != '<' && c != IDENT)
		goto bad;

	if (c == IDENT) {
		if ((nl = lookup((usch *)yytext, FIND)) == NULL)
			goto bad;
		if (subst(nl, NULL) == 0)
			goto bad;
		savch('\0');
		unpstr(osp);
		goto again;
	} else if (c == '<') {
		fn = stringbuf;
		while ((c = yylex()) != '>' && c != '\n') {
			if (c == '\n')
				goto bad;
			savstr((usch *)yytext);
		}
		savch('\0');
		while ((c = yylex()) == WSPACE)
			;
		if (c != '\n')
			goto bad;
		it = SYSINC;
	} else {
		usch *nm = stringbuf;

		yytext[strlen(yytext)-1] = 0;
		fn = (usch *)&yytext[1];
		/* first try to open file relative to previous file */
		/* but only if it is not an absolute path */
		if (*fn != '/') {
			savstr(ifiles->orgfn);
			if ((stringbuf =
			    (usch *)strrchr((char *)nm, '/')) == NULL)
				stringbuf = nm;
			else
				stringbuf++;
		}
		savstr(fn); savch(0);
		while ((c = yylex()) == WSPACE)
			;
		if (c != '\n')
			goto bad;
		slow = 0;
		if (pushfile(nm) == 0)
			return;
		stringbuf = nm;
	}

	/* create search path and try to open file */
	slow = 0;
	for (i = 0; i < 2; i++) {
		for (w = incdir[i]; w; w = w->next) {
			usch *nm = stringbuf;

			savstr(w->dir); savch('/');
			savstr(fn); savch(0);
			if (pushfile(nm) == 0)
				return;
			stringbuf = nm;
		}
	}
	error("cannot find '%s'", fn);
	/* error() do not return */

bad:	error("bad include");
	/* error() do not return */
}

static int
definp(void)
{
	int c;

	do
		c = yylex();
	while (c == WSPACE);
	return c;
}

void
define()
{
	struct symtab *np;
	usch *args[MAXARG], *ubuf, *sbeg;
	int c, i, redef;
	int mkstr = 0, narg = -1;
	int ellips = 0;

	if (flslvl)
		return;
	slow = 1;
	if (yylex() != WSPACE || yylex() != IDENT)
		goto bad;

	if (isdigit((int)yytext[0]))
		goto bad;

	np = lookup((usch *)yytext, ENTER);
	redef = np->value != NULL;

	sbeg = stringbuf;
	if ((c = yylex()) == '(') {
		narg = 0;
		/* function-like macros, deal with identifiers */
		for (;;) {
			c = definp();
			if (c == ')')
				break;
			if (c == ELLIPS) {
				ellips = 1;
				if (definp() != ')')
					goto bad;
				break;
			}
			if (c == IDENT) {
				args[narg] = alloca(strlen(yytext)+1);
				strcpy((char *)args[narg], yytext);
				narg++;
				if ((c = definp()) == ',')
					continue;
				if (c == ')')
					break;
				goto bad;
			}
			goto bad;
		}
		c = yylex();
	} else if (c == '\n') {
		/* #define foo */
		;
	} else if (c != WSPACE)
		goto bad;

	while (c == WSPACE)
		c = yylex();

	/* parse replacement-list, substituting arguments */
	savch('\0');
	while (c != '\n') {
		switch (c) {
		case WSPACE:
			/* remove spaces if it surrounds a ## directive */
			ubuf = stringbuf;
			savstr((usch *)yytext);
			c = yylex();
			if (c == CONCAT) {
				stringbuf = ubuf;
				savch(CONC);
				if ((c = yylex()) == WSPACE)
					c = yylex();
			}
			continue;

		case CONCAT:
			/* No spaces before concat op */
			savch(CONC);
			if ((c = yylex()) == WSPACE)
				c = yylex();
			continue;

		case MKSTR:
			if (narg < 0) {
				/* no meaning in object-type macro */
				savch('#');
				break;
			}
			/* remove spaces between # and arg */
			savch(SNUFF);
			if ((c = yylex()) == WSPACE)
				c = yylex(); /* whitespace, ignore */
			mkstr = 1;
			if (c == VA_ARGS)
				continue;

			/* FALLTHROUGH */
		case IDENT:
			if (narg < 0)
				goto id; /* just add it if object */
			/* check if its an argument */
			for (i = 0; i < narg; i++)
				if (strcmp(yytext, (char *)args[i]) == 0)
					break;
			if (i == narg) {
				if (mkstr)
					error("not argument");
				goto id;
			}
			savch(i);
			savch(WARN);
			if (mkstr)
				savch(SNUFF), mkstr = 0;
			break;

		case VA_ARGS:
			if (ellips == 0)
				error("unwanted %s", yytext);
			savch(VARG);
			savch(WARN);
			if (mkstr)
				savch(SNUFF), mkstr = 0;
			break;

		default:
id:			savstr((usch *)yytext);
			break;
		}
		c = yylex();
	}
	/* remove trailing whitespace */
	while (stringbuf > sbeg) {
		if (stringbuf[-1] == ' ' || stringbuf[-1] == '\t')
			stringbuf--;
		else
			break;
	}
	if (ellips) {
		savch(narg);
		savch(VARG);
	} else
		savch(narg < 0 ? OBJCT : narg);
	if (redef) {
		usch *o = np->value, *n = stringbuf-1;

		/* Redefinition to identical replacement-list is allowed */
		while (*o && *o == *n)
			o--, n--;
		if (*o || *o != *n)
			error("%s redefined\nprevious define: %s:%d",
			    np->namep, np->file, np->line);
		stringbuf = sbeg;  /* forget this space */
	} else
		np->value = stringbuf-1;

#ifdef CPP_DEBUG
	if (dflag) {
		usch *w = np->value;

		printf("!define: ");
		if (*w == OBJCT)
			printf("[object]");
		else if (*w == VARG)
			printf("[VARG%d]", *--w);
		while (*--w) {
			switch (*w) {
			case WARN: printf("<%d>", *--w); break;
			case CONC: printf("<##>"); break;
			case SNUFF: printf("<\">"); break;
			default: putchar(*w); break;
			}
		}
		putchar('\n');
	}
#endif
	slow = 0;
	return;

bad:	error("bad define");
}

void
xerror(usch *s)
{
	usch *t;

	flbuf();
	savch(0);
	if (ifiles != NULL) {
		t = sheap("%s:%d: ", ifiles->fname, ifiles->lineno);
		write (2, t, strlen((char *)t));
	}
	write (2, s, strlen((char *)s));
	write (2, "\n", 1);
	exit(1);
}

/*
 * store a character into the "define" buffer.
 */
void
savch(c)
{
	if (stringbuf-sbf < SBSIZE) {
		*stringbuf++ = c;
	} else {
		stringbuf = sbf; /* need space to write error message */
		error("Too much defining");
	} 
}

/*
 * substitute namep for sp->value.
 */
int
subst(sp, rp)
struct symtab *sp;
struct recur *rp;
{
	struct recur rp2;
	register usch *vp, *cp;
	int c, rv = 0, ws;

	DPRINT(("subst: %s\n", sp->namep));
	/*
	 * First check for special macros.
	 */
	if (sp == filloc) {
		(void)sheap("\"%s\"", ifiles->fname);
		return 1;
	} else if (sp == linloc) {
		(void)sheap("%d", ifiles->lineno);
		return 1;
	}
	vp = sp->value;

	rp2.next = rp;
	rp2.sp = sp;

	if (*vp-- != OBJCT) {
		int gotwarn = 0;

		/* should we be here at all? */
		/* check if identifier is followed by parentheses */
		rv = 1;
		ws = 0;
		do {
			c = yylex();
			if (c == WARN) {
				gotwarn++;
				if (rp == NULL)
					goto noid;
			} else if (c == WSPACE)
				ws = 1;
		} while (c == WSPACE || c == '\n' || c == WARN);

		cp = (usch *)yytext;
		while (*cp)
			cp++;
		while (cp > (usch *)yytext)
			cunput(*--cp);
		DPRINT(("c %d\n", c));
		if (c == '(' ) {
			expdef(vp, &rp2, gotwarn);
			return rv;
		} else {
			/* restore identifier */
noid:			while (gotwarn--)
				cunput(WARN);
			if (ws)
				cunput(' ');
			cp = sp->namep;
			while (*cp)
				cp++;
			while (cp > sp->namep)
				cunput(*--cp);
			if ((c = yylex()) != IDENT)
				error("internal sync error");
			return 0;
		}
	} else {
		cunput(WARN);
		cp = vp;
		while (*cp) {
			if (*cp != CONC)
				cunput(*cp);
			cp--;
		}
		expmac(&rp2);
	}
	return 1;
}

/*
 * do macro-expansion until WARN character read.
 * read from lex buffer and store result on heap.
 * will recurse into lookup() for recursive expansion.
 * when returning all expansions on the token list is done.
 */
void
expmac(struct recur *rp)
{
	struct symtab *nl;
	int c, noexp = 0, orgexp;
	usch *och, *stksv;
	extern int yyleng;

#ifdef CPP_DEBUG
	if (dflag) {
		struct recur *rp2 = rp;
		printf("\nexpmac\n");
		while (rp2) {
			printf("do not expand %s\n", rp->sp->namep);
			rp2 = rp2->next;
		}
	}
#endif
	while ((c = yylex()) != WARN) {
		switch (c) {
		case NOEXP: noexp++; break;
		case EXPAND: noexp--; break;

		case IDENT:
			/*
			 * Handle argument concatenation here.
			 * If an identifier is found and directly 
			 * after EXPAND or NOEXP then push the
			 * identifier back on the input stream and
			 * call yylex() again.
			 * Be careful to keep the noexp balance.
			 */
			och = stringbuf;
			savstr((usch *)yytext);
			DDPRINT(("id: str %s\n", och));

			orgexp = 0;
			while ((c = yylex()) == EXPAND || c == NOEXP)
				if (c == EXPAND)
					orgexp--;
				else
					orgexp++;

			DDPRINT(("id1: noexp %d orgexp %d\n", noexp, orgexp));
			if (c == IDENT) { /* XXX numbers? */
				DDPRINT(("id2: str %s\n", yytext));
				/* OK to always expand here? */
				savstr((usch *)yytext);
				switch (orgexp) {
				case 0: /* been EXP+NOEXP */
					if (noexp == 0)
						break;
					if (noexp != 1)
						error("case 0");
					cunput(NOEXP);
					noexp = 0;
					break;
				case -1: /* been EXP */
					if (noexp != 1)
						error("case -1");
					noexp = 0;
					break;
				case 1:
					if (noexp != 0)
						error("case 1");
					cunput(NOEXP);
					break;
				default:
					error("orgexp = %d", orgexp);
				}
				unpstr(och);
				stringbuf = och;
				continue; /* New longer identifier */
			}
			unpstr((usch *)yytext);
			if (orgexp == -1)
				cunput(EXPAND);
			else if (orgexp == 1)
				cunput(NOEXP);
			unpstr(och);
			stringbuf = och;


			yylex(); /* XXX reget last identifier */

			if ((nl = lookup((usch *)yytext, FIND)) == NULL)
				goto def;

			if (canexpand(rp, nl) == 0)
				goto def;
			/*
			 * If noexp == 0 then expansion of any macro is 
			 * allowed.  If noexp == 1 then expansion of a
			 * fun-like macro is allowed iff there is an 
			 * EXPAND between the identifier and the '('.
			 */
			if (noexp == 0) {
				if ((c = subst(nl, rp)) == 0)
					goto def;
				break;
			}
//printf("noexp1 %d nl->namep %s\n", noexp, nl->namep);
//if (noexp > 1) goto def;
			if (noexp != 1)
				error("bad noexp %d", noexp);
			stksv = NULL;
			if ((c = yylex()) == WSPACE) {
				stksv = alloca(yyleng+1);
				strcpy((char *)stksv, yytext);
				c = yylex();
			}
			/* only valid for expansion if fun macro */
			if (c == EXPAND && *nl->value != OBJCT) {
				noexp--;
				if (subst(nl, rp))
					break;
				savstr(nl->namep);
				if (stksv)
					savstr(stksv);
			} else {
				unpstr((usch *)yytext);
				if (stksv)
					unpstr(stksv);
				savstr(nl->namep);
			}
			break;

		case STRING:
			/* remove EXPAND/NOEXP from strings */
			if (yytext[1] == NOEXP) {
				savch('"');
				och = (usch *)&yytext[2];
				while (*och != EXPAND)
					savch(*och++);
				savch('"');
				break;
			}
			/* FALLTHROUGH */

def:		default:
			savstr((usch *)yytext);
			break;
		}
	}
	if (noexp)
		error("expmac noexp=%d", noexp);
	DPRINT(("return from expmac\n"));
}

/*
 * expand a function-like macro.
 * vp points to end of replacement-list
 * reads function arguments from yylex()
 * result is written on top of heap
 */
void
expdef(vp, rp, gotwarn)
	usch *vp;
	struct recur *rp;
{
	usch **args, *sptr, *ap, *bp, *sp;
	int narg, c, i, plev, snuff, instr;
	int ellips = 0;

	DPRINT(("expdef %s rp %s\n", vp, (rp ? (char *)rp->sp->namep : "")));
	if ((c = yylex()) != '(')
		error("got %c, expected )", c);
	if (vp[1] == VARG) {
		narg = *vp--;
		ellips = 1;
	} else
		narg = vp[1];
	args = alloca(sizeof(usch *) * (narg+ellips));

	/*
	 * read arguments and store them on heap.
	 * will be removed just before return from this function.
	 */
	sptr = stringbuf;
	for (i = 0; i < narg && c != ')'; i++) {
		args[i] = stringbuf;
		plev = 0;
		while ((c = yylex()) == WSPACE || c == '\n')
			;
		for (;;) {
			if (plev == 0 && (c == ')' || c == ','))
				break;
			if (c == '(')
				plev++;
			if (c == ')')
				plev--;
			savstr((usch *)yytext);
			while ((c = yylex()) == '\n')
				savch('\n');
		}
		while (args[i] < stringbuf &&
		    (stringbuf[-1] == ' ' || stringbuf[-1] == '\t'))
			stringbuf--;
		savch('\0');
	}
	if (ellips)
		args[i] = (usch *)"";
	if (ellips && c != ')') {
		args[i] = stringbuf;
		plev = 0;
		while ((c = yylex()) == WSPACE)
			;
		for (;;) {
			if (plev == 0 && c == ')')
				break;
			if (c == '(')
				plev++;
			if (c == ')')
				plev--;
			savstr((usch *)yytext);
			while ((c = yylex()) == '\n')
				savch('\n');
		}
		while (args[i] < stringbuf &&
		    (stringbuf[-1] == ' ' || stringbuf[-1] == '\t'))
			stringbuf--;
		savch('\0');
		
	}
	if (narg == 0 && ellips == 0)
		c = yylex();
	if (c != ')' || (i != narg && ellips == 0) || (i < narg && ellips == 1))
		error("wrong arg count");

	while (gotwarn--)
		cunput(WARN);

	sp = vp;
	instr = snuff = 0;

	/*
	 * push-back replacement-list onto lex buffer while replacing
	 * arguments. 
	 */
	cunput(WARN);
	while (*sp != 0) {
		if (*sp == SNUFF)
			cunput('\"'), snuff ^= 1;
		else if (*sp == CONC)
			;
		else if (*sp == WARN) {

			if (sp[-1] == VARG) {
				bp = ap = args[narg];
				sp--;
			} else
				bp = ap = args[(int)*--sp];
			if (sp[2] != CONC && !snuff && sp[-1] != CONC) {
				cunput(WARN);
				while (*bp)
					bp++;
				while (bp > ap)
					cunput(*--bp);
				DPRINT(("expand arg %d string %s\n", *sp, ap));
				bp = ap = stringbuf;
				savch(NOEXP);
				expmac(NULL);
				savch(EXPAND);
				savch('\0');
			}
			while (*bp)
				bp++;
			while (bp > ap) {
				bp--;
				if (snuff && !instr && 
				    (*bp == ' ' || *bp == '\t' || *bp == '\n')){
					while (*bp == ' ' || *bp == '\t' ||
					    *bp == '\n') {
						bp--;
					}
					cunput(' ');
				}
				cunput(*bp);
				if ((*bp == '\'' || *bp == '"')
				     && bp[-1] != '\\' && snuff) {
					instr ^= 1;
					if (instr == 0 && *bp == '"')
						cunput('\\');
				}
				if (instr && (*bp == '\\' || *bp == '"'))
					cunput('\\');
			}
		} else
			cunput(*sp);
		sp--;
	}
	stringbuf = sptr;

	/* scan the input buffer (until WARN) and save result on heap */
	expmac(rp);
}

usch *
savstr(usch *str)
{
	usch *rv = stringbuf;

	do {
		if (stringbuf >= &sbf[SBSIZE])   {
			stringbuf = sbf; /* need space to write error message */
			error("out of macro space!");
		}
	} while ((*stringbuf++ = *str++));
	stringbuf--;
	return rv;
}

int
canexpand(struct recur *rp, struct symtab *np)
{
	struct recur *w;

	for (w = rp; w && w->sp != np; w = w->next)
		;
	if (w != NULL)
		return 0;
	return 1;
}

void
unpstr(usch *c)
{
	usch *d = c;

	while (*d)
		d++;
	while (d > c) {
		cunput(*--d);
	}
}

void
flbuf()
{
	if (obufp == 0)
		return;
	if (Mflag == 0 && write(ofd, outbuf, obufp) < 0)
		error("obuf write error");
	obufp = 0;
}

void
putch(int ch)
{
	outbuf[obufp++] = ch;
	if (obufp == CPPBUF || (istty && ch == '\n'))
		flbuf();
}

void
putstr(usch *s)
{
	for (; *s; s++) {
		outbuf[obufp++] = *s;
		if (obufp == CPPBUF || (istty && *s == '\n'))
			flbuf();
	}
}

/*
 * convert a number to an ascii string. Store it on the heap.
 */
static void
num2str(int num)
{
	static usch buf[12];
	usch *b = buf;
	int m = 0;
	
	if (num < 0)
		num = -num, m = 1;
	do {
		*b++ = num % 10 + '0', num /= 10;
	} while (num);
	if (m)
		*b++ = '-';
	while (b > buf)
		savch(*--b);
}

/*
 * similar to sprintf, but only handles %s and %d. 
 * saves result on heap.
 */
usch *
sheap(char *fmt, ...)
{
	va_list ap;
	usch *op = stringbuf;

	va_start(ap, fmt);
	for (; *fmt; fmt++) {
		if (*fmt == '%') {
			fmt++;
			switch (*fmt) {
			case 's':
				savstr(va_arg(ap, usch *));
				break;
			case 'd':
				num2str(va_arg(ap, int));
				break;
			case 'c':
				savch(va_arg(ap, int));
				break;
			default:
				break; /* cannot call error() here */
			}
		} else
			savch(*fmt);
	}
	va_end(ap);
	*stringbuf = 0;
	return op;
}

void
usage()
{
	error("Usage: cpp [-Cdt] [-Dvar=val] [-Uvar] [-Ipath] [-Spath]");
}

#ifdef notyet
/*
 * Symbol table stuff.
 * The data structure used is a patricia tree implementation using only
 * bytes to store offsets.  
 * The information stored is (lower address to higher):
 *
 *	unsigned char bitno[2]; bit number in the string
 *	unsigned char left[3];	offset from base to left element
 *	unsigned char right[3];	offset from base to right element
 */
#endif

/*
 * This patricia implementation is more-or-less the same as
 * used in ccom for string matching.
 */
struct tree {
	int bitno;
	struct tree *lr[2];
};

#define BITNO(x)		((x) & ~(LEFT_IS_LEAF|RIGHT_IS_LEAF))
#define LEFT_IS_LEAF		0x80000000
#define RIGHT_IS_LEAF		0x40000000
#define IS_LEFT_LEAF(x)		(((x) & LEFT_IS_LEAF) != 0)
#define IS_RIGHT_LEAF(x)	(((x) & RIGHT_IS_LEAF) != 0)
#define P_BIT(key, bit)		(key[bit >> 3] >> (bit & 7)) & 1
#define CHECKBITS		8

static struct tree *sympole;
static int numsyms;

#define getree() malloc(sizeof(struct tree))

/*
 * Allocate a symtab struct and store the string.
 */
static struct symtab *
getsymtab(usch *str)
{
	struct symtab *sp = malloc(sizeof(struct symtab));

	sp->namep = savstr(str), savch('\0');
	sp->value = NULL;
	sp->file = ifiles ? ifiles->orgfn : (usch *)"<initial>";
	sp->line = ifiles ? ifiles->lineno : 0;
	return sp;
}

/*
 * Do symbol lookup in a patricia tree.
 * Only do full string matching, no pointer optimisations.
 */
struct symtab *
lookup(usch *key, int enterf)
{
	struct symtab *sp;
	struct tree *w, *new, *last;
	int len, cix, bit, fbit, svbit, ix, bitno;
	usch *k, *m, *sm;

	/* Count full string length */
	for (k = key, len = 0; *k; k++, len++)
		;

	switch (numsyms) {
	case 0: /* no symbols yet */
		if (enterf != ENTER)
			return NULL;
		sympole = (struct tree *)getsymtab(key);
		numsyms++;
		return (struct symtab *)sympole;

	case 1:
		w = sympole;
		svbit = 0; /* XXX gcc */
		break;

	default:
		w = sympole;
		bitno = len * CHECKBITS;
		for (;;) {
			bit = BITNO(w->bitno);
			fbit = bit > bitno ? 0 : P_BIT(key, bit);
			svbit = fbit ? IS_RIGHT_LEAF(w->bitno) :
			    IS_LEFT_LEAF(w->bitno);
			w = w->lr[fbit];
			if (svbit)
				break;
		}
	}

	sp = (struct symtab *)w;

	sm = m = sp->namep;
	k = key;

	/* Check for correct string and return */
	for (cix = 0; *m && *k && *m == *k; m++, k++, cix += CHECKBITS)
		;
	if (*m == 0 && *k == 0) {
		if (enterf != ENTER && sp->value == NULL)
			return NULL;
		return sp;
	}

	if (enterf != ENTER)
		return NULL; /* no string found and do not enter */

	ix = *m ^ *k;
	while ((ix & 1) == 0)
		ix >>= 1, cix++;

	/* Create new node */
	new = getree();
	bit = P_BIT(key, cix);
	new->bitno = cix | (bit ? RIGHT_IS_LEAF : LEFT_IS_LEAF);
	new->lr[bit] = (struct tree *)getsymtab(key);

	if (numsyms++ == 1) {
		new->lr[!bit] = sympole;
		new->bitno |= (bit ? LEFT_IS_LEAF : RIGHT_IS_LEAF);
		sympole = new;
		return (struct symtab *)new->lr[bit];
	}

	w = sympole;
	last = NULL;
	for (;;) {
		fbit = w->bitno;
		bitno = BITNO(w->bitno);
		if (bitno == cix)
			error("bitno == cix");
		if (bitno > cix)
			break;
		svbit = P_BIT(key, bitno);
		last = w;
		w = w->lr[svbit];
		if (fbit & (svbit ? RIGHT_IS_LEAF : LEFT_IS_LEAF))
			break;
	}

	new->lr[!bit] = w;
	if (last == NULL) {
		sympole = new;
	} else {
		last->lr[svbit] = new;
		last->bitno &= ~(svbit ? RIGHT_IS_LEAF : LEFT_IS_LEAF);
	}
	if (bitno < cix)
		new->bitno |= (bit ? LEFT_IS_LEAF : RIGHT_IS_LEAF);
	return (struct symtab *)new->lr[bit];
}

