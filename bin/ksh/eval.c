/*	$OpenBSD: eval.c,v 1.3 1997/06/18 22:42:33 kstailey Exp $	*/

/*
 * Expansion - quoting, separation, substitution, globbing
 */

#include "sh.h"
#include <pwd.h>
#include "ksh_dir.h"
#include "ksh_stat.h"

/*
 * string expansion
 *
 * first pass: quoting, IFS separation, ~, ${}, $() and $(()) substitution.
 * second pass: alternation ({,}), filename expansion (*?[]).
 */

/* expansion generator state */
typedef struct Expand {
	/* int  type; */	/* see expand() */
	const char *str;	/* string */
	union {
		const char **strv;/* string[] */
		struct shf *shf;/* file */
	} u;			/* source */
	struct tbl *var;	/* variable in ${var..} */
	short	split;		/* split "$@" / call waitlast $() */
} Expand;

#define	XBASE		0	/* scanning original */
#define	XSUB		1	/* expanding ${} string */
#define	XARGSEP		2	/* ifs0 between "$*" */
#define	XARG		3	/* expanding $*, $@ */
#define	XCOM		4	/* expanding $() */
#define XNULLSUB	5	/* "$@" when $# is 0 (don't generate word) */

/* States used for field splitting */
#define IFS_WORD	0	/* word has chars (or quotes) */
#define IFS_WS		1	/* have seen IFS white-space */
#define IFS_NWS		2	/* have seen IFS non-white-space */

static	int	varsub ARGS((Expand *xp, char *sp, char *word, int *stypep));
static	int	comsub ARGS((Expand *xp, char *cp));
static	char   *trimsub ARGS((char *str, char *pat, int how));
static	void	glob ARGS((char *cp, XPtrV *wp, int markdirs));
static	void	globit ARGS((XString *xs, char **xpp, char *sp, XPtrV *wp,
			     int check));
static char	*maybe_expand_tilde ARGS((char *p, XString *dsp, char **dpp,
					  int isassign));
static	char   *tilde ARGS((char *acp));
static	char   *homedir ARGS((char *name));
#ifdef BRACE_EXPAND
static void	alt_expand ARGS((XPtrV *wp, char *start, char *exp_start,
				 char *end, int fdo));
#endif

/* compile and expand word */
char *
substitute(cp, f)
	const char *cp;
	int f;
{
	struct source *s, *sold;

	sold = source;
	s = pushs(SWSTR, ATEMP);
	s->start = s->str = cp;
	source = s;
	if (yylex(ONEWORD) != LWORD)
		internal_errorf(1, "substitute");
	source = sold;
	afree(s, ATEMP);
	return evalstr(yylval.cp, f);
}

/*
 * expand arg-list
 */
char **
eval(ap, f)
	register char **ap;
	int f;
{
	XPtrV w;

	if (*ap == NULL)
		return ap;
	XPinit(w, 32);
	XPput(w, NULL);		/* space for shell name */
#ifdef	SHARPBANG
	XPput(w, NULL);		/* and space for one arg */
#endif
	while (*ap != NULL)
		expand(*ap++, &w, f);
	XPput(w, NULL);
#ifdef	SHARPBANG
	return (char **) XPclose(w) + 2;
#else
	return (char **) XPclose(w) + 1;
#endif
}

/*
 * expand string
 */
char *
evalstr(cp, f)
	char *cp;
	int f;
{
	XPtrV w;

	XPinit(w, 1);
	expand(cp, &w, f);
	cp = (XPsize(w) == 0) ? null : (char*) *XPptrv(w);
	XPfree(w);
	return cp;
}

/*
 * expand string - return only one component
 * used from iosetup to expand redirection files
 */
char *
evalonestr(cp, f)
	register char *cp;
	int f;
{
	XPtrV w;

	XPinit(w, 1);
	expand(cp, &w, f);
	switch (XPsize(w)) {
	case 0:
		cp = null;
		break;
	case 1:
		cp = (char*) *XPptrv(w);
		break;
	default:
		cp = evalstr(cp, f&~DOGLOB);
		break;
	}
	XPfree(w);
	return cp;
}

/* for nested substitution: ${var:=$var2} */
typedef struct SubType {
	short	stype;		/* [=+-?%#] action after expanded word */
	short	base;		/* begin position of expanded word */
	short	f;		/* saved value of f (DOPAT, etc) */
	struct tbl *var;	/* variable for ${var..} */
	short	quote;		/* saved value of quote (for ${..[%#]..}) */
	struct SubType *prev;	/* old type */
	struct SubType *next;	/* poped type (to avoid re-allocating) */
} SubType;

