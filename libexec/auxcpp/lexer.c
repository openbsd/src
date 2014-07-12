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

#include "tune.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>
#include "ucppi.h"
#include "mem.h"
#ifdef UCPP_MMAP
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#endif

/*
 * Character classes for description of the automaton.
 * The characters used for representing classes should not appear
 * explicitely in an automaton rule.
 */
#define SPC	' '	/* whitespace characters */
#define ALP	'Z'	/* A-Z, a-z, _ */
#define NUM	'9'	/* 0-9 */
#define ANY	'Y'	/* any character */
#define VCH	'F'	/* void character (for end of input) */

/*
 * flags and macros to test those flags
 * STO: the currently read string is a complete token
 * PUT: the currently read character must be added to the string
 * FRZ: the currently read character must be kept and read again
 */
#define MOD_MK		255
#define noMOD(x)	((x) & 255)
#define STO(x)		((x) | 256)
#define ttSTO(x)	((x) & 256)
#define FRZ(x)		((x) | 512)
#define ttFRZ(x)	((x) & 512)
#define PUT(x)		((x) | 1024)
#define ttPUT(x)	((x) & 1024)

/* order is important */
enum {
	S_START, S_SPACE, S_BANG, S_STRING, S_STRING2, S_COLON,
	S_SHARP, S_PCT, S_PCT2, S_PCT3, S_AMPER, S_CHAR, S_CHAR2, S_STAR,
	S_PLUS, S_MINUS, S_DOT, S_DOT2, S_SLASH, S_NUMBER, S_NUMBER2, S_LT,
	S_LT2, S_EQ, S_GT, S_GT2, S_CIRC, S_PIPE, S_BACKSLASH,
	S_COMMENT, S_COMMENT2, S_COMMENT3, S_COMMENT4, S_COMMENT5,
	S_NAME, S_NAME_BS, S_LCHAR,
	MSTATE,
	S_ILL, S_DDOT, S_DDSHARP, S_BS, S_ROGUE_BS, S_BEHEAD, S_DECAY,
	S_TRUNC, S_TRUNCC, S_OUCH
};

#define CMT(x)		((x) >= S_COMMENT && (x) <= S_COMMENT5)

#define CMCR	2

/*
 * This is the description of the automaton. It is not used "as is"
 * but copied at execution time into a table.
 *
 * To my utmost displeasure, there are a few hacks in read_token()
 * (which uses the transformed automaton) about the special handling
 * of slashes, sharps, and the letter L.
 */
