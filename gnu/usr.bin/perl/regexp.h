/*    regexp.h
 */

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */


typedef struct regexp {
	char **startp;
	char **endp;
	SV *regstart;		/* Internal use only. */
	char *regstclass;
	SV *regmust;		/* Internal use only. */
	I32 regback;		/* Can regmust locate first try? */
	I32 minlen;		/* mininum possible length of $& */
	I32 prelen;		/* length of precomp */
	U32 nparens;		/* number of parentheses */
	U32 lastparen;		/* last paren matched */
	char *precomp;		/* pre-compilation regular expression */
	char *subbase;		/* saved string so \digit works forever */
	char *subbeg;		/* same, but not responsible for allocation */
	char *subend;		/* end of subbase */
	U16 naughty;		/* how exponential is this pattern? */
	char reganch;		/* Internal use only. */
	char exec_tainted;	/* Tainted information used by regexec? */
	char program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

#define ROPT_ANCH	3
#define  ROPT_ANCH_BOL	 1
#define  ROPT_ANCH_GPOS	 2
#define ROPT_SKIP	4
#define ROPT_IMPLICIT	8