void
expand(cp, wp, f)
	char *cp;		/* input word */
	register XPtrV *wp;	/* output words */
	int f;			/* DO* flags */
{
	register int UNINITIALIZED(c);
	register int type;	/* expansion type */
	register int quote = 0;	/* quoted */
	XString ds;		/* destination string */
	register char *dp, *sp;	/* dest., source */
	int fdo, word;		/* second pass flags; have word */
	int doblank;		/* field spliting of parameter/command subst */
	Expand x;		/* expansion variables */
	SubType st_head, *st;
	int UNINITIALIZED(newlines); /* For trailing newlines in COMSUB */
	int saw_eq, tilde_ok;
	int make_magic;

	if (cp == NULL)
		internal_errorf(1, "expand(NULL)");
	/* for alias, readonly, set, typeset commands */
	if ((f & DOVACHECK) && is_wdvarassign(cp)) {
		f &= ~(DOVACHECK|DOBLANK|DOGLOB|DOTILDE);
		f |= DOASNTILDE;
	}
	if (Flag(FNOGLOB))
		f &= ~DOGLOB;
	if (Flag(FMARKDIRS))
		f |= DOMARKDIRS;
#ifdef BRACE_EXPAND
	if (Flag(FBRACEEXPAND) && (f & DOGLOB))
		f |= DOBRACE_;
#endif /* BRACE_EXPAND */

	Xinit(ds, dp, 128, ATEMP);	/* init dest. string */
	type = XBASE;
	sp = cp;
	fdo = 0;
	saw_eq = 0;
	tilde_ok = (f & (DOTILDE|DOASNTILDE)) ? 1 : 0; /* must be 1/0 */
	doblank = 0;
	make_magic = 0;
	word = (f&DOBLANK) ? IFS_WS : IFS_WORD;
	st_head.next = NULL;
	st = &st_head;

	while (1) {
		Xcheck(ds, dp);

		switch (type) {
		  case XBASE:	/* original prefixed string */
			c = *sp++;
			switch (c) {
			  case EOS:
				c = 0;
				break;
			  case CHAR:
				c = *sp++;
				break;
			  case QCHAR:
				quote |= 2; /* temporary quote */
				c = *sp++;
				break;
			  case OQUOTE:
				word = IFS_WORD;
				tilde_ok = 0;
				quote = 1;
				continue;
			  case CQUOTE:
				quote = 0;
				continue;
			  case COMSUB:
				tilde_ok = 0;
				if (f & DONTRUNCOMMAND) {
					word = IFS_WORD;
					*dp++ = '$'; *dp++ = '(';
					while (*sp != '\0') {
						Xcheck(ds, dp);
						*dp++ = *sp++;
					}
					*dp++ = ')';
				} else {
					type = comsub(&x, sp);
					if (type == XCOM && (f&DOBLANK))
						doblank++;
					sp = strchr(sp, 0) + 1;
					newlines = 0;
				}
				continue;
			  case EXPRSUB:
				word = IFS_WORD;
				tilde_ok = 0;
				if (f & DONTRUNCOMMAND) {
					*dp++ = '$'; *dp++ = '('; *dp++ = '(';
					while (*sp != '\0') {
						Xcheck(ds, dp);
						*dp++ = *sp++;
					}
					*dp++ = ')'; *dp++ = ')';
				} else {
					struct tbl v;
					char *p;

					v.flag = DEFINED|ISSET|INTEGER;
					v.type = 10; /* not default */
					v.name[0] = '\0';
					v_evaluate(&v, substitute(sp, 0),
						FALSE);
					sp = strchr(sp, 0) + 1;
					for (p = str_val(&v); *p; ) {
						Xcheck(ds, dp);
						*dp++ = *p++;
					}
				}
				continue;
			  case OSUBST: /* ${{#}var{:}[=+-?#%]word} */
			  /* format is:
			   *   OSUBST plain-variable-part \0
			   *     compiled-word-part CSUBST
			   * This is were all syntax checking gets done...
			   */
			  {
				char *varname = sp;
				int stype;

				sp = strchr(sp, '\0') + 1; /* skip variable */
				type = varsub(&x, varname, sp, &stype);
				if (type < 0) {
					char endc;
					char *str, *end;

					end = (char *) wdscan(sp, CSUBST);
					endc = *end;
					*end = EOS;
					str = snptreef(NULL, 64, "%S",
							varname - 1);
					*end = endc;
					errorf("%s: bad substitution", str);
				}
				if (f&DOBLANK)
					doblank++;
				tilde_ok = 0;
				if (type == XBASE) {	/* expand? */
					if (!st->next) {
						SubType *newst;

						newst = (SubType *) alloc(
							sizeof(SubType), ATEMP);
						newst->next = NULL;
						newst->prev = st;
						st->next = newst;
					}
					st = st->next;
					st->stype = stype;
					st->base = Xsavepos(ds, dp);
					st->f = f;
					st->var = x.var;
					st->quote = quote;
					/* skip qualifier(s) */
					if (stype) {
						sp += 2;
						/* :[-+=?] or double [#%] */
						if (stype & 0x80)
							sp += 2;
					}
					switch (stype & 0x7f) {
					  case '#':
					  case '%':
						/* ! DOBLANK,DOBRACE_,DOTILDE */
						f = DOPAT | (f&DONTRUNCOMMAND)
						    | DOTEMP_;
						quote = 0;
						break;
					  case '=':
						/* Enabling tilde expansion
						 * after :'s here is
						 * non-standard ksh, but is
						 * consistent with rules for
						 * other assignments.  Not
						 * sure what POSIX thinks of
						 * this.
						 * Not doing tilde expansion
						 * for integer variables is a
						 * non-POSIX thing - makes
						 * sense though, since ~ is
						 * a arithmetic operator.
						 */
#if !defined(__hppa) || __GNUC__ != 2	/* gcc 2.3.3 on hp-pa dies on this - ifdef goes away as soon as I get a new version of gcc.. */
						if (!(x.var->flag & INTEGER))
							f |= DOASNTILDE|DOTILDE;
						f |= DOTEMP_;
#else
						f |= DOTEMP_|DOASNTILDE|DOTILDE;
#endif
						/* These will be done after the
						 * value has been assigned.
						 */
						f &= ~(DOBLANK|DOGLOB|DOBRACE_);
						tilde_ok = 1;
						break;
					  case '?':
						f &= ~DOBLANK;
						f |= DOTEMP_;
						/* fall through */
					  default:
						/* Enable tilde expansion */
						tilde_ok = 1;
						f |= DOTILDE;
					}
				} else
					/* skip word */
					sp = (char *) wdscan(sp, CSUBST);
				continue;
			  }
			  case CSUBST: /* only get here if expanding word */
				tilde_ok = 0;	/* in case of ${unset:-} */
				*dp = '\0';
				quote = st->quote;
				f = st->f;
				if (f&DOBLANK)
					doblank--;
				switch (st->stype&0x7f) {
				  case '#':
				  case '%':
					dp = Xrestpos(ds, dp, st->base);
					/* Must use st->var since calling
					 * global would break things
					 * like x[i+=1].
					 */
					x.str = trimsub(str_val(st->var),
						dp, st->stype);
					type = XSUB;
					if (f&DOBLANK)
						doblank++;
					st = st->prev;
					continue;
				  case '=':
					if (st->var->flag & RDONLY)
						/* XXX POSIX says this is only
						 * fatal for special builtins
						 */
						errorf("%s: is read only",
							st->var->name);
					/* Restore our position and substitute
					 * the value of st->var (may not be
					 * the assigned value in the presence
					 * of integer/right-adj/etc attributes).
					 */
					dp = Xrestpos(ds, dp, st->base);
					/* Must use st->var since calling
					 * global would cause with things
					 * like x[i+=1] to be evaluated twice.
					 */
					setstr(st->var, debunk(
						(char *) alloc(strlen(dp) + 1,
							ATEMP), dp));
					x.str = str_val(st->var);
					type = XSUB;
					if (f&DOBLANK)
						doblank++;
					st = st->prev;
					continue;
				  case '?':
				    {
					char *s = Xrestpos(ds, dp, st->base);

					errorf("%s: %s", st->var->name,
					    dp == s ? 
					      "parameter null or not set"
					    : (debunk(s, s), s));
				    }
				}
				st = st->prev;
				type = XBASE;
				continue;

			  case OPAT: /* open pattern: *(foo|bar) */
				/* Next char is the type of pattern */
				make_magic = 1;
				c = *sp++ + 0x80;
				break;

			  case SPAT: /* pattern seperator (|) */
				make_magic = 1;
				c = '|';
				break;

			  case CPAT: /* close pattern */
				make_magic = 1;
				c = /*(*/ ')';
				break;
			}
			break;

		  case XNULLSUB:
			/* Special case for "$@" (and "${foo[@]}") - no
			 * word is generated if $# is 0 (unless there is
			 * other stuff inside the quotes).
			 */
			type = XBASE;
			if (f&DOBLANK) {
				doblank--;
				/* not really correct: x=; "$x$@" should
				 * generate a null argument and
				 * set A; "${@:+}" shouldn't.
				 */
				if (dp == Xstring(ds, dp))
					word = IFS_WS;
			}
			continue;

		  case XSUB:
			if ((c = *x.str++) == 0) {
				type = XBASE;
				if (f&DOBLANK)
					doblank--;
				continue;
			}
			break;

		  case XARGSEP:
			type = XARG;
			quote = 1;
		  case XARG:
			if ((c = *x.str++) == '\0') {
				/* force null words to be created so
				 * set -- '' 2 ''; foo "$@" will do
				 * the right thing
				 */
				if (quote && x.split)
					word = IFS_WORD;
				if ((x.str = *x.u.strv++) == NULL) {
					type = XBASE;
					if (f&DOBLANK)
						doblank--;
					continue;
				}
				c = ifs0;
				if (c == 0) {
					if (quote && !x.split)
						continue;
					c = ' ';
				}
				if (quote && x.split) {
					/* terminate word for "$@" */
					type = XARGSEP;
					quote = 0;
				}
			}
			break;

		  case XCOM:
			if (newlines) {		/* Spit out saved nl's */
				c = '\n';
				--newlines;
			} else {
				while ((c = shf_getc(x.u.shf)) == 0 || c == '\n')
				    if (c == '\n')
					    newlines++;	/* Save newlines */
				if (newlines && c != EOF) {
					shf_ungetc(c, x.u.shf);
					c = '\n';
					--newlines;
				}
			}
			if (c == EOF) {
				newlines = 0;
				shf_close(x.u.shf);
				if (x.split)
					subst_exstat = waitlast();
				type = XBASE;
				if (f&DOBLANK)
					doblank--;
				continue;
			}
			break;
		}

		/* check for end of word or IFS separation */
		if (c == 0 || (!quote && (f & DOBLANK) && doblank && !make_magic
			       && ctype(c, C_IFS)))
		{
			/* How words are broken up:
			 *		   |       value of c
			 *	  word	   |	ws	nws	0
			 *	-----------------------------------
			 *	IFS_WORD	w/WS	w/NWS	w
			 *	IFS_WS		-/WS	w/NWS	-
			 *	IFS_NWS		-/NWS	w/NWS	w
			 *   (w means generate a word)
			 * Note that IFS_NWS/0 generates a word (at&t ksh
			 * doesn't do this, but POSIX does).
			 */
			if (word == IFS_WORD
			    || (!ctype(c, C_IFSWS) && (c || word == IFS_NWS)))
			{
				char *p;

				*dp++ = '\0';
				p = Xclose(ds, dp);
#ifdef BRACE_EXPAND
				if (fdo & DOBRACE_)
					/* also does globbing */
					alt_expand(wp, p, p,
						   p + Xlength(ds, (dp - 1)),
						   fdo | (f & DOMARKDIRS));
				else
#endif /* BRACE_EXPAND */
				if (fdo & DOGLOB)
					glob(p, wp, f & DOMARKDIRS);
				else if ((f & DOPAT) || !(fdo & DOMAGIC_))
					XPput(*wp, p);
				else
					XPput(*wp, debunk(p, p));
				fdo = 0;
				saw_eq = 0;
				tilde_ok = (f & (DOTILDE|DOASNTILDE)) ? 1 : 0;
				if (c != 0)
					Xinit(ds, dp, 128, ATEMP);
			}
			if (c == 0)
				return;
			if (word != IFS_NWS)
				word = ctype(c, C_IFSWS) ? IFS_WS : IFS_NWS;
		} else {
			/* age tilde_ok info - ~ code tests second bit */
			tilde_ok <<= 1;
			/* mark any special second pass chars */
			if (!quote)
				switch (c) {
				  case '[':
				  case NOT:
				  case '-':
				  case ']':
					/* For character classes - doesn't hurt
					 * to have magic !,-,]'s outside of
					 * [...] expressions.
					 */
					if (f & (DOPAT | DOGLOB)) {
						fdo |= DOMAGIC_;
						if (c == '[')
							fdo |= f & DOGLOB;
						*dp++ = MAGIC;
					}
					break;
				  case '*':
				  case '?':
					if (f & (DOPAT | DOGLOB)) {
						fdo |= DOMAGIC_ | (f & DOGLOB);
						*dp++ = MAGIC;
					}
					break;
#ifdef BRACE_EXPAND
				  case OBRACE:
				  case ',':
				  case CBRACE:
					if ((f & DOBRACE_) && (c == OBRACE
						|| (fdo & DOBRACE_)))
					{
						fdo |= DOBRACE_|DOMAGIC_;
						*dp++ = MAGIC;
					}
					break;
#endif /* BRACE_EXPAND */
				  case '=':
					/* Note first unquoted = for ~ */
					if (!(f & DOTEMP_) && !saw_eq) {
						saw_eq = 1;
						tilde_ok = 1;
					}
					break;
				  case PATHSEP: /* : */
					/* Note unquoted : for ~ */
					if (!(f & DOTEMP_) && (f & DOASNTILDE))
						tilde_ok = 1;
					break;
				  case '~':
					/* tilde_ok is reset whenever
					 * any of ' " $( $(( ${ } are seen.
					 * Note that tilde_ok must be preserved
					 * through the sequence ${A=a=}~
					 */
					if (type == XBASE
					    && (f & (DOTILDE|DOASNTILDE))
					    && (tilde_ok & 2))
					{
						char *p, *dp_x;

						dp_x = dp;
						p = maybe_expand_tilde(sp,
							&ds, &dp_x,
							f & DOASNTILDE);
						if (p) {
							if (dp != dp_x)
								word = IFS_WORD;
							dp = dp_x;
							sp = p;
							continue;
						}
					}
					break;
				}
			else
				quote &= ~2; /* undo temporary */

			if (make_magic) {
				make_magic = 0;
				fdo |= DOMAGIC_ | (f & DOGLOB);
				*dp++ = MAGIC;
			} else if (ISMAGIC(c)) {
				fdo |= DOMAGIC_;
				*dp++ = MAGIC;
			}
			*dp++ = c; /* save output char */
			word = IFS_WORD;
		}
	}
}

