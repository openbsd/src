/*	$OpenBSD: misc.c,v 1.19 2003/09/01 15:47:40 naddy Exp $	*/

/*
 * Miscellaneous functions
 */

#include "sh.h"
#include <ctype.h>	/* for FILECHCONV */
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#ifndef UCHAR_MAX
# define UCHAR_MAX	0xFF
#endif

short ctypes [UCHAR_MAX+1];	/* type bits for unsigned char */

static int	do_gmatch ARGS((const unsigned char *s, const unsigned char *p,
			const unsigned char *se, const unsigned char *pe,
			int isfile));
static const unsigned char *cclass ARGS((const unsigned char *p, int sub));

/*
 * Fast character classes
 */
void
setctypes(s, t)
	register const char *s;
	register int t;
{
	register int i;

	if (t & C_IFS) {
		for (i = 0; i < UCHAR_MAX+1; i++)
			ctypes[i] &= ~C_IFS;
		ctypes[0] |= C_IFS; /* include \0 in C_IFS */
	}
	while (*s != 0)
		ctypes[(unsigned char) *s++] |= t;
}

void
initctypes()
{
	register int c;

	for (c = 'a'; c <= 'z'; c++)
		ctypes[c] |= C_ALPHA;
	for (c = 'A'; c <= 'Z'; c++)
		ctypes[c] |= C_ALPHA;
	ctypes['_'] |= C_ALPHA;
	setctypes("0123456789", C_DIGIT);
	setctypes(" \t\n|&;<>()", C_LEX1); /* \0 added automatically */
	setctypes("*@#!$-?", C_VAR1);
	setctypes(" \t\n", C_IFSWS);
	setctypes("=-+?", C_SUBOP1);
	setctypes("#%", C_SUBOP2);
	setctypes(" \n\t\"#$&'()*;<>?[\\`|", C_QUOTE);
}

/* convert unsigned long to base N string */

char *
ulton(n, base)
	register unsigned long n;
	int base;
{
	register char *p;
	static char buf [20];

	p = &buf[sizeof(buf)];
	*--p = '\0';
	do {
		*--p = "0123456789ABCDEF"[n%base];
		n /= base;
	} while (n != 0);
	return p;
}

char *
str_save(s, ap)
	register const char *s;
	Area *ap;
{
	size_t len;
	char *p;

	if (!s)
		return NULL;
	len = strlen(s)+1;
	p = alloc(len, ap);
	strlcpy(p, s, len+1);
	return (p);
}

/* Allocate a string of size n+1 and copy upto n characters from the possibly
 * null terminated string s into it.  Always returns a null terminated string
 * (unless n < 0).
 */
char *
str_nsave(s, n, ap)
	register const char *s;
	int n;
	Area *ap;
{
	char *ns;

	if (n < 0)
		return 0;
	ns = alloc(n + 1, ap);
	ns[0] = '\0';
	return strncat(ns, s, n);
}

/* called from expand.h:XcheckN() to grow buffer */
char *
Xcheck_grow_(xsp, xp, more)
	XString *xsp;
	char *xp;
	int more;
{
	char *old_beg = xsp->beg;

	xsp->len += more > xsp->len ? more : xsp->len;
	xsp->beg = aresize(xsp->beg, xsp->len + 8, xsp->areap);
	xsp->end = xsp->beg + xsp->len;
	return xsp->beg + (xp - old_beg);
}