static struct machine_state {
	int state;
	unsigned char input[CMCR];
	int new_state;
} cppms[] = {
	/* S_START is the generic beginning state */
	{ S_START,	{ ANY },	S_ILL			},
#ifdef SEMPER_FIDELIS
	{ S_START,	{ SPC },	PUT(S_SPACE)		},
#else
	{ S_START,	{ SPC },	S_SPACE			},
#endif
	{ S_START,	{ '\n' },	STO(NEWLINE)		},
	{ S_START,	{ '!' },	S_BANG			},
	{ S_START,	{ '"' },	PUT(S_STRING)		},
	{ S_START,	{ '#' },	S_SHARP			},
	{ S_START,	{ '%' },	S_PCT			},
	{ S_START,	{ '&' },	S_AMPER			},
	{ S_START,	{ '\'' },	PUT(S_CHAR)		},
	{ S_START,	{ '(' },	STO(LPAR)		},
	{ S_START,	{ ')' },	STO(RPAR)		},
	{ S_START,	{ '*' },	S_STAR			},
	{ S_START,	{ '+' },	S_PLUS			},
	{ S_START,	{ ',' },	STO(COMMA)		},
	{ S_START,	{ '-' },	S_MINUS			},
	{ S_START,	{ '.' },	PUT(S_DOT)		},
#ifdef SEMPER_FIDELIS
	{ S_START,	{ '/' },	PUT(S_SLASH)		},
#else
	{ S_START,	{ '/' },	S_SLASH			},
#endif
	{ S_START,	{ NUM },	PUT(S_NUMBER)		},
	{ S_START,	{ ':' },	S_COLON			},
	{ S_START,	{ ';' },	STO(SEMIC)		},
	{ S_START,	{ '<' },	S_LT			},
	{ S_START,	{ '=' },	S_EQ			},
	{ S_START,	{ '>' },	S_GT			},
	{ S_START,	{ '?' },	STO(QUEST)		},
	{ S_START,	{ ALP },	PUT(S_NAME)		},
	{ S_START,	{ 'L' },	PUT(S_LCHAR)		},
	{ S_START,	{ '[' },	STO(LBRK)		},
	{ S_START,	{ ']' },	STO(RBRK)		},
	{ S_START,	{ '^' },	S_CIRC			},
	{ S_START,	{ '{' },	STO(LBRA)		},
	{ S_START,	{ '|' },	S_PIPE			},
	{ S_START,	{ '}' },	STO(RBRA)		},
	{ S_START,	{ '~' },	STO(NOT)		},
	{ S_START,	{ '\\' },	S_BACKSLASH		},

	/* after a space */
	{ S_SPACE,	{ ANY },	FRZ(STO(NONE))		},
#ifdef SEMPER_FIDELIS
	{ S_SPACE,	{ SPC },	PUT(S_SPACE)		},
#else
	{ S_SPACE,	{ SPC },	S_SPACE			},
#endif

	/* after a ! */
	{ S_BANG,	{ ANY },	FRZ(STO(LNOT))		},
	{ S_BANG,	{ '=' },	STO(NEQ)		},

	/* after a " */
	{ S_STRING,	{ ANY },	PUT(S_STRING)		},
	{ S_STRING,	{ VCH },	FRZ(S_TRUNC)		},
	{ S_STRING,	{ '\n' },	FRZ(S_BEHEAD)		},
	{ S_STRING,	{ '\\' },	PUT(S_STRING2)		},
	{ S_STRING,	{ '"' },	PUT(STO(STRING))	},

	{ S_STRING2,	{ ANY },	PUT(S_STRING)		},
	{ S_STRING2,	{ VCH },	FRZ(S_TRUNC)		},

	/* after a # */
	{ S_SHARP,	{ ANY },	FRZ(STO(SHARP))		},
	{ S_SHARP,	{ '#' },	STO(DSHARP)		},

	/* after a : */
	{ S_COLON,	{ ANY },	FRZ(STO(COLON))		},
	{ S_COLON,	{ '>' },	STO(DIG_RBRK)		},

	/* after a % */
	{ S_PCT,	{ ANY },	FRZ(STO(PCT))		},
	{ S_PCT,	{ '=' },	STO(ASPCT)		},
	{ S_PCT,	{ '>' },	STO(DIG_RBRA)		},
	{ S_PCT,	{ ':' },	S_PCT2			},

	/* after a %: */
	{ S_PCT2,	{ ANY },	FRZ(STO(DIG_SHARP))	},
	{ S_PCT2,	{ '%' },	S_PCT3			},

	/* after a %:% */
	{ S_PCT3,	{ ANY },	FRZ(S_DDSHARP)		},
	{ S_PCT3,	{ ':' },	STO(DIG_DSHARP)		},

	/* after a & */
	{ S_AMPER,	{ ANY },	FRZ(STO(AND))		},
	{ S_AMPER,	{ '=' },	STO(ASAND)		},
	{ S_AMPER,	{ '&' },	STO(LAND)		},

	/* after a ' */
	{ S_CHAR,	{ ANY },	PUT(S_CHAR)		},
	{ S_CHAR,	{ VCH },	FRZ(S_TRUNC)		},
	{ S_CHAR,	{ '\'' },	PUT(STO(CHAR))		},
	{ S_CHAR,	{ '\\' },	PUT(S_CHAR2)		},

	/* after a \ in a character constant
	   useful only for '\'' */
	{ S_CHAR2,	{ ANY },	PUT(S_CHAR)		},
	{ S_CHAR2,	{ VCH },	FRZ(S_TRUNC)		},

	/* after a * */
	{ S_STAR,	{ ANY },	FRZ(STO(STAR))		},
	{ S_STAR,	{ '=' },	STO(ASSTAR)		},

	/* after a + */
	{ S_PLUS,	{ ANY },	FRZ(STO(PLUS))		},
	{ S_PLUS,	{ '+' },	STO(PPLUS)		},
	{ S_PLUS,	{ '=' },	STO(ASPLUS)		},

	/* after a - */
	{ S_MINUS,	{ ANY },	FRZ(STO(MINUS))		},
	{ S_MINUS,	{ '-' },	STO(MMINUS)		},
	{ S_MINUS,	{ '=' },	STO(ASMINUS)		},
	{ S_MINUS,	{ '>' },	STO(ARROW)		},

	/* after a . */
	{ S_DOT,	{ ANY },	FRZ(STO(DOT))		},
	{ S_DOT,	{ NUM },	PUT(S_NUMBER)		},
	{ S_DOT,	{ '.' },	S_DOT2			},

	/* after .. */
	{ S_DOT2,	{ ANY },	FRZ(S_DDOT)		},
	{ S_DOT2,	{ '.' },	STO(MDOTS)		},

	/* after a / */
	{ S_SLASH,	{ ANY },	FRZ(STO(SLASH))		},
	{ S_SLASH,	{ '=' },	STO(ASSLASH)		},
#ifdef SEMPER_FIDELIS
	{ S_SLASH,	{ '*' },	PUT(S_COMMENT)		},
	{ S_SLASH,	{ '/' },	PUT(S_COMMENT5)		},
#else
	{ S_SLASH,	{ '*' },	S_COMMENT		},
	{ S_SLASH,	{ '/' },	S_COMMENT5		},
#endif
	/*
	 * There is a little hack in read_token() to disable
	 * this last rule, if C++ (C99) comments are not enabled.
	 */

	/* after a number */
	{ S_NUMBER,	{ ANY },	FRZ(STO(NUMBER))	},
	{ S_NUMBER,	{ ALP, NUM },	PUT(S_NUMBER)		},
	{ S_NUMBER,	{ '.' },	PUT(S_NUMBER)		},
	{ S_NUMBER,	{ 'E', 'e' },	PUT(S_NUMBER2)		},
	{ S_NUMBER,	{ 'P', 'p' },	PUT(S_NUMBER2)		},

	{ S_NUMBER2,	{ ANY },	FRZ(STO(NUMBER))	},
	{ S_NUMBER2,	{ ALP, NUM },	PUT(S_NUMBER)		},
	{ S_NUMBER2,	{ '+', '-' },	PUT(S_NUMBER)		},

	/* after a < */
	{ S_LT,		{ ANY },	FRZ(STO(LT))		},
	{ S_LT,		{ '=' },	STO(LEQ)		},
	{ S_LT,		{ '<' },	S_LT2			},
	{ S_LT,		{ ':' },	STO(DIG_LBRK)		},
	{ S_LT,		{ '%' },	STO(DIG_LBRA)		},

	{ S_LT2,	{ ANY },	FRZ(STO(LSH))		},
	{ S_LT2,	{ '=' },	STO(ASLSH)		},

	/* after a > */
	{ S_GT,		{ ANY },	FRZ(STO(GT))		},
	{ S_GT,		{ '=' },	STO(GEQ)		},
	{ S_GT,		{ '>' },	S_GT2			},

	{ S_GT2,	{ ANY },	FRZ(STO(RSH))		},
	{ S_GT2,	{ '=' },	STO(ASRSH)		},

	/* after a = */
	{ S_EQ,		{ ANY },	FRZ(STO(ASGN))		},
	{ S_EQ,		{ '=' },	STO(SAME)		},
#ifdef CAST_OP
	{ S_EQ,		{ '>' },	STO(CAST)		},
#endif

	/* after a \ */
	{ S_BACKSLASH,	{ ANY },	FRZ(S_BS)		},
	{ S_BACKSLASH,	{ 'U', 'u' },	FRZ(S_NAME_BS)		},

	/* after a letter */
	{ S_NAME,	{ ANY },	FRZ(STO(NAME))		},
	{ S_NAME,	{ ALP, NUM },	PUT(S_NAME)		},
	{ S_NAME,	{ '\\' },	S_NAME_BS		},

	/* after a \ in an identifier */
	{ S_NAME_BS,	{ ANY },	FRZ(S_ROGUE_BS)		},
	{ S_NAME_BS,	{ 'u', 'U' },	PUT(S_NAME)		},

	/* after a L */
	{ S_LCHAR,	{ ANY },	FRZ(S_NAME)		},
	{ S_LCHAR,	{ '"' },	PUT(S_STRING)		},
	{ S_LCHAR,	{ '\'' },	PUT(S_CHAR)		},

	/* after a ^ */
	{ S_CIRC,	{ ANY },	FRZ(STO(CIRC))		},
	{ S_CIRC,	{ '=' },	STO(ASCIRC)		},

	/* after a | */
	{ S_PIPE,	{ ANY },	FRZ(STO(OR))		},
	{ S_PIPE,	{ '=' },	STO(ASOR)		},
	{ S_PIPE,	{ '|' },	STO(LOR)		},

	/* after a / and * */
#ifdef SEMPER_FIDELIS
	{ S_COMMENT,	{ ANY },	PUT(S_COMMENT)		},
	{ S_COMMENT,	{ VCH },	FRZ(S_TRUNCC)		},
	{ S_COMMENT,	{ '*' },	PUT(S_COMMENT2)		},

	{ S_COMMENT2,	{ ANY },	FRZ(S_COMMENT)		},
	{ S_COMMENT2,	{ VCH },	FRZ(S_TRUNCC)		},
	{ S_COMMENT2,	{ '*' },	PUT(S_COMMENT2)		},
	{ S_COMMENT2,	{ '/' },	STO(PUT(COMMENT))	},

	{ S_COMMENT5,	{ ANY },	PUT(S_COMMENT5)		},
	{ S_COMMENT5,	{ VCH },	FRZ(S_DECAY)		},
	{ S_COMMENT5,	{ '\n' },	FRZ(STO(COMMENT))	},
#else
	{ S_COMMENT,	{ ANY },	S_COMMENT		},
	{ S_COMMENT,	{ VCH },	FRZ(S_TRUNCC)		},
	{ S_COMMENT,	{ '*' },	S_COMMENT2		},

	{ S_COMMENT2,	{ ANY },	FRZ(S_COMMENT)		},
	{ S_COMMENT2,	{ VCH },	FRZ(S_TRUNCC)		},
	{ S_COMMENT2,	{ '*' },	S_COMMENT2		},
	{ S_COMMENT2,	{ '/' },	STO(COMMENT)		},

	{ S_COMMENT5,	{ ANY },	S_COMMENT5		},
	{ S_COMMENT5,	{ VCH },	FRZ(S_DECAY)		},
	{ S_COMMENT5,	{ '\n' },	FRZ(STO(COMMENT))	},
#endif

	/* dummy end of machine description */
	{ 0,		{ 0 },		0			}
};