/*
 * Prepare to generate the string returned by ${} substitution.
 */
static int
varsub(xp, sp, word, stypep)
	Expand *xp;
	char *sp;
	char *word;
	int *stypep;
{
	int c;
	int state;	/* next state: XBASE, XARG, XSUB, XNULLSUB */
	int stype;	/* substitution type */
	char *p;
	struct tbl *vp;

	if (sp[0] == '\0')	/* Bad variable name */
		return -1;

	xp->var = NULL;

	/* ${#var}, string length or array size */
	if (sp[0] == '#' && (c = sp[1]) != '\0') {
		int zero_ok = 0;

		/* Can't have any modifiers for ${#...} */
		if (*word != CSUBST)
			return -1;
		sp++;
		/* Check for size of array */
		if ((p=strchr(sp,'[')) && (p[1]=='*'||p[1]=='@') && p[2]==']') {
			c = 0;
			vp = global(arrayname(sp));
			if (vp->flag & (ISSET|ARRAY))
				zero_ok = 1;
			for (; vp; vp = vp->u.array)
				if (vp->flag&ISSET)
					c = vp->index+1;
		} else if (c == '*' || c == '@')
			c = e->loc->argc;
		else {
			p = str_val(global(sp));
			zero_ok = p != null;
			c = strlen(p);
		}
		if (Flag(FNOUNSET) && c == 0 && !zero_ok)
			errorf("%s: parameter not set", sp);
		*stypep = 0; /* unqualified variable/string substitution */
		xp->str = str_save(ulton((unsigned long)c, 10), ATEMP);
		return XSUB;
	}

	/* Check for qualifiers in word part */
	stype = 0;
	c = *word == CHAR ? word[1] : 0;
	if (c == ':') {
		stype = 0x80;
		c = word[2] == CHAR ? word[3] : 0;
	}
	if (ctype(c, C_SUBOP1))
		stype |= c;
	else if (stype)	/* :, :# or :% is not ok */
		return -1;
	else if (ctype(c, C_SUBOP2)) {
		stype = c;
		if (word[2] == CHAR && c == word[3])
			stype |= 0x80;
	}
	if (!stype && *word != CSUBST)
		return -1;
	*stypep = stype;

	c = sp[0];
	if (c == '*' || c == '@') {
		switch (stype & 0x7f) {
		  case '=':	/* can't assign to a vector */
		  case '%':	/* can't trim a vector */
		  case '#':
			return -1;
		}
		if (e->loc->argc == 0) {
			xp->str = null;
			state = c == '@' ? XNULLSUB : XSUB;
		} else {
			xp->u.strv = (const char **) e->loc->argv + 1;
			xp->str = *xp->u.strv++;
			xp->split = c == '@'; /* $@ */
			state = XARG;
		}
	} else {
		if ((p=strchr(sp,'[')) && (p[1]=='*'||p[1]=='@') && p[2]==']') {
			XPtrV wv;

			switch (stype & 0x7f) {
			  case '=':	/* can't assign to a vector */
			  case '%':	/* can't trim a vector */
			  case '#':
				return -1;
			}
			XPinit(wv, 32);
			vp = global(arrayname(sp));
			for (; vp; vp = vp->u.array) {
				if (!(vp->flag&ISSET))
					continue;
				XPput(wv, str_val(vp));
			}
			if (XPsize(wv) == 0) {
				xp->str = null;
				state = p[1] == '@' ? XNULLSUB : XSUB;
				XPfree(wv);
			} else {
				XPput(wv, 0);
				xp->u.strv = (const char **) XPptrv(wv);
				xp->str = *xp->u.strv++;
				xp->split = p[1] == '@'; /* ${foo[@]} */
				state = XARG;
			}
		} else {
			/* Can't assign things like $! or $1 */
			if ((stype & 0x7f) == '='
			    && (ctype(*sp, C_VAR1) || digit(*sp)))
				return -1;
			xp->var = global(sp);
			xp->str = str_val(xp->var);
			state = XSUB;
		}
	}

	c = stype&0x7f;
	/* test the compiler's code generator */
	if (ctype(c, C_SUBOP2) ||
	    (((stype&0x80) ? *xp->str=='\0' : xp->str==null) ? /* undef? */
	     c == '=' || c == '-' || c == '?' : c == '+'))
		state = XBASE;	/* expand word instead of variable value */
	if (Flag(FNOUNSET) && xp->str == null
	    && (ctype(c, C_SUBOP2) || (state != XBASE && c != '+')))
		errorf("%s: parameter not set", sp);
	return state;
}

