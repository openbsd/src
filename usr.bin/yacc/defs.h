/*	$OpenBSD: defs.h,v 1.20 2020/05/24 17:31:54 espie Exp $	*/
/*	$NetBSD: defs.h,v 1.6 1996/03/19 03:21:30 jtc Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Paul Corbett.
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
 *
 *	@(#)defs.h	5.6 (Berkeley) 5/24/93
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*  machine-dependent definitions			*/
/*  the following definitions are for the Tahoe		*/
/*  they might have to be changed for other machines	*/

/*  MAXCHAR is the largest unsigned character value	*/
/*  MAXSHORT is the largest value of a C short		*/
/*  MINSHORT is the most negative value of a C short	*/
/*  MAXTABLE is the maximum table size			*/
/*  BITS_PER_WORD is the number of bits in a C unsigned	*/
/*  WORDSIZE computes the number of words needed to	*/
/*	store n bits					*/
/*  BIT returns the value of the n-th bit starting	*/
/*	from r (0-indexed)				*/
/*  SETBIT sets the n-th bit starting from r		*/

#define	MAXCHAR		255
#define	MAXSHORT	32767
#define MINSHORT	-32768
#define MAXTABLE	32500
#define BITS_PER_WORD	32
#define	WORDSIZE(n)	(((n)+(BITS_PER_WORD-1))/BITS_PER_WORD)
#define	BIT(r, n)	((((r)[(n)>>5])>>((n)&31))&1)
#define	SETBIT(r, n)	((r)[(n)>>5]|=((unsigned)1<<((n)&31)))


/*  character names  */

#define	NUL		'\0'    /*  the null character  */
#define	NEWLINE		'\n'    /*  line feed  */
#define	SP		' '     /*  space  */
#define	BS		'\b'    /*  backspace  */
#define	HT		'\t'    /*  horizontal tab  */
#define	VT		'\013'  /*  vertical tab  */
#define	CR		'\r'    /*  carriage return  */
#define	FF		'\f'    /*  form feed  */
#define	QUOTE		'\''    /*  single quote  */
#define	DOUBLE_QUOTE	'\"'    /*  double quote  */
#define	BACKSLASH	'\\'    /*  backslash  */


/* defines for constructing filenames */

#define CODE_SUFFIX	".code.c"
#define	DEFINES_SUFFIX	".tab.h"
#define	OUTPUT_SUFFIX	".tab.c"
#define	VERBOSE_SUFFIX	".output"


/* keyword codes */

#define TOKEN 0
#define LEFT 1
#define RIGHT 2
#define NONASSOC 3
#define MARK 4
#define TEXT 5
#define TYPE 6
#define START 7
#define UNION 8
#define IDENT 9
#define EXPECT 10


/*  symbol classes  */

#define UNKNOWN 0
#define TERM 1
#define NONTERM 2


/*  the undefined value  */

#define UNDEFINED (-1)


/*  action codes  */

#define SHIFT 1
#define REDUCE 2


/*  character macros  */

#define IS_IDENT(c)	(isalnum(c) || (c) == '_' || (c) == '.' || (c) == '$')
#define	NUMERIC_VALUE(c)	((c) - '0')


/*  symbol macros  */

#define ISTOKEN(s)	((s) < start_symbol)
#define ISVAR(s)	((s) >= start_symbol)


/*  storage allocation macros  */

#define	NEW(t)		((t*)allocate(sizeof(t)))
#define	NEW2(n,t)	((t*)allocate((n)*sizeof(t)))


/*  the structure of a symbol table entry  */

typedef struct bucket bucket;
struct bucket {
	struct bucket *link;
	struct bucket *next;
	char *name;
	char *tag;
	short value;
	short index;
	short prec;
	char class;
	char assoc;
};


/*  the structure of the LR(0) state machine  */

typedef struct core core;
struct core {
	struct core *next;
	struct core *link;
	short number;
	short accessing_symbol;
	short nitems;
	short items[1];
};


/*  the structure used to record shifts  */

typedef struct shifts shifts;
struct shifts {
	struct shifts *next;
	short number;
	short nshifts;
	short shift[1];
};


/*  the structure used to store reductions  */

typedef struct reductions reductions;
struct reductions {
	struct reductions *next;
	short number;
	short nreds;
	short rules[1];
};


