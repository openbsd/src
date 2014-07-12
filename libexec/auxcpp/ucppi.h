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

#ifndef UCPP__UCPPI__
#define UCPP__UCPPI__

#include "tune.h"
#include "cpp.h"
#include "nhash.h"

/*
 * A macro represented in a compact form; simple tokens are represented
 * by one byte, containing their number. Tokens with a string value are
 * followed by the value (string finished by a 0). Macro arguments are
 * followed by the argument number (in one byte -- thus implying a hard
 * limit of 254 arguments (number 255 is for __VA_ARGS__).
 */
struct comp_token_fifo {
	size_t length;
	size_t rp;
	unsigned char *t;
};

/* These declarations are used only internally by ucpp */

/*
 * S_TOKEN(x)	checks whether x is a token type with an embedded string
 * ttMWS(x)	checks whether x is macro whitespace (space, comment...)
 * ttWHI(x)	checks whether x is whitespace (MWS or newline)
 */
#define S_TOKEN(x)	STRING_TOKEN(x)
#define ttMWS(x)	((x) == NONE || (x) == COMMENT || (x) == OPT_NONE)
#define ttWHI(x)	(ttMWS(x) || (x) == NEWLINE)

/*
 * Function prototypes
 */
/*
 * from lexer.c
 */
#define init_cppm	ucpp_init_cppm
#define put_char	ucpp_put_char
#define discard_char	ucpp_discard_char
#define next_token	ucpp_next_token
#define grap_char	ucpp_grap_char
#define space_char	ucpp_space_char

void init_cppm(void);
void put_char(struct lexer_state *, unsigned char);
void discard_char(struct lexer_state *);
int next_token(struct lexer_state *);
int grap_char(struct lexer_state *);
int space_char(int);

/*
 * from assert.c
 */
struct assert {
	hash_item_header head;    /* first field */
	size_t nbval;
	struct token_fifo *val;
};

#define cmp_token_list		ucpp_cmp_token_list
#define handle_assert		ucpp_handle_assert
#define handle_unassert		ucpp_handle_unassert
#define get_assertion		ucpp_get_assertion
#define wipe_assertions		ucpp_wipe_assertions

int cmp_token_list(struct token_fifo *, struct token_fifo *);
int handle_assert(struct lexer_state *);
int handle_unassert(struct lexer_state *);
struct assert *get_assertion(char *);
void wipe_assertions(void);

/*
 * from macro.c
 */
struct macro {
	hash_item_header head;     /* first field */
	int narg;
	char **arg;
	int nest;
	int vaarg;
#ifdef LOW_MEM
	struct comp_token_fifo cval;
#else
	struct token_fifo val;
#endif
};

#define print_token		ucpp_print_token
#define handle_define		ucpp_handle_define
#define handle_undef		ucpp_handle_undef
#define handle_ifdef		ucpp_handle_ifdef
#define handle_ifndef		ucpp_handle_ifndef
#define substitute_macro	ucpp_substitute_macro
#define get_macro		ucpp_get_macro
#define wipe_macros		ucpp_wipe_macros
#define dsharp_lexer		ucpp_dsharp_lexer
#define compile_time		ucpp_compile_time
#define compile_date		ucpp_compile_date
#ifdef PRAGMA_TOKENIZE
#define tokenize_lexer		ucpp_tokenize_lexer
#endif

void print_token(struct lexer_state *, struct token *, long);
int handle_define(struct lexer_state *);
int handle_undef(struct lexer_state *);
int handle_ifdef(struct lexer_state *);
int handle_ifndef(struct lexer_state *);
int substitute_macro(struct lexer_state *, struct macro *,
	struct token_fifo *, int, int, long);
struct macro *get_macro(char *);
void wipe_macros(void);

extern struct lexer_state dsharp_lexer;
extern char compile_time[], compile_date[];
#ifdef PRAGMA_TOKENIZE
extern struct lexer_state tokenize_lexer;
#endif

/*
 * from eval.c
 */
#define strtoconst	ucpp_strtoconst
#define eval_expr	ucpp_eval_expr
#define eval_line	ucpp_eval_line

unsigned long strtoconst(char *);
unsigned long eval_expr(struct token_fifo *, int *, int);
extern long eval_line;

#define eval_exception	ucpp_eval_exception

#ifdef POSIX_JMP
#define JMP_BUF	sigjmp_buf
#define catch(x)	sigsetjmp((x), 0)
#define throw(x)	siglongjmp((x), 1)
#else
#define JMP_BUF	jmp_buf
#define catch(x)	setjmp((x))
#define throw(x)	longjmp((x), 1)
#endif
extern JMP_BUF eval_exception;

/*
 * from cpp.c
 */
#define token_name		ucpp_token_name
#define throw_away		ucpp_throw_away
#define garbage_collect		ucpp_garbage_collect
#define init_buf_lexer_state	ucpp_init_buf_lexer_state
#ifdef PRAGMA_TOKENIZE
#define compress_token_list	ucpp_compress_token_list
#endif

char *token_name(struct token *);
void throw_away(struct garbage_fifo *, char *);
void garbage_collect(struct garbage_fifo *);
void init_buf_lexer_state(struct lexer_state *, int);
#ifdef PRAGMA_TOKENIZE
struct comp_token_fifo compress_token_list(struct token_fifo *);
#endif

#define ouch		ucpp_ouch
#define error		ucpp_error
#define warning		ucpp_warning

#endif