const struct option options[] = {
	/* Special cases (see parse_args()): -A, -o, -s.
	 * Options are sorted by their longnames - the order of these
	 * entries MUST match the order of sh_flag F* enumerations in sh.h.
	 */
	{ "allexport",	'a',		OF_ANY },
#ifdef BRACE_EXPAND
	{ "braceexpand",  0,		OF_ANY }, /* non-standard */
#endif
	{ "bgnice",	  0,		OF_ANY },
	{ (char *) 0, 	'c',	    OF_CMDLINE },
#ifdef EMACS
	{ "emacs",	  0,		OF_ANY },
	{ "emacs-usemeta",  0,		OF_ANY }, /* non-standard */
#endif
	{ "errexit",	'e',		OF_ANY },
#ifdef EMACS
	{ "gmacs",	  0,		OF_ANY },
#endif
	{ "ignoreeof",	  0,		OF_ANY },
	{ "interactive",'i',	    OF_CMDLINE },
	{ "keyword",	'k',		OF_ANY },
	{ "login",	'l',	    OF_CMDLINE },
	{ "markdirs",	'X',		OF_ANY },
#ifdef JOBS
	{ "monitor",	'm',		OF_ANY },
#else /* JOBS */
	{ (char *) 0,	'm',		     0 }, /* so FMONITOR not ifdef'd */
#endif /* JOBS */
	{ "noclobber",	'C',		OF_ANY },
	{ "noexec",	'n',		OF_ANY },
	{ "noglob",	'f',		OF_ANY },
	{ "nohup",	  0,		OF_ANY },
	{ "nolog",	  0,		OF_ANY }, /* no effect */
#ifdef	JOBS
	{ "notify",	'b',		OF_ANY },
#endif	/* JOBS */
	{ "nounset",	'u',		OF_ANY },
	{ "physical",	  0,		OF_ANY }, /* non-standard */
	{ "posix",	  0,		OF_ANY }, /* non-standard */
	{ "privileged",	'p',		OF_ANY },
	{ "restricted",	'r',	    OF_CMDLINE },
	{ "sh",		  0,		OF_ANY }, /* non-standard */
	{ "stdin",	's',	    OF_CMDLINE }, /* pseudo non-standard */
	{ "trackall",	'h',		OF_ANY },
	{ "verbose",	'v',		OF_ANY },
#ifdef VI
	{ "vi",		  0,		OF_ANY },
	{ "viraw",	  0,		OF_ANY }, /* no effect */
	{ "vi-show8",	  0,		OF_ANY }, /* non-standard */
	{ "vi-tabcomplete",  0, 	OF_ANY }, /* non-standard */
	{ "vi-esccomplete",  0, 	OF_ANY }, /* non-standard */
#endif
	{ "xtrace",	'x',		OF_ANY },
	/* Anonymous flags: used internally by shell only
	 * (not visable to user)
	 */
	{ (char *) 0,	0,		OF_INTERNAL }, /* FTALKING_I */
};

/*
 * translate -o option into F* constant (also used for test -o option)
 */
int
option(n)
	const char *n;
{
	int i;

	for (i = 0; i < NELEM(options); i++)
		if (options[i].name && strcmp(options[i].name, n) == 0)
			return i;

	return -1;
}

struct options_info {
	int opt_width;
	struct {
		const char *name;
		int	flag;
	} opts[NELEM(options)];
};

static char *options_fmt_entry ARGS((void *arg, int i, char *buf, int buflen));
static void printoptions ARGS((int verbose));

/* format a single select menu item */
static char *
options_fmt_entry(arg, i, buf, buflen)
	void *arg;
	int i;
	char *buf;
	int buflen;
{
	struct options_info *oi = (struct options_info *) arg;

	shf_snprintf(buf, buflen, "%-*s %s",
		oi->opt_width, oi->opts[i].name,
		Flag(oi->opts[i].flag) ? "on" : "off");
	return buf;
}

static void
printoptions(verbose)
	int verbose;
{
	int i;

	if (verbose) {
		struct options_info oi;
		int n, len;

		/* verbose version */
		shprintf("Current option settings\n");

		for (i = n = oi.opt_width = 0; i < NELEM(options); i++)
			if (options[i].name) {
				len = strlen(options[i].name);
				oi.opts[n].name = options[i].name;
				oi.opts[n++].flag = i;
				if (len > oi.opt_width)
					oi.opt_width = len;
			}
		print_columns(shl_stdout, n, options_fmt_entry, &oi,
			      oi.opt_width + 5, 1);
	} else {
		/* short version ala ksh93 */
		shprintf("set");
		for (i = 0; i < NELEM(options); i++)
			if (Flag(i) && options[i].name)
				shprintf(" -o %s", options[i].name);
		shprintf(newline);
	}
}

char *
getoptions()
{
	int i;
	char m[(int) FNFLAGS + 1];
	register char *cp = m;

	for (i = 0; i < NELEM(options); i++)
		if (options[i].c && Flag(i))
			*cp++ = options[i].c;
	*cp = 0;
	return str_save(m, ATEMP);
}

/* change a Flag(*) value; takes care of special actions */
void
change_flag(f, what, newval)
	enum sh_flag f;	/* flag to change */
	int what;	/* what is changing the flag (command line vs set) */
	int newval;
{
	int oldval;

	oldval = Flag(f);
	Flag(f) = newval;
#ifdef JOBS
	if (f == FMONITOR) {
		if (what != OF_CMDLINE && newval != oldval)
			j_change();
	} else
#endif /* JOBS */
#ifdef EDIT
	if (0
# ifdef VI
	    || f == FVI
# endif /* VI */
# ifdef EMACS
	    || f == FEMACS || f == FGMACS
# endif /* EMACS */
	   )
	{
		if (newval) {
# ifdef VI
			Flag(FVI) = 0;
# endif /* VI */
# ifdef EMACS
			Flag(FEMACS) = Flag(FGMACS) = 0;
# endif /* EMACS */
			Flag(f) = newval;
		}
	} else
#endif /* EDIT */
	/* Turning off -p? */
	if (f == FPRIVILEGED && oldval && !newval) {
#ifdef OS2
		;
#else /* OS2 */
		seteuid(ksheuid = getuid());
		setuid(ksheuid);
		setegid(getgid());
		setgid(getgid());
#endif /* OS2 */
	} else if (f == FPOSIX && newval) {
#ifdef BRACE_EXPAND
		Flag(FBRACEEXPAND) = 0
#endif /* BRACE_EXPAND */
		;
	}
	/* Changing interactive flag? */
	if (f == FTALKING) {
		if ((what == OF_CMDLINE || what == OF_SET) && procpid == kshpid)
			Flag(FTALKING_I) = newval;
	}
}

