/*
 * GSP assembler - definitions
 *
 * Copyright (c) 1993 Paul Mackerras.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Mackerras.
 * 4. The name of the author may not be used to endorse or promote products
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

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
/*#include <alloca.h>*/

#define MAXLINE		133

typedef char	bool;
#define TRUE	1
#define FALSE	0

#define YYDEBUG	1

/* Structure for symbol in symbol table */
typedef struct symbol {
	int16_t	flags;
	int16_t	ndefn;
	unsigned int	value;
	unsigned int	lineno;
	struct symbol	*next;
	struct numlab	*nlab;
	char		name[1];
} *symbol;

/* Values for flags */
#define DEFINED		1
#define SET_LABEL	2
#define NUMERIC_LABEL	4

#define NOT_YET		65535U		/* line no. for `not defined yet' */

/* Info about successive numeric labels */
struct numlab {
	unsigned int	value;
	unsigned int	lineno;
	struct numlab	*next;
};

/* Structure for expressions */
typedef struct expr {
	int	e_op;
	union {
		struct {
			struct expr *left;
			struct expr *right;
		} e_s;
		symbol	sym;
		int32_t	val;
	} e_u;
} *expr;
#define e_left	e_u.e_s.left
#define e_right	e_u.e_s.right
#define e_sym	e_u.sym
#define e_val	e_u.val

/* Operators other than '+', '-', etc. */
#define SYM	1
#define CONST	2
#define NEG	3

/* Structure for an operand */
typedef struct operand {
	char	type;
	char	mode;			/* EA mode */
	int16_t	reg_no;
	union {
		expr	value;
		char	*string;
	} op_u;
	struct operand	*next;
} *operand;

/* Values for operand type */
#define REG	1		/* register operand */
#define EXPR	2		/* expression operand */
#define EA	4		/* effective address */
#define STR_OPN	8		/* string operand */

/* Addressing modes */
/* NB codes for modes with an expression must be > other modes */
#define M_REG		0	/* R */
#define M_IND		1	/* *R */
#define M_POSTINC	2	/* *R+ */
#define M_PREDEC	3	/* *-R */
#define M_INDXY		4	/* *R.XY (pixt only) */
#define M_INDEX		5	/* *R(n) */
#define M_ABSOLUTE	6	/* @adr */

/* Register names */
#define A0	0x20
#define B0	0x50
#define SP	0x6F		/* (r1 & r2 & REGFILE) != 0 iff */
#define REGFILE	0x60		/* r1 and r2 are in the same file */

/* Prototypes */
void statement(char *opcode, operand operands);
void do_asg(char *, expr, int flags);
void set_label(char *);
void set_numeric_label(int);
void reset_numeric_labels(void);
operand reg_op(int reg);
operand expr_op(expr);
operand string_op(char *);
operand abs_adr(expr);
operand reg_ind(int, int);
operand reg_indxy(int, char *);
operand reg_index(int, expr);
operand add_operand(operand, operand);
expr id_expr(char *);
expr num_expr(int);
expr here_expr(void);
expr bexpr(int, expr, expr);
void free_operands(operand);
void free_expr(expr);

void perr(char *fmt, ...);
void p1err(char *fmt, ...);
int get_line(char *lp, int maxlen);
char *alloc(size_t nbytes);

symbol lookup(char *id, bool makeit);
expr fold(expr);
int eval_expr(expr, int32_t *, unsigned int *);
void pseudo(int code, operand operands);

void putcode(u_int16_t *, int);

extern unsigned int pc;
extern short pass2;

extern int lineno;
extern int err_count;
extern char line[], *lineptr;

#if defined(sparc) && !(defined(__NetBSD__) || defined(__OpenBSD__))
#include <alloca.h>
#else
#ifdef __GNUC__
#define alloca __builtin_alloca
#endif
#endif

#define ALLOC(x, type)	((x) = (type) alloc (sizeof(*(x))))
