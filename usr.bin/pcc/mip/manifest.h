/*	$Id: manifest.h,v 1.1.1.1 2007/09/15 18:12:35 otto Exp $	*/
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
 * notice, this list of conditionsand the following disclaimer in the
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

#ifndef MANIFEST
#define	MANIFEST

#include <stdio.h>
#include "../config.h"
#include "macdefs.h"
#include "node.h"

/*
 * Node types
 */
#define LTYPE	02		/* leaf */
#define UTYPE	04		/* unary */
#define BITYPE	010		/* binary */

/*
 * DSIZE is the size of the dope array
 */
#define DSIZE	(MAXOP+1)

/*
 * Type names, used in symbol table building.
 * The order of the integer types are important.
 * Signed types must have bit 0 unset, unsigned types set (used below).
 */
#define	UNDEF		0	/* free symbol table entry */
#define	FARG		1 	/* function argument */
#define	CHAR		2
#define	UCHAR		3
#define	SHORT		4
#define	USHORT		5
#define	INT		6
#define	UNSIGNED	7
#define	LONG		8
#define	ULONG		9      
#define	LONGLONG	10
#define	ULONGLONG	11
#define	FLOAT		12
#define	DOUBLE		13
#define	LDOUBLE		14
#define	STRTY		15
#define	UNIONTY		16
#define	ENUMTY		17
#define	MOETY		18	/* member of enum */
#define	VOID		19

#define	MAXTYPES	19	/* highest type+1 to be used by lang code */
/*
 * Various flags
 */
#define NOLAB	(-1)

/* 
 * Type modifiers.
 */
#define	PTR		0x20
#define	FTN		0x40
#define	ARY		0x60
#define	CON		0x20
#define	VOL		0x40

/*
 * Type packing constants
 */
#define TMASK	0x060
#define TMASK1	0x180
#define TMASK2	0x1e0
#define BTMASK	0x1f
#define BTSHIFT	5
#define TSHIFT	2

/*
 * Macros
 */
#define MODTYPE(x,y)	x = ((x)&(~BTMASK))|(y)	/* set basic type of x to y */
#define BTYPE(x)	((x)&BTMASK)		/* basic type of x */
#define	ISLONGLONG(x)	((x) == LONGLONG || (x) == ULONGLONG)
#define ISUNSIGNED(x)	(((x) <= ULONGLONG) && (((x) & 1) == (UNSIGNED & 1)))
#define UNSIGNABLE(x)	(((x)<=ULONGLONG&&(x)>=CHAR) && !ISUNSIGNED(x))
#define ENUNSIGN(x)	((x)|1)
#define DEUNSIGN(x)	((x)&~1)
#define ISPTR(x)	(((x)&TMASK)==PTR)
#define ISFTN(x)	(((x)&TMASK)==FTN)	/* is x a function type? */
#define ISARY(x)	(((x)&TMASK)==ARY)	/* is x an array type? */
#define	ISCON(x)	(((x)&CON)==CON)	/* is x const? */
#define	ISVOL(x)	(((x)&VOL)==VOL)	/* is x volatile? */
#define INCREF(x)	((((x)&~BTMASK)<<TSHIFT)|PTR|((x)&BTMASK))
#define INCQAL(x)	((((x)&~BTMASK)<<TSHIFT)|((x)&BTMASK))
#define DECREF(x)	((((x)>>TSHIFT)&~BTMASK)|((x)&BTMASK))
#define DECQAL(x)	((((x)>>TSHIFT)&~BTMASK)|((x)&BTMASK))
#define SETOFF(x,y)	{ if ((x)%(y) != 0) (x) = (((x)/(y) + 1) * (y)); }
		/* advance x to a multiple of y */
#define NOFIT(x,y,z)	(((x)%(z) + (y)) > (z))
		/* can y bits be added to x without overflowing z */

#ifndef SPECIAL_INTEGERS
#define	ASGLVAL(lval, val)
#endif

/*
 * Pack and unpack field descriptors (size and offset)
 */
#define PKFIELD(s,o)	(((o)<<6)| (s))
#define UPKFSZ(v)	((v)&077)
#define UPKFOFF(v)	((v)>>6)

/*
 * Operator information
 */
#define TYFLG	016
#define ASGFLG	01
#define LOGFLG	020

#define SIMPFLG	040
#define COMMFLG	0100
#define DIVFLG	0200
#define FLOFLG	0400
#define LTYFLG	01000
#define CALLFLG	02000
#define MULFLG	04000
#define SHFFLG	010000
#define ASGOPFLG 020000

#define SPFLG	040000

/*
 * Location counters
 */
#define PROG		0		/* (ro) program segment */
#define DATA		1		/* (rw) data segment */
#define RDATA		2		/* (ro) data segment */
#define STRNG		3		/* (ro) string segment */


/*
 * 
 */
extern int bdebug, tdebug, edebug;
extern int ddebug, xdebug, f2debug;
extern int iTflag, oTflag;
extern int vdebug, sflag, nflag, gflag;
extern int Wstrict_prototypes, Wmissing_prototypes, Wimplicit_int,
	Wimplicit_function_declaration;