/* parse command line & set command arguments.  returns the index of
 * non-option arguments, -1 if there is an error.
 */
int
parse_args(argv, what, setargsp)
	char **argv;
	int	what;		/* OF_CMDLINE or OF_SET */
	int	*setargsp;
{
	static char cmd_opts[NELEM(options) + 3]; /* o:\0 */
	static char set_opts[NELEM(options) + 5]; /* Ao;s\0 */
	char *opts;
	char *array = (char *) 0;
	Getopt go;
	int i, optc, set, sortargs = 0, arrayset = 0;

	/* First call?  Build option strings... */
	if (cmd_opts[0] == '\0') {
		char *p, *q;

		/* see cmd_opts[] declaration */
		strlcpy(cmd_opts, "o:", sizeof cmd_opts);
		p = cmd_opts + strlen(cmd_opts);
		/* see set_opts[] declaration */
		strlcpy(set_opts, "A:o;s", sizeof set_opts);
		q = set_opts + strlen(set_opts);
		for (i = 0; i < NELEM(options); i++) {
			if (options[i].c) {
				if (options[i].flags & OF_CMDLINE)
					*p++ = options[i].c;
				if (options[i].flags & OF_SET)
					*q++ = options[i].c;
			}
		}
		*p = '\0';
		*q = '\0';
	}

	if (what == OF_CMDLINE) {
		char *p;
		/* Set FLOGIN before parsing options so user can clear
		 * flag using +l.
		 */
		Flag(FLOGIN) = (argv[0][0] == '-'
				|| ((p = ksh_strrchr_dirsep(argv[0]))
				     && *++p == '-'));
		opts = cmd_opts;
	} else
		opts = set_opts;
	ksh_getopt_reset(&go, GF_ERROR|GF_PLUSOPT);
	while ((optc = ksh_getopt(argv, &go, opts)) != EOF) {
		set = (go.info & GI_PLUS) ? 0 : 1;
		switch (optc) {
		  case 'A':
			arrayset = set ? 1 : -1;
			array = go.optarg;
			break;

		  case 'o':
			if (go.optarg == (char *) 0) {
				/* lone -o: print options
				 *
				 * Note that on the command line, -o requires
				 * an option (ie, can't get here if what is
				 * OF_CMDLINE).
				 */
				printoptions(set);
				break;
			}
			i = option(go.optarg);
			if (i >= 0 && set == Flag(i))
				/* Don't check the context if the flag
				 * isn't changing - makes "set -o interactive"
				 * work if you're already interactive.  Needed
				 * if the output of "set +o" is to be used.
				 */
				;
			else if (i >= 0 && (options[i].flags & what))
				change_flag((enum sh_flag) i, what, set);
			else {
				bi_errorf("%s: bad option", go.optarg);
				return -1;
			}
			break;

		  case '?':
			return -1;

		  default:
			/* -s: sort positional params (at&t ksh stupidity) */
			if (what == OF_SET && optc == 's') {
				sortargs = 1;
				break;
			}
			for (i = 0; i < NELEM(options); i++)
				if (optc == options[i].c
				    && (what & options[i].flags))
				{
					change_flag((enum sh_flag) i, what,
						    set);
					break;
				}
			if (i == NELEM(options)) {
				internal_errorf(1, "parse_args: `%c'", optc);
				return -1; /* not reached */
			}
		}
	}
	if (!(go.info & GI_MINUSMINUS) && argv[go.optind]
	    && (argv[go.optind][0] == '-' || argv[go.optind][0] == '+')
	    && argv[go.optind][1] == '\0')
	{
		/* lone - clears -v and -x flags */
		if (argv[go.optind][0] == '-' && !Flag(FPOSIX))
			Flag(FVERBOSE) = Flag(FXTRACE) = 0;
		/* set skips lone - or + option */
		go.optind++;
	}
	if (setargsp)
		/* -- means set $#/$* even if there are no arguments */
		*setargsp = !arrayset && ((go.info & GI_MINUSMINUS)
					  || argv[go.optind]);

	if (arrayset && (!*array || *skip_varname(array, FALSE))) {
		bi_errorf("%s: is not an identifier", array);
		return -1;
	}
	if (sortargs) {
		for (i = go.optind; argv[i]; i++)
			;
		qsortp((void **) &argv[go.optind], (size_t) (i - go.optind),
			xstrcmp);
	}
	if (arrayset) {
		set_array(array, arrayset, argv + go.optind);
		for (; argv[go.optind]; go.optind++)
			;
	}

	return go.optind;
}