/*
 * Run the command in $(...) and read its output.
 */
static int
comsub(xp, cp)
	register Expand *xp;
	char *cp;
{
	Source *s, *sold;
	register struct op *t;
	struct shf *shf;

	s = pushs(SSTRING, ATEMP);
	s->start = s->str = cp;
	sold = source;
	t = compile(s);
	source = sold;

	if (t == NULL)
		return XBASE;

	if (t != NULL && t->type == TCOM && /* $(<file) */
	    *t->args == NULL && *t->vars == NULL && t->ioact != NULL) {
		register struct ioword *io = *t->ioact;
		char *name;

		if ((io->flag&IOTYPE) != IOREAD)
			errorf("funny $() command: %s",
				snptreef(NULL, 32, "%R", io));
		shf = shf_open(name = evalstr(io->name, DOTILDE), O_RDONLY, 0,
			SHF_MAPHI|SHF_CLEXEC);
		if (shf == NULL)
			errorf("%s: cannot open $() input", name);
		xp->split = 0;	/* no waitlast() */
	} else {
		int ofd1, pv[2];
		openpipe(pv);
		shf = shf_fdopen(pv[0], SHF_RD, NULL);
		ofd1 = savefd(1, 0);	/* fd 1 may be closed... */
		ksh_dup2(pv[1], 1, FALSE);
		close(pv[1]);
		execute(t, XFORK|XXCOM|XPIPEO);
		restfd(1, ofd1);
		startlast();
		xp->split = 1;	/* waitlast() */
	}

	xp->u.shf = shf;
	return XCOM;
}

