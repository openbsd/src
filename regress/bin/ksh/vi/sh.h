/*
 * This file is in the public domain.
 * It contains parts from ksh/*.h, which are in the public domain,
 * and additions by Ingo Schwarze <schwarze@openbsd.org> (2016),
 * who places the additions in the public domain, too.
 */

#include <stdlib.h>	/* for malloc(3) */
#include <stdio.h>	/* for snprintf(3) */

/* sh.h */
#define Flag(f)		0
#define letnum(c)	(isalnum((unsigned char)(c)))
#define MIN_EDIT_SPACE	7
#define x_cols		80

/* sh.h version.c */
extern const char ksh_version[];

/* shf.h shf.c */
#define shf_snprintf		snprintf

/* table.h table.c */
struct tbl {			/* table item */
	int	flag;		/* flags */
	union {
		char *s;	/* string */
	} val;			/* value */
};
#define ISSET			0
extern const char *prompt;
#define ktsearch(a, b, c)	NULL

/* lex.h lex.c */
struct source { int line; };
extern struct source *source;
void pprompt(const char *, int);

/* sh.h alloc.c */
#define alloc(s, a)		malloc(s)
#define aresize(p, s, a)	realloc(p, s)
#define afree(p, a)		free(p)

/* sh.h history.c */
#define histsave(a, b, c)
char **histpos(void);
#define histnum(a)		0
#define findhist(a, b, c, d)	-1

/* sh.h io.c */
#define internal_errorf(i, s)	warnx(s)

/* sh.h main.c */
#define unwind(a)		errx(1, "unwind")

/* sh.h trap.c */
#define trapsig(a)		errx(1, "trapsig")
