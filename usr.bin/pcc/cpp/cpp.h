/*	$OpenBSD: cpp.h,v 1.4 2007/10/12 21:40:49 stefan Exp $	*/

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

#include <stdio.h> /* for obuf */

#include "../config.h"

typedef unsigned char usch;
#ifdef YYTEXT_POINTER
extern char *yytext;
#else
extern char yytext[];
#endif
extern usch *stringbuf;

extern	int	trulvl;
extern	int	flslvl;
extern	int	elflvl;
extern	int	elslvl;
extern	int	tflag, Cflag;
extern	int	Mflag, dMflag;
extern	usch	*Mfile;
extern	int	ofd;

/* args for lookup() */
#define FIND    0
#define ENTER   1

/* buffer used internally */
#ifndef CPPBUF
#ifdef __pdp11__
#define CPPBUF  BUFSIZ
#else
#define CPPBUF	65536
#endif
#endif

#define	NAMEMAX	64 /* max len of identifier */

/* definition for include file info */
struct includ {
	struct includ *next;
	usch *fname;	/* current fn, changed if #line found */
	usch *orgfn;	/* current fn, not changed */
	int lineno;
	int infil;
	usch *curptr;
	usch *maxread;
	usch *buffer;
	usch bbuf[NAMEMAX+CPPBUF+1];
} *ifiles;

/* Symbol table entry  */
struct symtab {
	usch *namep;    
	usch *value;    
	usch *file;
	int line;
};

struct initar {
	struct initar *next;
	int type;
	char *str;
};

struct val {
	union {
		long long val;
		unsigned long long uval;
	} v;
	int type;
};

struct nd {
	union {
		struct {
			struct nd *left;
			struct nd *right;
		} t;
		struct val v;
	} n;
	int op;
};

#define nd_left n.t.left
#define nd_right n.t.right
#define nd_val n.v.v.val
#define nd_uval n.v.v.uval
#define nd_type n.v.type

struct nd *mknode(int, struct nd *, struct nd *);
struct nd *mknum(struct val);

struct recur;	/* not used outside cpp.c */
int subst(struct symtab *, struct recur *);
struct symtab *lookup(usch *namep, int enterf);
usch *gotident(struct symtab *nl);
int slow;	/* scan slowly for new tokens */

int pushfile(usch *fname);
void popfile(void);
void prtline(void);
int yylex(void);
void cunput(int);
int curline(void);
char *curfile(void);
void setline(int);
void setfile(char *);
int yyparse(void);
void yyerror(char *);
void unpstr(usch *);
usch *savstr(usch *str);
void savch(int c);
void mainscan(void);
void putch(int);
void putstr(usch *s);
void line(void);
usch *sheap(char *fmt, ...);
void xwarning(usch *);
void xerror(usch *);
#define warning(...) xwarning(sheap(__VA_ARGS__))
#define error(...) xerror(sheap(__VA_ARGS__))
void expmac(struct recur *);