/*
 * perform #pattern and %pattern substitution in ${}
 */

static char *
trimsub(str, pat, how)
	register char *str;
	char *pat;
	int how;
{
	register char *end = strchr(str, 0);
	register char *p, c;

	switch (how&0xff) {	/* UCHAR_MAX maybe? */
	  case '#':		/* shortest at begining */
		for (p = str; p <= end; p++) {
			c = *p; *p = '\0';
			if (gmatch(str, pat, FALSE)) {
				*p = c;
				return p;
			}
			*p = c;
		}
		break;
	  case '#'|0x80:	/* longest match at begining */
		for (p = end; p >= str; p--) {
			c = *p; *p = '\0';
			if (gmatch(str, pat, FALSE)) {
				*p = c;
				return p;
			}
			*p = c;
		}
		break;
	  case '%':		/* shortest match at end */
		for (p = end; p >= str; p--) {
			if (gmatch(p, pat, FALSE))
				return str_nsave(str, p - str, ATEMP);
		}
		break;
	  case '%'|0x80:	/* longest match at end */
		for (p = str; p <= end; p++) {
			if (gmatch(p, pat, FALSE))
				return str_nsave(str, p - str, ATEMP);
		}
		break;
	}

	return str;		/* no match, return string */
}

/*
 * glob
 * Name derived from V6's /etc/glob, the program that expanded filenames.
 */