/*
 * cppm is the table used to store the automaton: if we are in state s
 * and we read character c, we apply the action cppm[s][c] (jumping to
 * another state, or emitting a token).
 * cppm_vch is the table for the special virtual character "end of input"
 */
static int cppm[MSTATE][MAX_CHAR_VAL];
static int cppm_vch[MSTATE];

/*
 * init_cppm() fills cppm[][] with the information stored in cppms[].
 * It must be called before beginning the lexing process.
 */
void init_cppm(void)
{
	int i, j, k, c;
	static unsigned char upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static unsigned char lower[] = "abcdefghijklmnopqrstuvwxyz";
	unsigned char *cp;

	for (i = 0; i < MSTATE; i ++) {
		for (j = 0; j < MAX_CHAR_VAL; j ++) cppm[i][j] = S_OUCH;
		cppm_vch[i] = S_OUCH;
	}
	for (i = 0; cppms[i].input[0]; i ++) for (k = 0; k < CMCR; k ++) {
		int s = cppms[i].state;
		int ns = cppms[i].new_state;

		switch (c = cppms[i].input[k]) {
		case 0:
			break;
		case SPC:
			/* see space_char() also */
			cppm[s][' '] = ns;
			cppm[s]['\t'] = ns;
			cppm[s]['\v'] = ns;
			cppm[s]['\f'] = ns;
#ifdef UNBREAKABLE_SPACE
			if (MAX_CHAR_VAL > UNBREAKABLE_SPACE)
				cppm[s][UNBREAKABLE_SPACE] = ns;
#endif
			break;
		case ALP:
			for (cp = upper; *cp; cp ++) cppm[s][(int)*cp] = ns;
			for (cp = lower; *cp; cp ++) cppm[s][(int)*cp] = ns;
			cppm[s]['_'] = ns;
			break;
		case NUM:
			for (j = '0'; j <= '9'; j ++) cppm[s][j] = ns;
			break;
		case ANY:
			for (j = 0; j < MAX_CHAR_VAL; j ++) cppm[s][j] = ns;
			cppm_vch[s] = ns;
			break;
		case VCH:
			cppm_vch[s] = ns;
			break;
		default:
			cppm[s][c] = ns;
			break;
		}
	}
}