extern int xssaflag, xtailcallflag, xtemps, xdeljumps;

int yyparse(void);
void yyaccpt(void);

/*
 * List handling macros, similar to those in 4.4BSD.
 * The double-linked list is insque-style.
 */
/* Double-linked list macros */
#define	DLIST_INIT(h,f)		{ (h)->f.q_forw = (h); (h)->f.q_back = (h); }
#define	DLIST_ENTRY(t)		struct { struct t *q_forw, *q_back; }
#define	DLIST_NEXT(h,f)		(h)->f.q_forw
#define	DLIST_PREV(h,f)		(h)->f.q_back
#define DLIST_ISEMPTY(h,f)	((h)->f.q_forw == (h))
#define	DLIST_FOREACH(v,h,f) \
	for ((v) = (h)->f.q_forw; (v) != (h); (v) = (v)->f.q_forw)
#define	DLIST_FOREACH_REVERSE(v,h,f) \
	for ((v) = (h)->f.q_back; (v) != (h); (v) = (v)->f.q_back)
#define	DLIST_INSERT_BEFORE(h,e,f) {	\
	(e)->f.q_forw = (h);		\
	(e)->f.q_back = (h)->f.q_back;	\
	(e)->f.q_back->f.q_forw = (e);	\
	(h)->f.q_back = (e);		\
}
#define	DLIST_INSERT_AFTER(h,e,f) {	\
	(e)->f.q_forw = (h)->f.q_forw;	\
	(e)->f.q_back = (h);		\
	(e)->f.q_forw->f.q_back = (e);	\
	(h)->f.q_forw = (e);		\
}
#define DLIST_REMOVE(e,f) {			 \
	(e)->f.q_forw->f.q_back = (e)->f.q_back; \
	(e)->f.q_back->f.q_forw = (e)->f.q_forw; \
}

/* Single-linked list */
#define	SLIST_INIT(h)	\
	{ (h)->q_forw = NULL; (h)->q_last = &(h)->q_forw; }
#define	SLIST_ENTRY(t)	struct { struct t *q_forw; }
#define	SLIST_HEAD(n,t) struct n { struct t *q_forw, **q_last; }
#define	SLIST_FIRST(h)	((h)->q_forw)
#define	SLIST_FOREACH(v,h,f) \
	for ((v) = (h)->q_forw; (v) != NULL; (v) = (v)->f.q_forw)
#define	SLIST_INSERT_LAST(h,e,f) {	\
	(e)->f.q_forw = NULL;		\
	*(h)->q_last = (e);		\
	(h)->q_last = &(e)->f.q_forw;	\
}

/*
 * Functions for inter-pass communication.
 *
 */
struct interpass {
	DLIST_ENTRY(interpass) qelem;
	int type;
	int lineno;
	union {
		NODE *_p;
		int _locctr;
		int _label;
		int _curoff;
		char *_name;
	} _un;
};

/*
 * Special struct for prologue/epilogue.
 * - ip_lblnum contains the lowest/highest+1 label used
 * - ip_lbl is set before/after all code and after/before the prolog/epilog.
 */
struct interpass_prolog {
	struct interpass ipp_ip;
	char *ipp_name;		/* Function name */
	int ipp_vis;		/* Function visibility */
	TWORD ipp_type;		/* Function type */
	int ipp_regs;		/* Bitmask of registers to save */
	int ipp_autos;		/* Size on stack needed */
	int ip_tmpnum;		/* # allocated temp nodes so far */
	int ip_lblnum;		/* # used labels so far */
};

/*
 * Epilog/prolog takes following arguments (in order):
 * - type
 * - regs
 * - autos
 * - name
 * - type
 * - retlab
 */

#define	ip_node	_un._p
#define	ip_locc	_un._locctr
#define	ip_lbl	_un._label
#define	ip_name	_un._name
#define	ip_asm	_un._name
#define	ip_off	_un._curoff

/* Types of inter-pass structs */
#define	IP_NODE		1
#define	IP_PROLOG	2
#define	IP_EPILOG	4
#define	IP_DEFLAB	5
#define	IP_DEFNAM	6
#define	IP_ASM		7
#define	MAXIP		7

void send_passt(int type, ...);
/*
 * External declarations, typedefs and the like
 */
char	*hash(char *s);
char	*savestr(char *cp);
char	*tstr(char *cp);

/* memory management stuff */
void *permalloc(int size);
void *tmpcalloc(int size);
void *tmpalloc(int size);
void tmpfree(void);
char *newstring(char *, int len);

void tprint(FILE *, TWORD, TWORD);

/* pass t communication subroutines */
void topt_compile(struct interpass *);

/* pass 2 communication subroutines */
void pass2_compile(struct interpass *);

/* node routines */
NODE *nfree(NODE *);
void fwalk(NODE *t, void (*f)(NODE *, int, int *, int *), int down);

extern	int nerrors;		/* number of errors seen so far */
#endif