/* XXX cp not const 'cause slashes are temporarily replaced with nulls... */
static void
glob(cp, wp, markdirs)
	char *cp;
	register XPtrV *wp;
	int markdirs;
{
	int oldsize = XPsize(*wp);

	if (glob_str(cp, wp, markdirs) == 0)
		XPput(*wp, debunk(cp, cp));
	else
		qsortp(XPptrv(*wp) + oldsize, (size_t)(XPsize(*wp) - oldsize),
			xstrcmp);
}

#define GF_NONE		0
#define GF_EXCHECK	BIT(0)		/* do existance check on file */
#define GF_GLOBBED	BIT(1)		/* some globbing has been done */
#define GF_MARKDIR	BIT(2)		/* add trailing / to directories */

/* Apply file globbing to cp and store the matching files in wp.  Returns
 * the number of matches found.
 */
int
glob_str(cp, wp, markdirs)
	char *cp;
	XPtrV *wp;
	int markdirs;
{
	int oldsize = XPsize(*wp);
	XString xs;
	char *xp;

	Xinit(xs, xp, 256, ATEMP);
	globit(&xs, &xp, cp, wp, markdirs ? GF_MARKDIR : GF_NONE);
	Xfree(xs, xp);

	return XPsize(*wp) - oldsize;
}