/*
 * Make some character as equivalent to a letter for identifiers.
 */
void set_identifier_char(int c)
{
	cppm[S_START][c] = PUT(S_NAME);
	cppm[S_NAME][c] = PUT(S_NAME);
}

/*
 * Remove the "identifier" status from a character.
 */
void unset_identifier_char(int c)
{
	cppm[S_START][c] = S_ILL;
	cppm[S_NAME][c] = FRZ(STO(NAME));
}

int space_char(int c)
{
	if (c == ' ' || c == '\t' || c == '\v' || c == '\f'
#ifdef UNBREAKABLE_SPACE
		|| c == UNBREAKABLE_SPACE
#endif
		) return 1;
	return 0;
}

#ifndef NO_UCPP_BUF
/*
 * our output buffer is full, flush it
 */
void flush_output(struct lexer_state *ls)
{
	size_t x = ls->sbuf, y = 0, z;

	if (ls->sbuf == 0) return;
	do {
		z = fwrite(ls->output_buf + y, 1, x, ls->output);
		x -= z;
		y += z;
	} while (z && x > 0);
	if (!y) {
		error(ls->line, "could not flush output (disk full ?)");
		die();
	}
	ls->sbuf = 0;
}
#endif

/*
 * Output one character; flush the buffer if needed.
 * This function should not be called, except by put_char().
 */