/*  the structure used to represent parser actions  */

typedef struct action action;
struct action {
	struct action *next;
	short symbol;
	short number;
	short prec;
	char action_code;
	char assoc;
	char suppressed;
};


/* global variables */

extern char dflag;
extern char lflag;
extern char rflag;
extern char tflag;
extern char vflag;
extern char *symbol_prefix;

extern char *cptr;
extern char *line;
extern int lineno;
extern int outline;

extern char *banner[];
extern char *tables[];
extern char *header[];
extern char *body[];
extern char *trailer[];

extern char *code_file_name;
extern char *defines_file_name;
extern char *input_file_name;
extern char *output_file_name;
extern char *verbose_file_name;

extern FILE *action_file;
extern FILE *code_file;
extern FILE *defines_file;
extern FILE *input_file;
extern FILE *output_file;
extern FILE *text_file;
extern FILE *union_file;
extern FILE *verbose_file;

extern int nitems;
extern int nrules;
extern int nsyms;
extern int ntokens;
extern int nvars;
extern int ntags;

extern char unionized;
extern char line_format[];

extern int   start_symbol;
extern char  **symbol_name;
extern short *symbol_value;
extern short *symbol_prec;
extern char  *symbol_assoc;

extern short *ritem;
extern short *rlhs;
extern short *rrhs;
extern short *rprec;
extern char  *rassoc;

extern short **derives;
extern char *nullable;

extern bucket *first_symbol;
extern bucket *last_symbol;

extern int nstates;
extern core *first_state;
extern shifts *first_shift;
extern reductions *first_reduction;
extern short *accessing_symbol;
extern core **state_table;
extern shifts **shift_table;
extern reductions **reduction_table;
extern unsigned *LA;
extern short *LAruleno;
extern short *lookaheads;
extern short *goto_map;
extern short *from_state;
extern short *to_state;

extern action **parser;
extern int SRtotal;
extern int SRexpect;
extern int RRtotal;
extern short *SRconflicts;
extern short *RRconflicts;
extern short *defred;
extern short *rules_used;
extern short nunused;
extern short final_state;

/* global functions */

extern void *allocate(size_t);
extern bucket *lookup(char *);
extern bucket *make_bucket(char *);
extern void set_first_derives(void);
extern void closure(short *, int);
extern void finalize_closure(void);

extern __dead void fatal(char *);

extern void reflexive_transitive_closure(unsigned *, int);

extern __dead void no_space(void);
extern __dead void open_error(char *);
extern __dead void tempfile_error(void);
extern __dead void open_write_error(char *);
extern __dead void unexpected_EOF(void);
extern void print_pos(char *, char *);
extern __dead void syntax_error(int, char *, char *);
extern __dead void unterminated_comment(int, char *, char *);
extern __dead void unterminated_string(int, char *, char *);
extern __dead void unterminated_text(int, char *, char *);
extern __dead void unterminated_union(int, char *, char *);
extern __dead void over_unionized(char *);
extern __dead void illegal_tag(int, char *, char *);
extern __dead void illegal_character(char *);
extern __dead void used_reserved(char *);
extern __dead void tokenized_start(char *);
extern void retyped_warning(char *);
extern void reprec_warning(char *);
extern void revalued_warning(char *);
extern __dead void terminal_start(char *);
extern void restarted_warning(void);
extern __dead void no_grammar(void);
extern __dead void terminal_lhs(int);
extern void prec_redeclared(void);
extern __dead void unterminated_action(int, char *, char *);
extern void dollar_warning(int, int);
extern __dead void dollar_error(int, char *, char *);
extern __dead void untyped_lhs(void);
extern __dead void untyped_rhs(int, char *);
extern __dead void unknown_rhs(int);
extern void default_action_warning(void);
extern __dead void undefined_goal(char *);
extern void undefined_symbol_warning(char *);

extern void lalr(void);

extern void reader(void);
extern void lr0(void);
extern void free_nullable(void);
extern void free_derives(void);
extern void make_parser(void);
extern void verbose(void);
extern void output(void);
extern void free_parser(void);
extern void write_section(char *[]);

extern void create_symbol_table(void);
extern void free_symbol_table(void);
extern void free_symbols(void);


/* system variables */

extern char *__progname;