static void
globit(xs, xpp, sp, wp, check)
	XString *xs;		/* dest string */
	char **xpp;		/* ptr to dest end */
	char *sp;		/* source path */
	register XPtrV *wp;	/* output list */
	int check;		/* GF_* flags */
{
	register char *np;	/* next source component */
	char *xp = *xpp;
	char *se;
	char odirsep;

	/* This to allow long expansions to be interrupted */
	intrcheck();

	if (sp == NULL) {	/* end of source path */
		/* We only need to check if the file exists if a pattern
		 * is followed by a non-pattern (eg, foo*x/bar; no check
		 * is needed for foo* since the match must exist) or if
		 * any patterns were expanded and the markdirs option is set.
		 * Symlinks make things a bit tricky...
		 */
		if ((check & GF_EXCHECK)
		    || ((check & GF_MARKDIR) && (check & GF_GLOBBED)))
		{
#define stat_check()	(stat_done ? stat_done : \
			    (stat_done = stat(Xstring(*xs, xp), &statb) < 0 \
				? -1 : 1))
			struct stat lstatb, statb;
			int stat_done = 0;	 /* -1: failed, 1 ok */

			if (lstat(Xstring(*xs, xp), &lstatb) < 0)
				return;
			/* special case for systems which strip trailing
			 * slashes from regular files (eg, /etc/passwd/).
			 * SunOS 4.1.3 does this...
			 */
			if ((check & GF_EXCHECK) && xp > Xstring(*xs, xp)
			    && ISDIRSEP(xp[-1]) && !S_ISDIR(lstatb.st_mode)
#ifdef S_ISLNK
			    && (!S_ISLNK(lstatb.st_mode)
				|| stat_check() < 0
				|| !S_ISDIR(statb.st_mode))
#endif /* S_ISLNK */
				)
				return;
			/* Possibly tack on a trailing / if there isn't already
			 * one and if the file is a directory or a symlink to a
			 * directory
			 */
			if (((check & GF_MARKDIR) && (check & GF_GLOBBED))
			    && xp > Xstring(*xs, xp) && !ISDIRSEP(xp[-1])
			    && (S_ISDIR(lstatb.st_mode)
#ifdef S_ISLNK
				|| (S_ISLNK(lstatb.st_mode)
				    && stat_check() > 0
				    && S_ISDIR(statb.st_mode))
#endif /* S_ISLNK */
				    ))
			{
				*xp++ = DIRSEP;
				*xp = '\0';
			}
		}
#ifdef OS2 /* Done this way to avoid bug in gcc 2.7.2... */
    /* Ugly kludge required for command
     * completion - see how search_access()
     * is implemented for OS/2...
     */
# define KLUDGE_VAL	4
#else /* OS2 */
# define KLUDGE_VAL	0
#endif /* OS2 */
		XPput(*wp, str_nsave(Xstring(*xs, xp), Xlength(*xs, xp)
			+ KLUDGE_VAL, ATEMP));
		return;
	}

	if (xp > Xstring(*xs, xp))
		*xp++ = DIRSEP;
	while (ISDIRSEP(*sp)) {
		Xcheck(*xs, xp);
		*xp++ = *sp++;
	}
	np = ksh_strchr_dirsep(sp);
	if (np != NULL) {
		se = np;
		odirsep = *np;	/* don't assume DIRSEP, can be multiple kinds */
		*np++ = '\0';
	} else {
		odirsep = '\0'; /* keep gcc quiet */
		se = sp + strlen(sp);
	}


	/* Check if sp needs globbing - done to avoid pattern checks for strings
	 * containing MAGIC characters, open ['s without the matching close ],
	 * etc. (otherwise opendir() will be called which may fail because the
	 * directory isn't readable - if no globbing is needed, only execute
	 * permission should be required (as per POSIX)).
	 */
	if (!has_globbing(sp, se)) {
		XcheckN(*xs, xp, se - sp + 1);
		debunk(xp, sp);
		xp += strlen(xp);
		*xpp = xp;
		globit(xs, xpp, np, wp, check);
	} else {
		DIR *dirp;
		struct dirent *d;
		char *name;
		int len;
		int prefix_len;

		/* xp = *xpp;	   copy_non_glob() may have re-alloc'd xs */
		*xp = '\0';
		prefix_len = Xlength(*xs, xp);
		dirp = ksh_opendir(prefix_len ? Xstring(*xs, xp) : ".");
		if (dirp == NULL)
			goto Nodir;
		while ((d = readdir(dirp)) != NULL) {
			name = d->d_name;
			if (name[0] == '.' &&
			    (name[1] == 0 || (name[1] == '.' && name[2] == 0)))
				continue; /* always ignore . and .. */
			if ((*name == '.' && *sp != '.')
			    || !gmatch(name, sp, TRUE))
				continue;

			len = NLENGTH(d) + 1;
			XcheckN(*xs, xp, len);
			memcpy(xp, name, len);
			*xpp = xp + len - 1;
			globit(xs, xpp, np, wp,
				(check & GF_MARKDIR) | GF_GLOBBED
				| (np ? GF_EXCHECK : GF_NONE));
			xp = Xstring(*xs, xp) + prefix_len;
		}
		closedir(dirp);
	  Nodir:;
	}

	if (np != NULL)
		*--np = odirsep;
}

#if 0
/* Check if p contains something that needs globbing; if it does, 0 is
 * returned; if not, p is copied into xs/xp after stripping any MAGICs
 */
static int	copy_non_glob ARGS((XString *xs, char **xpp, char *p));
static int
copy_non_glob(xs, xpp, p)
	XString *xs;
	char **xpp;
	char *p;
{
	char *xp;
	int len = strlen(p);

	XcheckN(*xs, *xpp, len);
	xp = *xpp;
	for (; *p; p++) {
		if (ISMAGIC(*p)) {
			int c = *++p;

			if (c == '*' || c == '?')
				return 0;
			if (*p == '[') {
				char *q = p + 1;

				if (ISMAGIC(*q) && q[1] == NOT)
					q += 2;
				if (ISMAGIC(*q) && q[1] == ']')
					q += 2;
				for (; *q; q++)
					if (ISMAGIC(*q) && *++q == ']')
						return 0;
				/* pass a literal [ through */
			}
			/* must be a MAGIC-MAGIC, or MAGIC-!, MAGIC--, etc. */
		}
		*xp++ = *p;
	}
	*xp = '\0';
	*xpp = xp;
	return 1;
}
#endif /* 0 */

/* remove MAGIC from string */
char *
debunk(dp, sp)
	char *dp;
	const char *sp;
{
	char *d, *s;

	if ((s = strchr(sp, MAGIC))) {
		memcpy(dp, sp, s - sp);
		for (d = dp + (s - sp); *s; s++)
			if (!ISMAGIC(*s) || !(*++s & 0x80)
			    || !strchr("*+?@!", *s & 0x7f))
				*d++ = *s;
			else {
				/* extended pattern operators: *+?@! */
				*d++ = *s & 0x7f;
				*d++ = '(';
			}
		*d = '\0';
	} else if (dp != sp)
		strcpy(dp, sp);
	return dp;
}