static inline void write_char(struct lexer_state *ls, unsigned char c)
{
#ifndef NO_UCPP_BUF
	ls->output_buf[ls->sbuf ++] = c;
	if (ls->sbuf == OUTPUT_BUF_MEMG) flush_output(ls);
#else
	if (putc((int)c, ls->output) == EOF) {
		error(ls->line, "output write error (disk full ?)");
		die();
	}
#endif
	if (c == '\n') {
		ls->oline ++;
	}
}

/*
 * schedule a character for output
 */
void put_char(struct lexer_state *ls, unsigned char c)
{
	if (ls->flags & KEEP_OUTPUT) write_char(ls, c);
}

/*
 * get next raw input character
 */
static inline int read_char(struct lexer_state *ls)
{
	unsigned char c;

	if (!ls->input) {
		return ((ls->pbuf ++) < ls->ebuf) ?
			ls->input_string[ls->pbuf - 1] : -1;
	}
	while (1) {
#ifndef NO_UCPP_BUF
		if (ls->pbuf == ls->ebuf) {
#ifdef UCPP_MMAP
			if (ls->from_mmap) {
				munmap((void *)ls->input_buf, ls->ebuf);
				ls->from_mmap = 0;
				ls->input_buf = ls->input_buf_sav;
			}
#endif
			ls->ebuf = fread(ls->input_buf, 1,
				INPUT_BUF_MEMG, ls->input);
			ls->pbuf = 0;
		}
		if (ls->ebuf == 0) return -1;
		c = ls->input_buf[ls->pbuf ++];
#else
		int x = getc(ls->input);

		if (x == EOF) return -1;
		c = x;
#endif
		if (ls->flags & COPY_LINE) {
			if (c == '\n') {
				ls->copy_line[ls->cli] = 0;
				ls->cli = 0;
			} else if (ls->cli < (COPY_LINE_LENGTH - 1)) {
				ls->copy_line[ls->cli ++] = c;
			}
		}
		if (ls->macfile && c == '\n') {
			ls->macfile = 0;
			continue;
		}
		ls->macfile = 0;
		if (c == '\r') {
			/*
			 * We found a '\r'; we handle it as a newline
			 * and ignore the next newline. This should work
			 * with all combinations of Msdos, MacIntosh and
			 * Unix files on these three platforms. On other
			 * platforms, native file formats are always
			 * supported.
			 */
			ls->macfile = 1;
			c = '\n';
		}
		break;
	}
	return c;
}

/*
 * next_fifo_char(), char_lka1() and char_lka2() give a two character
 * look-ahead on the input stream; this is needed for trigraphs
 */
static inline int next_fifo_char(struct lexer_state *ls)
{
	int c;

	if (ls->nlka != 0) {
		c = ls->lka[0];
		ls->lka[0] = ls->lka[1];
		ls->nlka --;
	} else c = read_char(ls);
	return c;
}

static inline int char_lka1(struct lexer_state *ls)
{
	if (ls->nlka == 0) {
		ls->lka[0] = read_char(ls);
		ls->nlka ++;
	}
	return ls->lka[0];
}

static inline int char_lka2(struct lexer_state *ls)
{
#ifdef AUDIT
	if (ls->nlka == 0) ouch("always in motion future is");
#endif
	if (ls->nlka == 1) {
		ls->lka[1] = read_char(ls);
		ls->nlka ++;
	}
	return ls->lka[1];
}

static struct trigraph {
	int old, new;
} trig[9] = {
	{ '=', '#' },
	{ '/', '\\' },
	{ '\'', '^' },
	{ '(', '[' },
	{ ')', ']' },
	{ '!', '|' },
	{ '<', '{' },
	{ '>', '}' },
	{ '-', '~' }
};

/*
 * Returns the next character, after treatment of trigraphs and terminating
 * backslashes. Return value is -1 if there is no more input.
 */