/* parse a decimal number: returns 0 if string isn't a number, 1 otherwise */
int
getn(as, ai)
	const char *as;
	int *ai;
{
	char *p;
	long n;

	n = strtol(as, &p, 10);

	if (!*as || *p || INT_MIN >= n || n >= INT_MAX)
		return 0;

	*ai = (int)n;
	return 1;
}

/* getn() that prints error */
int
bi_getn(as, ai)
	const char *as;
	int *ai;
{
	int rv = getn(as, ai);

	if (!rv)
		bi_errorf("%s: bad number", as);
	return rv;
}

/* -------- gmatch.c -------- */

/*
 * int gmatch(string, pattern)
 * char *string, *pattern;
 *
 * Match a pattern as in sh(1).
 * pattern character are prefixed with MAGIC by expand.
 */

int
gmatch(s, p, isfile)
	const char *s, *p;
	int isfile;
{
	const char *se, *pe;

	if (s == NULL || p == NULL)
		return 0;
	se = s + strlen(s);
	pe = p + strlen(p);
	/* isfile is false iff no syntax check has been done on
	 * the pattern.  If check fails, just to a strcmp().
	 */
	if (!isfile && !has_globbing(p, pe)) {
		int len = pe - p + 1;
		char tbuf[64];
		char *t = len <= sizeof(tbuf) ? tbuf
				: (char *) alloc(len, ATEMP);
		debunk(t, p, len);
		return !strcmp(t, s);
	}
	return do_gmatch((const unsigned char *) s, (const unsigned char *) se,
			 (const unsigned char *) p, (const unsigned char *) pe,
			 isfile);
}

/* Returns if p is a syntacticly correct globbing pattern, false
 * if it contains no pattern characters or if there is a syntax error.
 * Syntax errors are:
 *	- [ with no closing ]
 *	- imbalanced $(...) expression
 *	- [...] and *(...) not nested (eg, [a$(b|]c), *(a[b|c]d))
 */
/*XXX
- if no magic,
	if dest given, copy to dst
	return ?
- if magic && (no globbing || syntax error)
	debunk to dst
	return ?
- return ?
*/
int
has_globbing(xp, xpe)
	const char *xp, *xpe;
{
	const unsigned char *p = (const unsigned char *) xp;
	const unsigned char *pe = (const unsigned char *) xpe;
	int c;
	int nest = 0, bnest = 0;
	int saw_glob = 0;
	int in_bracket = 0; /* inside [...] */

	for (; p < pe; p++) {
		if (!ISMAGIC(*p))
			continue;
		if ((c = *++p) == '*' || c == '?')
			saw_glob = 1;
		else if (c == '[') {
			if (!in_bracket) {
				saw_glob = 1;
				in_bracket = 1;
				if (ISMAGIC(p[1]) && p[2] == NOT)
					p += 2;
				if (ISMAGIC(p[1]) && p[2] == ']')
					p += 2;
			}
			/* XXX Do we need to check ranges here? POSIX Q */
		} else if (c == ']') {
			if (in_bracket) {
				if (bnest)		/* [a*(b]) */
					return 0;
				in_bracket = 0;
			}
		} else if ((c & 0x80) && strchr("*+?@! ", c & 0x7f)) {
			saw_glob = 1;
			if (in_bracket)
				bnest++;
			else
				nest++;
		} else if (c == '|') {
			if (in_bracket && !bnest)	/* *(a[foo|bar]) */
				return 0;
		} else if (c == /*(*/ ')') {
			if (in_bracket) {
				if (!bnest--)		/* *(a[b)c] */
					return 0;
			} else if (nest)
				nest--;
		}
		/* else must be a MAGIC-MAGIC, or MAGIC-!, MAGIC--, MAGIC-]
			 MAGIC-{, MAGIC-,, MAGIC-} */
	}
	return saw_glob && !in_bracket && !nest;
}

