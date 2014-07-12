/*
 * (c) Thomas Pornin 1999 - 2002
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. The name of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef UCPP__CPP__
#define UCPP__CPP__

/*
 * Uncomment the following if you want ucpp to use externally provided
 * error-reporting functions (ucpp_warning(), ucpp_error() and ucpp_ouch())
 */
/* #define NO_UCPP_ERROR_FUNCTIONS */

/*
 * Tokens (do not change the order unless checking operators_name[] in cpp.c)
 *
 * It is important that the token NONE is 0
 * Check the STRING_TOKEN macro
 */
#define CPPERR	512
enum {
	NONE,		/* whitespace */
	NEWLINE,	/* newline */
	COMMENT,	/* comment */
	NUMBER,		/* number constant */
	NAME,		/* identifier */
	BUNCH,		/* non-C characters */
	PRAGMA,		/* a #pragma directive */
	CONTEXT,	/* new file or #line */
	STRING,		/* constant "xxx" */
	CHAR,		/* constant 'xxx' */
	SLASH,		/*	/	*/
	ASSLASH,	/*	/=	*/
	MINUS,		/*	-	*/
	MMINUS,		/*	--	*/
	ASMINUS,	/*	-=	*/
	ARROW,		/*	->	*/
	PLUS,		/*	+	*/
	PPLUS,		/*	++	*/
	ASPLUS,		/*	+=	*/
	LT,		/*	<	*/
	LEQ,		/*	<=	*/
	LSH,		/*	<<	*/
	ASLSH,		/*	<<=	*/
	GT,		/*	>	*/
	GEQ,		/*	>=	*/
	RSH,		/*	>>	*/
	ASRSH,		/*	>>=	*/
	ASGN,		/*	=	*/
	SAME,		/*	==	*/
#ifdef CAST_OP
	CAST,		/*	=>	*/
#endif
	NOT,		/*	~	*/
	NEQ,		/*	!=	*/
	AND,		/*	&	*/
	LAND,		/*	&&	*/
	ASAND,		/*	&=	*/
	OR,		/*	|	*/
	LOR,		/*	||	*/
	ASOR,		/*	|=	*/
	PCT,		/*	%	*/
	ASPCT,		/*	%=	*/
	STAR,		/*	*	*/
	ASSTAR,		/*	*=	*/
	CIRC,		/*	^	*/
	ASCIRC,		/*	^=	*/
	LNOT,		/*	!	*/
	LBRA,		/*	{	*/
	RBRA,		/*	}	*/
	LBRK,		/*	[	*/
	RBRK,		/*	]	*/
	LPAR,		/*	(	*/
	RPAR,		/*	)	*/
	COMMA,		/*	,	*/
	QUEST,		/*	?	*/
	SEMIC,		/*	;	*/
	COLON,		/*	:	*/
	DOT,		/*	.	*/
	MDOTS,		/*	...	*/
	SHARP,		/*	#	*/
	DSHARP,		/*	##	*/

	OPT_NONE,	/* optional space to separate tokens in text output */

	DIGRAPH_TOKENS,			/* there begin digraph tokens */

	/* for DIG_*, do not change order, unless checking undig() in cpp.c */
	DIG_LBRK,	/*	<:	*/
	DIG_RBRK,	/*	:>	*/
	DIG_LBRA,	/*	<%	*/
	DIG_RBRA,	/*	%>	*/
	DIG_SHARP,	/*	%:	*/
	DIG_DSHARP,	/*	%:%:	*/

	DIGRAPH_TOKENS_END,		/* digraph tokens end here */

	LAST_MEANINGFUL_TOKEN,		/* reserved words will go there */

	MACROARG,	/* special token for representing macro arguments */

	UPLUS = CPPERR,	/* unary + */
	UMINUS		/* unary - */
};

#include "tune.h"
#include <stdio.h>
#include <setjmp.h>

struct token {
	int type;
	long line;
	char *name;
};

struct token_fifo {
	struct token *t;
	size_t nt, art;
};

struct lexer_state {
	/* input control */
	FILE *input;
#ifndef NO_UCPP_BUF
	unsigned char *input_buf;
#ifdef UCPP_MMAP
	int from_mmap;
	unsigned char *input_buf_sav;
#endif
#endif
	unsigned char *input_string;
	size_t ebuf;
	size_t pbuf;
	int lka[2];
	int nlka;
	int macfile;
	int last;
	int discard;
	unsigned long utf8;
	unsigned char copy_line[COPY_LINE_LENGTH];
	int cli;

	/* output control */
	FILE *output;
	struct token_fifo *output_fifo, *toplevel_of;
#ifndef NO_UCPP_BUF
	unsigned char *output_buf;
#endif
	size_t sbuf;