static inline int next_char(struct lexer_state *ls)
{
	int c;

	if (!ls->discard) return ls->last;
	ls->discard = 0;
	do {
		c = next_fifo_char(ls);
		/* check trigraphs */
		if (c == '?' && char_lka1(ls) == '?'
			&& (ls->flags & HANDLE_TRIGRAPHS)) {
			int i, d;

			d = char_lka2(ls);
			for (i = 0; i < 9; i ++) if (d == trig[i].old) {
				if (ls->flags & WARN_TRIGRAPHS) {
					ls->count_trigraphs ++;
				}
				if (ls->flags & WARN_TRIGRAPHS_MORE) {
					warning(ls->line, "trigraph ?""?%c "
						"encountered", d);
				}
				next_fifo_char(ls);
				next_fifo_char(ls);
				c = trig[i].new;
				break;
			}
		}
		if (c == '\\' && char_lka1(ls) == '\n') {
			ls->line ++;
			next_fifo_char(ls);
		} else if (c == '\r' && char_lka1(ls) == '\n') {
			ls->line ++;
			next_fifo_char(ls);
			c = '\n';
			return c;
		} else {
			ls->last = c;
			return c;
		}
	} while (1);
}

/*
 * wrapper for next_char(), to be called from outside
 * (used by #error, #include directives)
 */
int grap_char(struct lexer_state *ls)
{
	return next_char(ls);
}

/*
 * Discard the current character, so that the next call to next_char()
 * will step into the input stream.
 */
void discard_char(struct lexer_state *ls)
{
#ifdef AUDIT
	if (ls->discard) ouch("overcollecting garbage");
#endif
	ls->discard = 1;
	ls->utf8 = 0;
	if (ls->last == '\n') ls->line ++;
}

/*
 * Convert an UTF-8 encoded character to a Universal Character Name
 * using \u (or \U when appropriate).
 */
static int utf8_to_string(unsigned char buf[], unsigned long utf8)
{
	unsigned long val = 0;
	static char hex[16] = "0123456789abcdef";

	if (utf8 & 0x80UL) {
		unsigned long x1, x2, x3, x4;

		x1 = (utf8 >> 24) & 0x7fUL;
		x2 = (utf8 >> 16) & 0x7fUL;
		x3 = (utf8 >> 8) & 0x7fUL;
		x4 = (utf8) & 0x3fUL;
		x1 &= 0x07UL;
		if (x2 & 0x40UL) x2 &= 0x0fUL;
		if (x3 & 0x40UL) x3 &= 0x1fUL;
		val = x4 | (x3 << 6) | (x2 << 12) | (x1 << 16);
	} else val = utf8;
	if (val < 128) {
		buf[0] = val;
		buf[1] = 0;
		return 1;
	} else if (val < 0xffffUL) {
		buf[0] = '\\';
		buf[1] = 'u';
		buf[2] = hex[(size_t)(val >> 12)];
		buf[3] = hex[(size_t)((val >> 8) & 0xfU)];
		buf[4] = hex[(size_t)((val >> 4) & 0xfU)];
		buf[5] = hex[(size_t)(val & 0xfU)];
		buf[6] = 0;
		return 6;
	}
	buf[0] = '\\';
	buf[1] = 'U';
	buf[2] = '0';
	buf[3] = '0';
	buf[4] = hex[(size_t)(val >> 20)];
	buf[5] = hex[(size_t)((val >> 16) & 0xfU)];
	buf[6] = hex[(size_t)((val >> 12) & 0xfU)];
	buf[7] = hex[(size_t)((val >> 8) & 0xfU)];
	buf[8] = hex[(size_t)((val >> 4) & 0xfU)];
	buf[9] = hex[(size_t)(val & 0xfU)];
	buf[10] = 0;
	return 10;
}

/*
 * Scan the identifier and put it in canonical form:
 *  -- tranform \U0000xxxx into \uxxxx
 *  -- inside \u and \U, make letters low case
 *  -- report (some) incorrect use of UCN
 */