/* Function must return either 0 or 1 (assumed by code for 0x80|'!') */
static int
do_gmatch(s, se, p, pe, isfile)
	const unsigned char *s, *p;
	const unsigned char *se, *pe;
	int isfile;
{
	register int sc, pc;
	const unsigned char *prest, *psub, *pnext;
	const unsigned char *srest;

	if (s == NULL || p == NULL)
		return 0;
	while (p < pe) {
		pc = *p++;
		sc = s < se ? *s : '\0';
		s++;
		if (isfile) {
			sc = FILECHCONV(sc);
			pc = FILECHCONV(pc);
		}
		if (!ISMAGIC(pc)) {
			if (sc != pc)
				return 0;
			continue;
		}
		switch (*p++) {
		  case '[':
			if (sc == 0 || (p = cclass(p, sc)) == NULL)
				return 0;
			break;

		  case '?':
			if (sc == 0)
				return 0;
			break;

		  case '*':
			if (p == pe)
				return 1;
			s--;
			do {
				if (do_gmatch(s, se, p, pe, isfile))
					return 1;
			} while (s++ < se);
			return 0;

		  /*
		   * [*+?@!](pattern|pattern|..)
		   *
		   * Not ifdef'd KSH as this is needed for ${..%..}, etc.
		   */
		  case 0x80|'+': /* matches one or more times */
		  case 0x80|'*': /* matches zero or more times */
			if (!(prest = pat_scan(p, pe, 0)))
				return 0;
			s--;
			/* take care of zero matches */
			if (p[-1] == (0x80 | '*')
			    && do_gmatch(s, se, prest, pe, isfile))
				return 1;
			for (psub = p; ; psub = pnext) {
				pnext = pat_scan(psub, pe, 1);
				for (srest = s; srest <= se; srest++) {
					if (do_gmatch(s, srest,
						psub, pnext - 2, isfile)
					    && (do_gmatch(srest, se,
							  prest, pe, isfile)
						|| (s != srest
						    && do_gmatch(srest, se,
							p - 2, pe, isfile))))
						return 1;
				}
				if (pnext == prest)
					break;
			}
			return 0;

		  case 0x80|'?': /* matches zero or once */
		  case 0x80|'@': /* matches one of the patterns */
		  case 0x80|' ': /* simile for @ */
			if (!(prest = pat_scan(p, pe, 0)))
				return 0;
			s--;
			/* Take care of zero matches */
			if (p[-1] == (0x80 | '?')
			    && do_gmatch(s, se, prest, pe, isfile))
				return 1;
			for (psub = p; ; psub = pnext) {
				pnext = pat_scan(psub, pe, 1);
				srest = prest == pe ? se : s;
				for (; srest <= se; srest++) {
					if (do_gmatch(s, srest,
						      psub, pnext - 2, isfile)
					    && do_gmatch(srest, se,
							 prest, pe, isfile))
						return 1;
				}
				if (pnext == prest)
					break;
			}
			return 0;

		  case 0x80|'!': /* matches none of the patterns */
			if (!(prest = pat_scan(p, pe, 0)))
				return 0;
			s--;
			for (srest = s; srest <= se; srest++) {
				int matched = 0;

				for (psub = p; ; psub = pnext) {
					pnext = pat_scan(psub, pe, 1);
					if (do_gmatch(s, srest,
						      psub, pnext - 2, isfile))
					{
						matched = 1;
						break;
					}
					if (pnext == prest)
						break;
				}
				if (!matched && do_gmatch(srest, se,
							  prest, pe, isfile))
					return 1;
			}
			return 0;

		  default:
			if (sc != p[-1])
				return 0;
			break;
		}
	}
	return s == se;
}

static const unsigned char *
cclass(p, sub)
	const unsigned char *p;
	register int sub;
{
	register int c, d, not, found = 0;
	const unsigned char *orig_p = p;

	if ((not = (ISMAGIC(*p) && *++p == NOT)))
		p++;
	do {
		c = *p++;
		if (ISMAGIC(c)) {
			c = *p++;
			if ((c & 0x80) && !ISMAGIC(c)) {
				c &= 0x7f;/* extended pattern matching: *+?@! */
				/* XXX the ( char isn't handled as part of [] */
				if (c == ' ') /* simile for @: plain (..) */
					c = '(' /*)*/;
			}
		}
		if (c == '\0')
			/* No closing ] - act as if the opening [ was quoted */
			return sub == '[' ? orig_p : NULL;
		if (ISMAGIC(p[0]) && p[1] == '-'
		    && (!ISMAGIC(p[2]) || p[3] != ']'))
		{
			p += 2; /* MAGIC- */
			d = *p++;
			if (ISMAGIC(d)) {
				d = *p++;
				if ((d & 0x80) && !ISMAGIC(d))
					d &= 0x7f;
			}
			/* POSIX says this is an invalid expression */
			if (c > d)
				return NULL;
		} else
			d = c;
		if (c == sub || (c <= sub && sub <= d))
			found = 1;
	} while (!(ISMAGIC(p[0]) && p[1] == ']'));

	return (found != not) ? p+2 : NULL;
}

/* Look for next ) or | (if match_sep) in *(foo|bar) pattern */
const unsigned char *
pat_scan(p, pe, match_sep)
	const unsigned char *p;
	const unsigned char *pe;
	int match_sep;
{
	int nest = 0;

	for (; p < pe; p++) {
		if (!ISMAGIC(*p))
			continue;
		if ((*++p == /*(*/ ')' && nest-- == 0)
		    || (*p == '|' && match_sep && nest == 0))
			return ++p;
		if ((*p & 0x80) && strchr("*+?@! ", *p & 0x7f))
			nest++;
	}
	return (const unsigned char *) 0;
}


