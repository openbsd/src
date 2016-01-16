/*
 * This file is in the public domain.
 * It contains parts from ksh/config.h, which is in the public domain,
 * and additions by Ingo Schwarze <schwarze@openbsd.org> (2016),
 * who places the additions in the public domain, too.
 */

#define BEL	0x07

/* tty driver characters we are interested in */
typedef struct {
	int erase;
	int kill;
	int werase;
	int intr;
	int quit;
	int eof;
} X_chars;

/* edit.c */
extern X_chars edchars;

#define x_getc()				getchar()
#define x_flush()
#define x_putc(c)				putchar(c)
#define x_puts(s)				fputs(s, stdout)
#define x_mode(a)
#define x_do_comment(a, b, c)			-1
#define x_print_expansions(a, b, c)
#define x_cf_glob(a, b, c, d, e, f, g, h)	0
#define x_longest_prefix(a, b)			0
#define x_basename(a, b)			0
#define x_free_words(a, b)
#define x_escape(a, b, c)			-1

int x_vi(char *, size_t);

/* lex.c */
int promptlen(const char *, const char **);