	/* token control */
	struct token *ctok;
	struct token *save_ctok;
	size_t tknl;
	int ltwnl;
	int pending_token;
#ifdef INMACRO_FLAG
	int inmacro;
	long macro_count;
#endif

	/* lexer options */
	long line;
	long oline;
	unsigned long flags;
	long count_trigraphs;
	struct garbage_fifo *gf;
	int ifnest;
	int condnest;
	int condcomp;
	int condmet;
	unsigned long condf[2];
};

/*
 * Flags for struct lexer_state
 */
/* warning flags */
#define WARN_STANDARD	     0x000001UL	/* emit standard warnings */
#define WARN_ANNOYING	     0x000002UL	/* emit annoying warnings */
#define WARN_TRIGRAPHS	     0x000004UL	/* warn when trigraphs are used */
#define WARN_TRIGRAPHS_MORE  0x000008UL	/* extra-warn for trigraphs */
#define WARN_PRAGMA	     0x000010UL	/* warn for pragmas in non-lexer mode */

/* error flags */
#define FAIL_SHARP	     0x000020UL	/* emit errors on rogue '#' */
#define CCHARSET	     0x000040UL	/* emit errors on non-C characters */

/* emission flags */
#define DISCARD_COMMENTS     0x000080UL	/* discard comments from text output */
#define CPLUSPLUS_COMMENTS   0x000100UL	/* understand C++-like comments */
#define LINE_NUM	     0x000200UL	/* emit #line directives in output */
#define GCC_LINE_NUM	     0x000400UL	/* same as #line, with gcc-syntax */

/* language flags */
#define HANDLE_ASSERTIONS    0x000800UL	/* understand assertions */
#define HANDLE_PRAGMA	     0x001000UL	/* emit PRAGMA tokens in lexer mode */
#define MACRO_VAARG	     0x002000UL	/* understand macros with '...' */
#define UTF8_SOURCE	     0x004000UL	/* identifiers are in UTF8 encoding */
#define HANDLE_TRIGRAPHS     0x008000UL	/* handle trigraphs */

/* global ucpp behaviour */
#define LEXER		     0x010000UL	/* behave as a lexer */
#define KEEP_OUTPUT	     0x020000UL	/* emit the result of preprocessing */
#define COPY_LINE	     0x040000UL /* make a copy of the parsed line */

/* internal flags */
#define READ_AGAIN	     0x080000UL	/* emit again the last token */
#define TEXT_OUTPUT	     0x100000UL	/* output text */

/*
 * Public function prototypes
 */

#ifndef NO_UCPP_BUF
void flush_output(struct lexer_state *);
#endif

void init_assertions(void);
int make_assertion(char *);
int destroy_assertion(char *);
void print_assertions(void);

void init_macros(void);
int define_macro(struct lexer_state *, char *);
int undef_macro(struct lexer_state *, char *);
void print_defines(void);

void set_init_filename(char *, int);
void init_cpp(void);
void init_include_path(char *[]);
void init_lexer_state(struct lexer_state *);
void init_lexer_mode(struct lexer_state *);
void free_lexer_state(struct lexer_state *);
void wipeout(void);
int lex(struct lexer_state *);
int check_cpp_errors(struct lexer_state *);
void add_incpath(char *);
void init_tables(int);
int enter_file(struct lexer_state *, unsigned long);
int cpp(struct lexer_state *);
void set_identifier_char(int c);
void unset_identifier_char(int c);

#ifdef UCPP_MMAP
FILE *fopen_mmap_file(char *);
void set_input_file(struct lexer_state *, FILE *);
#endif

struct stack_context {
	char *long_name, *name;
	long line;
};
struct stack_context *report_context(void);

extern int no_special_macros, system_macros,
	emit_dependencies, emit_defines, emit_assertions;
extern int c99_compliant, c99_hosted;
extern FILE *emit_output;
extern char *current_filename, *current_long_filename;
extern char *operators_name[];
extern struct protect {
	char *macro;
	int state;
	struct found_file *ff;
} protect_detect;

void ucpp_ouch(char *, ...);
void ucpp_error(long, char *, ...);
void ucpp_warning(long, char *, ...);

extern int *transient_characters;

/*
 * Errors from CPPERR_EOF and above are not real erros, only show-stoppers.
 * Errors below CPPERR_EOF are real ones.
 */
#define CPPERR_NEST	 900
#define CPPERR_EOF	1000

/*
 * This macro tells whether the name field of a given token type is
 * relevant, or not. Irrelevant name field means that it might point
 * to outerspace.
 */
#ifdef SEMPER_FIDELIS
#define STRING_TOKEN(x)    ((x) == NONE || ((x) >= COMMENT && (x) <= CHAR))
#else
#define STRING_TOKEN(x)    ((x) >= NUMBER && (x) <= CHAR)
#endif

#endif