/* -------- qsort.c -------- */

/*
 * quick sort of array of generic pointers to objects.
 */
static void qsort1 ARGS((void **base, void **lim, int (*f)(void *, void *)));

void
qsortp(base, n, f)
	void **base;				/* base address */
	size_t n;				/* elements */
	int (*f) ARGS((void *, void *));	/* compare function */
{
	qsort1(base, base + n, f);
}

#define	swap2(a, b)	{\
	register void *t; t = *(a); *(a) = *(b); *(b) = t;\
}
#define	swap3(a, b, c)	{\
	register void *t; t = *(a); *(a) = *(c); *(c) = *(b); *(b) = t;\
}

static void
qsort1(base, lim, f)
	void **base, **lim;
	int (*f) ARGS((void *, void *));
{
	register void **i, **j;
	register void **lptr, **hptr;
	size_t n;
	int c;

  top:
	n = (lim - base) / 2;
	if (n == 0)
		return;
	hptr = lptr = base+n;
	i = base;
	j = lim - 1;

	for (;;) {
		if (i < lptr) {
			if ((c = (*f)(*i, *lptr)) == 0) {
				lptr --;
				swap2(i, lptr);
				continue;
			}
			if (c < 0) {
				i += 1;
				continue;
			}
		}

	  begin:
		if (j > hptr) {
			if ((c = (*f)(*hptr, *j)) == 0) {
				hptr ++;
				swap2(hptr, j);
				goto begin;
			}
			if (c > 0) {
				if (i == lptr) {
					hptr ++;
					swap3(i, hptr, j);
					i = lptr += 1;
					goto begin;
				}
				swap2(i, j);
				j -= 1;
				i += 1;
				continue;
			}
			j -= 1;
			goto begin;
		}

		if (i == lptr) {
			if (lptr-base >= lim-hptr) {
				qsort1(hptr+1, lim, f);
				lim = lptr;
			} else {
				qsort1(base, lptr, f);
				base = hptr+1;
			}
			goto top;
		}

		lptr -= 1;
		swap3(j, lptr, i);
		j = hptr -= 1;
	}
}

int
xstrcmp(p1, p2)
	void *p1, *p2;
{
	return (strcmp((char *)p1, (char *)p2));
}

/* Initialize a Getopt structure */
void
ksh_getopt_reset(go, flags)
	Getopt *go;
	int flags;
{
	go->optind = 1;
	go->optarg = (char *) 0;
	go->p = 0;
	go->flags = flags;
	go->info = 0;
	go->buf[1] = '\0';
}


/* getopt() used for shell built-in commands, the getopts command, and
 * command line options.
 * A leading ':' in options means don't print errors, instead return '?'
 * or ':' and set go->optarg to the offending option character.
 * If GF_ERROR is set (and option doesn't start with :), errors result in
 * a call to bi_errorf().
 *
 * Non-standard features:
 *	- ';' is like ':' in options, except the argument is optional
 *	  (if it isn't present, optarg is set to 0).
 *	  Used for 'set -o'.
 *	- ',' is like ':' in options, except the argument always immediately
 *	  follows the option character (optarg is set to the null string if
 *	  the option is missing).
 *	  Used for 'read -u2', 'print -u2' and fc -40.
 *	- '#' is like ':' in options, expect that the argument is optional
 *	  and must start with a digit.  If the argument doesn't start with a
 *	  digit, it is assumed to be missing and normal option processing
 *	  continues (optarg is set to 0 if the option is missing).
 *	  Used for 'typeset -LZ4'.
 *	- accepts +c as well as -c IF the GF_PLUSOPT flag is present.  If an
 *	  option starting with + is accepted, the GI_PLUS flag will be set
 *	  in go->info.
 */