static void canonize_id(struct lexer_state *ls, char *id)
{
	char *c, *d;

	for (c = d = id; *c;) {
		if (*c == '\\') {
			int i;

			if (!*(c + 1)) goto canon_error;
			if (*(c + 1) == 'U') {
				for (i = 0; i < 8 && *(c + i + 2); i ++);
				if (i != 8) goto canon_error;
				*(d ++) = '\\';
				c += 2;
				for (i = 0; i < 4 && *(c + i) == '0'; i ++);
				if (i == 4) {
					*(d ++) = 'u';
					c += 4;
				} else {
					*(d ++) = 'U';
					i = 8;
				}
				for (; i > 0; i --) {
					switch (*c) {
					case 'A': *(d ++) = 'a'; break;
					case 'B': *(d ++) = 'b'; break;
					case 'C': *(d ++) = 'c'; break;
					case 'D': *(d ++) = 'd'; break;
					case 'E': *(d ++) = 'e'; break;
					case 'F': *(d ++) = 'f'; break;
					default: *(d ++) = *c; break;
					}
					c ++;
				}
			} else if (*(c + 1) == 'u') {
				for (i = 0; i < 4 && *(c + i + 2); i ++);
				if (i != 4) goto canon_error;
				*(d ++) = '\\';
				*(d ++) = 'u';
				c += 2;
				for (; i > 0; i --) {
					switch (*c) {
					case 'A': *(d ++) = 'a'; break;
					case 'B': *(d ++) = 'b'; break;
					case 'C': *(d ++) = 'c'; break;
					case 'D': *(d ++) = 'd'; break;
					case 'E': *(d ++) = 'e'; break;
					case 'F': *(d ++) = 'f'; break;
					default: *(d ++) = *c; break;
					}
					c ++;
				}
			} else goto canon_error;
			continue;
		}
		*(d ++) = *(c ++);
	}
	*d = 0;
	return;

canon_error:
	for (; *c; *(d ++) = *(c ++));
	if (ls->flags & WARN_STANDARD) {
		warning(ls->line, "malformed identifier with UCN: '%s'", id);
	}
	*d = 0;
}

/*
 * Run the automaton, in order to get the next token.
 * This function should not be called, except by next_token()
 *
 * return value: 1 on error, 2 on end-of-file, 0 otherwise.
 */