/* Check if p is an unquoted name, possibly followed by a / or :.  If so
 * puts the expanded version in *dcp,dp and returns a pointer in p just
 * past the name, otherwise returns 0.
 */
static char *
maybe_expand_tilde(p, dsp, dpp, isassign)
	char *p;
	XString *dsp;
	char **dpp;
	int isassign;
{
	XString ts;
	char *dp = *dpp;
	char *tp, *r;

	Xinit(ts, tp, 16, ATEMP);
	/* : only for DOASNTILDE form */
	while (p[0] == CHAR && !ISDIRSEP(p[1])
	       && (!isassign || p[1] != PATHSEP))
	{
		Xcheck(ts, tp);
		*tp++ = p[1];
		p += 2;
	}
	*tp = '\0';
	r = (p[0] == EOS || p[0] == CHAR || p[0] == CSUBST) ? tilde(Xstring(ts, tp)) : NULL;
	Xfree(ts, tp);
	if (r) {
		while (*r) {
			Xcheck(*dsp, dp);
			if (ISMAGIC(*r))
				*dp++ = MAGIC;
			*dp++ = *r++;
		}
		*dpp = dp;
		r = p;
	}
	return r;
}

/*
 * tilde expansion
 *
 * based on a version by Arnold Robbins
 */

static char *
tilde(cp)
	char *cp;
{
	char *dp;

	if (cp[0] == '\0')
		dp = str_val(global("HOME"));
	else if (cp[0] == '+' && cp[1] == '\0')
		dp = str_val(global("PWD"));
	else if (cp[0] == '-' && cp[1] == '\0')
		dp = str_val(global("OLDPWD"));
	else
		dp = homedir(cp);
	/* If HOME, PWD or OLDPWD are not set, don't expand ~ */
	if (dp == null)
		dp = NULL;
	return dp;
}

/*
 * map userid to user's home directory.
 * note that 4.3's getpw adds more than 6K to the shell,
 * and the YP version probably adds much more.
 * we might consider our own version of getpwnam() to keep the size down.
 */

static char *
homedir(name)
	char *name;
{
	register struct tbl *ap;

	ap = tenter(&homedirs, name, hash(name));
	if (!(ap->flag & ISSET)) {
#ifdef OS2
		/* No usernames in OS2 - punt */
		return NULL;
#else /* OS2 */
		struct passwd *pw;

		pw = getpwnam(name);
		if (pw == NULL)
			return NULL;
		ap->val.s = str_save(pw->pw_dir, APERM);
		ap->flag |= DEFINED|ISSET|ALLOC;
#endif /* OS2 */
	}
	return ap->val.s;
}

#ifdef BRACE_EXPAND
static void
alt_expand(wp, start, exp_start, end, fdo)
	XPtrV *wp;
	char *start, *exp_start;
	char *end;
	int fdo;
{
	int UNINITIALIZED(count);
	char *brace_start, *brace_end, *UNINITIALIZED(comma);
	char *field_start;
	char *p;

	/* search for open brace */
	for (p = exp_start; (p = strchr(p, MAGIC)) && p[1] != OBRACE; p += 2)
		;
	brace_start = p;

	/* find matching close brace, if any */
	if (p) {
		comma = NULL;
		count = 1;
		for (p += 2; *p && count; p++) {
			if (ISMAGIC(*p)) {
				if (*++p == OBRACE)
					count++;
				else if (*p == CBRACE)
					--count;
				else if (*p == ',' && count == 1)
					comma = p;
			}
		}
	}
	/* no valid expansions... */
	if (!p || count != 0) {
		/* Note that given a{{b,c} we do not expand anything (this is
		 * what at&t ksh does.  This may be changed to do the {b,c}
		 * expansion. }
		 */
		if (fdo & DOGLOB)
			glob(start, wp, fdo & DOMARKDIRS);
		else
			XPput(*wp, debunk(start, start));
		return;
	}
	brace_end = p;
	if (!comma) {
		alt_expand(wp, start, brace_end, end, fdo);
		return;
	}

	/* expand expression */
	field_start = brace_start + 2;
	count = 1;
	for (p = brace_start + 2; p != brace_end; p++) {
		if (ISMAGIC(*p)) {
			if (*++p == OBRACE)
				count++;
			else if ((*p == CBRACE && --count == 0)
				 || (*p == ',' && count == 1))
			{
				char *new;
				int l1, l2, l3;

				l1 = brace_start - start;
				l2 = (p - 1) - field_start;
				l3 = end - brace_end;
				new = (char *) alloc(l1 + l2 + l3 + 1, ATEMP);
				memcpy(new, start, l1);
				memcpy(new + l1, field_start, l2);
				memcpy(new + l1 + l2, brace_end, l3);
				new[l1 + l2 + l3] = '\0';
				alt_expand(wp, new, new + l1,
					   new + l1 + l2 + l3, fdo);
				field_start = p + 1;
			}
		}
	}
	return;
}
#endif /* BRACE_EXPAND */