int
ksh_getopt(argv, go, options)
	char **argv;
	Getopt *go;
	const char *options;
{
	char c;
	char *o;

	if (go->p == 0 || (c = argv[go->optind - 1][go->p]) == '\0') {
		char *arg = argv[go->optind], flag = arg ? *arg : '\0';

		go->p = 1;
		if (flag == '-' && arg[1] == '-' && arg[2] == '\0') {
			go->optind++;
			go->p = 0;
			go->info |= GI_MINUSMINUS;
			return EOF;
		}
		if (arg == (char *) 0
		    || ((flag != '-' ) /* neither a - nor a + (if + allowed) */
			&& (!(go->flags & GF_PLUSOPT) || flag != '+'))
		    || (c = arg[1]) == '\0')
		{
			go->p = 0;
			return EOF;
		}
		go->optind++;
		go->info &= ~(GI_MINUS|GI_PLUS);
		go->info |= flag == '-' ? GI_MINUS : GI_PLUS;
	}
	go->p++;
	if (c == '?' || c == ':' || c == ';' || c == ',' || c == '#'
	    || !(o = strchr(options, c)))
	{
		if (options[0] == ':') {
			go->buf[0] = c;
			go->optarg = go->buf;
		} else {
			warningf(TRUE, "%s%s-%c: unknown option",
				(go->flags & GF_NONAME) ? "" : argv[0],
				(go->flags & GF_NONAME) ? "" : ": ", c);
			if (go->flags & GF_ERROR)
				bi_errorf(null);
		}
		return '?';
	}
	/* : means argument must be present, may be part of option argument
	 *   or the next argument
	 * ; same as : but argument may be missing
	 * , means argument is part of option argument, and may be null.
	 */
	if (*++o == ':' || *o == ';') {
		if (argv[go->optind - 1][go->p])
			go->optarg = argv[go->optind - 1] + go->p;
		else if (argv[go->optind])
			go->optarg = argv[go->optind++];
		else if (*o == ';')
			go->optarg = (char *) 0;
		else {
			if (options[0] == ':') {
				go->buf[0] = c;
				go->optarg = go->buf;
				return ':';
			}
			warningf(TRUE, "%s%s-`%c' requires argument",
				(go->flags & GF_NONAME) ? "" : argv[0],
				(go->flags & GF_NONAME) ? "" : ": ", c);
			if (go->flags & GF_ERROR)
				bi_errorf(null);
			return '?';
		}
		go->p = 0;
	} else if (*o == ',') {
		/* argument is attached to option character, even if null */
		go->optarg = argv[go->optind - 1] + go->p;
		go->p = 0;
	} else if (*o == '#') {
		/* argument is optional and may be attached or unattached
		 * but must start with a digit.  optarg is set to 0 if the
		 * argument is missing.
		 */
		if (argv[go->optind - 1][go->p]) {
			if (digit(argv[go->optind - 1][go->p])) {
				go->optarg = argv[go->optind - 1] + go->p;
				go->p = 0;
			} else
				go->optarg = (char *) 0;
		} else {
			if (argv[go->optind] && digit(argv[go->optind][0])) {
				go->optarg = argv[go->optind++];
				go->p = 0;
			} else
				go->optarg = (char *) 0;
		}
	}
	return c;
}

/* print variable/alias value using necessary quotes
 * (POSIX says they should be suitable for re-entry...)
 * No trailing newline is printed.
 */
void
print_value_quoted(s)
	const char *s;
{
	const char *p;
	int inquote = 0;

	/* Test if any quotes are needed */
	for (p = s; *p; p++)
		if (ctype(*p, C_QUOTE))
			break;
	if (!*p) {
		shprintf("%s", s);
		return;
	}
	for (p = s; *p; p++) {
		if (*p == '\'') {
			shprintf("'\\'" + 1 - inquote);
			inquote = 0;
		} else {
			if (!inquote) {
				shprintf("'");
				inquote = 1;
			}
			shf_putc(*p, shl_stdout);
		}
	}
	if (inquote)
		shprintf("'");
}

/* Print things in columns and rows - func() is called to format the ith
 * element
 */
void
print_columns(shf, n, func, arg, max_width, prefcol)
	struct shf *shf;
	int n;
	char *(*func) ARGS((void *, int, char *, int));
	void *arg;
	int max_width;
	int prefcol;
{
	char *str = (char *) alloc(max_width + 1, ATEMP);
	int i;
	int r, c;
	int rows, cols;
	int nspace;

	/* max_width + 1 for the space.  Note that no space
	 * is printed after the last column to avoid problems
	 * with terminals that have auto-wrap.
	 */
	cols = x_cols / (max_width + 1);
	if (!cols)
		cols = 1;
	rows = (n + cols - 1) / cols;
	if (prefcol && n && cols > rows) {
		int tmp = rows;

		rows = cols;
		cols = tmp;
		if (rows > n)
			rows = n;
	}

	nspace = (x_cols - max_width * cols) / cols;
	if (nspace <= 0)
		nspace = 1;
	for (r = 0; r < rows; r++) {
		for (c = 0; c < cols; c++) {
			i = c * rows + r;
			if (i < n) {
				shf_fprintf(shf, "%-*s",
					max_width,
					(*func)(arg, i, str, max_width + 1));
				if (c + 1 < cols)
					shf_fprintf(shf, "%*s", nspace, null);
			}
		}
		shf_putchar('\n', shf);
	}
	afree(str, ATEMP);
}