static inline int read_token(struct lexer_state *ls)
{
	int cstat = S_START, nstat;
	size_t ltok = 0;
	int c, outc = 0, ucn_in_id = 0;
	int shift_state;
	unsigned long utf8;
	long l = ls->line;

	ls->ctok->line = l;
	if (ls->pending_token) {
		if ((ls->ctok->type = ls->pending_token) == BUNCH) {
			ls->ctok->name[0] = '\\';
			ls->ctok->name[1] = 0;
		}
		ls->pending_token = 0;
		return 0;
	}
	if (ls->flags & UTF8_SOURCE) {
		utf8 = ls->utf8;
		shift_state = 0;
	}
	if (!(ls->flags & LEXER) && (ls->flags & KEEP_OUTPUT))
		for (; ls->line > ls->oline;) put_char(ls, '\n');
	do {
		c = next_char(ls);
		if (c < 0) {
			if ((ls->flags & UTF8_SOURCE) && shift_state) {
				if (ls->flags & WARN_STANDARD)
					warning(ls->line, "truncated UTF-8 "
						"character");
				shift_state = 0;
				utf8 = 0;
			}
			if (cstat == S_START) return 2;
			nstat = cppm_vch[cstat];
		} else {
			if (ls->flags & UTF8_SOURCE) {
				if (shift_state) {
					if ((c & 0xc0) != 0x80) {
						if (ls->flags & WARN_STANDARD)
							warning(ls->line,
								"truncated "
								"UTF-8 "
								"character");
						shift_state = 0;
						utf8 = 0;
						c = '_';
					} else {
						utf8 = (utf8 << 8) | c;
						if (-- shift_state) {
							ls->discard = 1;
							continue;
						}
						c = '_';
					}
				} else if ((c & 0xc0) == 0xc0) {
					if ((c & 0x30) == 0x30) {
						shift_state = 3;
					} else if (c & 0x20) {
						shift_state = 2;
					} else {
						shift_state = 1;
					}
					utf8 = c;
					ls->discard = 1;
					continue;
				} else utf8 = 0;
			}
			nstat = cppm[cstat][c < MAX_CHAR_VAL ? c : 0];
		}
#ifdef AUDIT
		if (nstat == S_OUCH) {
			ouch("bad move...");
		}
#endif
		/*
		 * disable C++-like comments
		 */
		if (nstat == S_COMMENT5 && !(ls->flags & CPLUSPLUS_COMMENTS))
			nstat = FRZ(STO(SLASH));

		if (noMOD(nstat) >= MSTATE && !ttSTO(nstat))
			switch (noMOD(nstat)) {
		case S_ILL:
			if (ls->flags & CCHARSET) {
				error(ls->line, "illegal character '%c'", c);
				return 1;
			}
			nstat = PUT(STO(BUNCH));
			break;
		case S_BS:
			ls->ctok->name[0] = '\\';
			ltok ++;
			nstat = FRZ(STO(BUNCH));
			if (!(ls->flags & LEXER)) put_char(ls, '\\');
			break;
		case S_ROGUE_BS:
			ls->pending_token = BUNCH;
			nstat = FRZ(STO(NAME));
			break;
		case S_DDOT:
			ls->pending_token = DOT;
			nstat = FRZ(STO(DOT));
			break;
		case S_DDSHARP:
			ls->pending_token = PCT;
			nstat = FRZ(STO(DIG_SHARP));
			break;
		case S_BEHEAD:
			error(l, "unfinished string at end of line");
			return 1;
		case S_DECAY:
			warning(l, "unterminated // comment");
			nstat = FRZ(STO(COMMENT));
			break;
		case S_TRUNC:
			error(l, "truncated token");
			return 1;
		case S_TRUNCC:
			error(l, "truncated comment");
			return 1;
#ifdef AUDIT
		case S_OUCH:
			ouch("machine went out of control");
			break;
#endif
		}
		if (!ttFRZ(nstat)) {
			discard_char(ls);
			if (!(ls->flags & LEXER) && ls->condcomp) {
				int z = ttSTO(nstat) ? S_ILL : noMOD(nstat);

				if (cstat == S_NAME || z == S_NAME
					|| ((CMT(cstat) || CMT(z))
					&& (ls->flags & DISCARD_COMMENTS))) {
					outc = 0;
				} else if (z == S_LCHAR || z == S_SLASH
					|| (z == S_SHARP && ls->ltwnl)
					|| (z == S_PCT && ls->ltwnl)
					|| (z == S_BACKSLASH)) {
					outc = c;
				} else if (z == S_PCT2 && ls->ltwnl) {
					outc = -1;
				} else if (z == S_PCT3 && ls->ltwnl) {
					/* we have %:% but this still might
					   not be a %:%: */
					outc = -2;
				} else {
					if (outc < 0) {
						put_char(ls, '%');
						put_char(ls, ':');
						if (outc == -2)
							put_char(ls, '%');
						outc = 0;
					} else if (outc) {
						put_char(ls, outc);
						outc = 0;
					}
					put_char(ls, c);
				}
			}
		} else if (outc == '/' && !(ls->flags & LEXER)
			&& ls->condcomp) {
			/* this is a hack: we need to dump a pending slash */
			put_char(ls, outc);
			outc = 0;
		}
		if (ttPUT(nstat)) {
			if (cstat == S_NAME_BS) {
				ucn_in_id = 1;
				wan(ls->ctok->name, ltok, '\\', ls->tknl);
			}
			if ((ls->flags & UTF8_SOURCE) && utf8) {
				unsigned char buf[11];
				int i, j;

				for (i = 0, j = utf8_to_string(buf, utf8);
					i < j; i ++)
					wan(ls->ctok->name, ltok, buf[i],
						ls->tknl);
				/* if (j > 1) ucn_in_id = 1; */
			} else wan(ls->ctok->name, ltok,
				(unsigned char)c, ls->tknl);
		}
		if (ttSTO(nstat)) {
			if (S_TOKEN(noMOD(nstat))) {
				wan(ls->ctok->name, ltok,
					(unsigned char)0, ls->tknl);
			}
			ls->ctok->type = noMOD(nstat);
			break;
		}
		cstat = noMOD(nstat);
	} while (1);
	if (!(ls->flags & LEXER) && (ls->flags & DISCARD_COMMENTS)
			&& ls->ctok->type == COMMENT) put_char(ls, ' ');
	if (ucn_in_id && ls->ctok->type == NAME)
		canonize_id(ls, ls->ctok->name);
	return 0;
}

/*
 * fills ls->ctok with the next token
 */
int next_token(struct lexer_state *ls)
{
	if (ls->flags & READ_AGAIN) {
		ls->flags &= ~READ_AGAIN;
		if (!(ls->flags & LEXER)) {
			char *c = S_TOKEN(ls->ctok->type) ?
				ls->ctok->name : token_name(ls->ctok);
			if (ls->ctok->type == OPT_NONE) {
				ls->ctok->type = NONE;
#ifdef SEMPER_FIDELIS
				ls->ctok->name[0] = ' ';
				ls->ctok->name[1] = 0;
#endif
				put_char(ls, ' ');
			} else if (ls->ctok->type != NAME &&
				!(ls->ltwnl && (ls->ctok->type == SHARP
					|| ls->ctok->type == DIG_SHARP)))
				for (; *c; c ++) put_char(ls, *c);
		}
		return 0;
	}
	return read_token(ls);
}