/* Strip any nul bytes from buf - returns new length (nbytes - # of nuls) */
int
strip_nuls(buf, nbytes)
	char *buf;
	int nbytes;
{
	char *dst;

	/* nbytes check because some systems (older freebsd's) have a buggy
	 * memchr()
	 */
	if (nbytes && (dst = memchr(buf, '\0', nbytes))) {
		char *end = buf + nbytes;
		char *p, *q;

		for (p = dst; p < end; p = q) {
			/* skip a block of nulls */
			while (++p < end && *p == '\0')
				;
			/* find end of non-null block */
			if (!(q = memchr(p, '\0', end - p)))
				q = end;
			memmove(dst, p, q - p);
			dst += q - p;
		}
		*dst = '\0';
		return dst - buf;
	}
	return nbytes;
}

/* Copy at most dsize-1 bytes from src to dst, ensuring dst is null terminated.
 * Returns dst.
 */
char *
str_zcpy(dst, src, dsize)
	char *dst;
	const char *src;
	int dsize;
{
	if (dsize > 0) {
		int len = strlen(src);

		if (len >= dsize)
			len = dsize - 1;
		memcpy(dst, src, len);
		dst[len] = '\0';
	}
	return dst;
}

/* Like read(2), but if read fails due to non-blocking flag, resets flag
 * and restarts read.
 */
int
blocking_read(fd, buf, nbytes)
	int fd;
	char *buf;
	int nbytes;
{
	int ret;
	int tried_reset = 0;

	while ((ret = read(fd, buf, nbytes)) < 0) {
		if (!tried_reset && (errno == EAGAIN
#ifdef EWOULDBLOCK
				     || errno == EWOULDBLOCK
#endif /* EWOULDBLOCK */
				    ))
		{
			int oerrno = errno;
			if (reset_nonblock(fd) > 0) {
				tried_reset = 1;
				continue;
			}
			errno = oerrno;
		}
		break;
	}
	return ret;
}

/* Reset the non-blocking flag on the specified file descriptor.
 * Returns -1 if there was an error, 0 if non-blocking wasn't set,
 * 1 if it was.
 */
int
reset_nonblock(fd)
	int fd;
{
	int flags;
	int blocking_flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
		return -1;
	/* With luck, the C compiler will reduce this to a constant */
	blocking_flags = 0;
#ifdef O_NONBLOCK
	blocking_flags |= O_NONBLOCK;
#endif /* O_NONBLOCK */
#ifdef O_NDELAY
	blocking_flags |= O_NDELAY;
#else /* O_NDELAY */
# ifndef O_NONBLOCK
	blocking_flags |= FNDELAY; /* hope this exists... */
# endif /* O_NONBLOCK */
#endif /* O_NDELAY */
	if (!(flags & blocking_flags))
		return 0;
	flags &= ~blocking_flags;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -1;
	return 1;
}


#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#ifndef MAXPATHLEN
# define MAXPATHLEN PATH
#endif /* MAXPATHLEN */

#ifdef HPUX_GETWD_BUG
# include "ksh_dir.h"

/*
 * Work around bug in hpux 10.x C library - getwd/getcwd dump core
 * if current directory is not readable.  Done in macro 'cause code
 * is needed in GETWD and GETCWD cases.
 */
# define HPUX_GETWD_BUG_CODE \
	{ \
	    DIR *d = ksh_opendir("."); \
	    if (!d) \
		return (char *) 0; \
	    closedir(d); \
	}
#else /* HPUX_GETWD_BUG */
# define HPUX_GETWD_BUG_CODE
#endif /* HPUX_GETWD_BUG */

/* Like getcwd(), except bsize is ignored if buf is 0 (MAXPATHLEN is used) */
char *
ksh_get_wd(buf, bsize)
	char *buf;
	int bsize;
{
#ifdef HAVE_GETCWD
	char *b;
	char *ret;

	/* Before memory allocated */
	HPUX_GETWD_BUG_CODE

	/* Assume getcwd() available */
	if (!buf) {
		bsize = MAXPATHLEN;
		b = alloc(MAXPATHLEN + 1, ATEMP);
	} else
		b = buf;

	ret = getcwd(b, bsize);

	if (!buf) {
		if (ret)
			ret = aresize(b, strlen(b) + 1, ATEMP);
		else
			afree(b, ATEMP);
	}

	return ret;
#else /* HAVE_GETCWD */
	extern char *getwd ARGS((char *));
	char *b;
	int len;

	/* Before memory allocated */
	HPUX_GETWD_BUG_CODE

	if (buf && bsize > MAXPATHLEN)
		b = buf;
	else
		b = alloc(MAXPATHLEN + 1, ATEMP);
	if (!getwd(b)) {
		errno = EACCES;
		if (b != buf)
			afree(b, ATEMP);
		return (char *) 0;
	}
	len = strlen(b) + 1;
	if (!buf)
		b = aresize(b, len, ATEMP);
	else if (buf != b) {
		if (len > bsize) {
			errno = ERANGE;
			return (char *) 0;
		}
		memcpy(buf, b, len);
		afree(b, ATEMP);
		b = buf;
	}

	return b;
#endif /* HAVE_GETCWD */
}
